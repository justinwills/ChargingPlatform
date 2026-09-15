"""Identify future high-load periods from forecast results."""

from __future__ import annotations

import argparse
from pathlib import Path
from typing import Any, Iterable, Optional

import pandas as pd


def _as_frame(predictions: Any) -> pd.DataFrame:
    if isinstance(predictions, (str, Path)):
        frame = pd.read_csv(predictions)
    elif isinstance(predictions, pd.DataFrame):
        frame = predictions.copy()
    elif isinstance(predictions, (list, tuple)):
        frame = pd.DataFrame(predictions)
    elif predictions.__class__.__module__.startswith("pyspark.sql"):
        frame = predictions.toPandas()
    else:
        raise TypeError("predictions must be a pandas/Spark DataFrame, records, or CSV path")
    aliases = {"prediction": "predicted_load_kwh", "load_prediction": "predicted_load_kwh", "horizon": "horizon_hour"}
    for source, target in aliases.items():
        if target not in frame.columns and source in frame.columns:
            frame[target] = frame[source]
    required = {"datetime", "predicted_load_kwh"}
    missing = sorted(required - set(frame.columns))
    if missing:
        raise ValueError("Forecast data is missing columns: " + ", ".join(missing))
    frame["datetime"] = pd.to_datetime(frame["datetime"], errors="coerce")
    frame["predicted_load_kwh"] = pd.to_numeric(frame["predicted_load_kwh"], errors="coerce")
    frame = frame.dropna(subset=["datetime", "predicted_load_kwh"])
    if "station_id" not in frame.columns:
        frame["station_id"] = "global"
    return frame


def identify_peak_periods(
    predictions: Any,
    top_n: int = 3,
    station_id: Optional[str] = None,
    threshold: Optional[float] = None,
    threshold_quantile: float = 0.9,
    aggregate: str = "sum",
    return_all: bool = False,
    as_records: bool = False,
):
    """Rank future time slots by predicted charging demand.

    Loads from all stations are summed (or averaged) at each timestamp.  The
    returned rows are ordered from highest to lowest predicted load and include
    a ``is_peak`` flag based on an absolute threshold or the requested
    quantile.  Set ``return_all=True`` to retain every ranked time slot.
    """
    if top_n <= 0:
        raise ValueError("top_n must be positive")
    if not 0 <= threshold_quantile <= 1:
        raise ValueError("threshold_quantile must be between 0 and 1")
    frame = _as_frame(predictions)
    if station_id is not None:
        frame = frame[frame["station_id"].astype(str) == str(station_id)]
    if frame.empty:
        raise ValueError("No forecast rows available for the requested station")
    if aggregate not in {"sum", "mean", "max"}:
        raise ValueError("aggregate must be sum, mean, or max")
    grouped = frame.groupby("datetime", as_index=False).agg(
        predicted_load_kwh=("predicted_load_kwh", aggregate),
        station_count=("station_id", "nunique"),
    )
    grouped["hour"] = grouped["datetime"].dt.hour
    grouped["peak_threshold_kwh"] = float(threshold) if threshold is not None else grouped["predicted_load_kwh"].quantile(threshold_quantile)
    grouped["is_peak"] = grouped["predicted_load_kwh"] >= grouped["peak_threshold_kwh"]
    grouped = grouped.sort_values(["predicted_load_kwh", "datetime"], ascending=[False, True], kind="mergesort").reset_index(drop=True)
    grouped["rank"] = grouped.index + 1
    result = grouped if return_all else grouped.head(top_n)
    result = result[["rank", "datetime", "hour", "predicted_load_kwh", "station_count", "peak_threshold_kwh", "is_peak"]]
    return result.to_dict("records") if as_records else result.reset_index(drop=True)


def identify_peak_periods_per_station(predictions: Any, top_n: int = 3, **kwargs) -> dict[str, pd.DataFrame]:
    """Return top predicted periods separately for every station."""
    frame = _as_frame(predictions)
    return {
        str(station): identify_peak_periods(group, top_n=top_n, **kwargs)
        for station, group in frame.groupby("station_id", sort=True)
    }


def _parse_args(argv: Optional[Iterable[str]] = None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, help="forecast CSV path")
    parser.add_argument("--output", default=None)
    parser.add_argument("--top-n", type=int, default=3)
    parser.add_argument("--station-id", default=None)
    return parser.parse_args(argv)


def main(argv: Optional[Iterable[str]] = None):
    args = _parse_args(argv)
    result = identify_peak_periods(args.input, top_n=args.top_n, station_id=args.station_id)
    if args.output:
        result.to_csv(args.output, index=False)
        print(f"Peak periods written to {args.output}")
    else:
        print(result.to_json(orient="records", date_format="iso"))


if __name__ == "__main__":
    main()
