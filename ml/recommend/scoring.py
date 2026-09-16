"""Station recommendation scoring.

These helpers consume forecast output from ``ml.forecast`` and station/history
tables.  They intentionally do not train or load a model; prediction remains a
separate concern.
"""

from __future__ import annotations

import logging
import os
from pathlib import Path
from typing import Any, Iterable, Optional

import pandas as pd


LOGGER = logging.getLogger(__name__)

PREDICTION_ENV = "CHARGING_PREDICTION_PATH"
STATION_ENV = "CHARGING_STATION_PATH"
HISTORY_ENV = "CHARGING_HISTORY_PATH"

NORMAL_STATUS_VALUES = {"normal", "open", "online", "active", "available", "1", "true"}


def _read_optional_frame(data: Any, env_name: Optional[str] = None) -> pd.DataFrame:
    if data is None and env_name:
        configured = os.getenv(env_name)
        if configured:
            data = configured
    if data is None:
        return pd.DataFrame()
    if isinstance(data, pd.DataFrame):
        return data.copy()
    if isinstance(data, (str, Path)):
        path = Path(data)
        if not path.exists():
            LOGGER.warning("Configured data file does not exist: %s", path)
            return pd.DataFrame()
        return pd.read_csv(path)
    if isinstance(data, Iterable) and not isinstance(data, (bytes, str, dict)):
        return pd.DataFrame(list(data))
    if isinstance(data, dict):
        return pd.DataFrame([data])
    raise TypeError("data must be a pandas DataFrame, CSV path, dict, or iterable of dicts")


def _first_column(frame: pd.DataFrame, candidates: Iterable[str]) -> Optional[str]:
    for column in candidates:
        if column in frame.columns:
            return column
    return None


def _numeric(frame: pd.DataFrame, column: Optional[str], default: float = 0.0) -> pd.Series:
    if column is None:
        return pd.Series(default, index=frame.index, dtype="float64")
    return pd.to_numeric(frame[column], errors="coerce").fillna(default).astype("float64")


def _minmax(series: pd.Series, higher_is_better: bool = True) -> pd.Series:
    series = pd.to_numeric(series, errors="coerce").fillna(0.0).astype("float64")
    minimum = float(series.min()) if not series.empty else 0.0
    maximum = float(series.max()) if not series.empty else 0.0
    if maximum == minimum:
        score = pd.Series(1.0, index=series.index, dtype="float64")
    else:
        score = (series - minimum) / (maximum - minimum)
    if not higher_is_better:
        score = 1.0 - score
    return score.clip(0.0, 1.0)


def _status_score(frame: pd.DataFrame) -> pd.Series:
    column = _first_column(frame, ["station_status", "status", "station_status_code"])
    if column is None:
        return pd.Series(1.0, index=frame.index, dtype="float64")
    values = frame[column]
    if pd.api.types.is_numeric_dtype(values):
        return pd.to_numeric(values, errors="coerce").fillna(1.0).clip(0.0, 1.0)
    return values.astype(str).str.strip().str.lower().isin(NORMAL_STATUS_VALUES).astype("float64")


def _aggregate_predictions(predictions: pd.DataFrame) -> pd.DataFrame:
    if predictions.empty:
        return pd.DataFrame(columns=["station_id", "predicted_load", "peak_time"])
    if "station_id" not in predictions.columns:
        raise ValueError("prediction data must include station_id")
    load_column = _first_column(predictions, ["predicted_load", "predicted_load_kwh", "prediction", "load"])
    if load_column is None:
        raise ValueError("prediction data must include predicted_load or predicted_load_kwh")
    frame = predictions.copy()
    frame["_predicted_load"] = pd.to_numeric(frame[load_column], errors="coerce").fillna(0.0)
    grouped = frame.groupby("station_id", as_index=False).agg(predicted_load=("_predicted_load", "mean"))
    if "datetime" in frame.columns:
        idx = frame.groupby("station_id")["_predicted_load"].idxmax()
        peak_times = frame.loc[idx, ["station_id", "datetime"]].rename(columns={"datetime": "peak_time"})
        grouped = grouped.merge(peak_times, on="station_id", how="left")
    else:
        grouped["peak_time"] = None
    return grouped


def _aggregate_history(history: pd.DataFrame) -> pd.DataFrame:
    if history.empty:
        return pd.DataFrame(columns=["station_id", "historical_volume", "historical_frequency"])
    if "station_id" not in history.columns:
        raise ValueError("history data must include station_id")
    volume_column = _first_column(history, ["energy_kwh", "historical_volume", "charge_volume", "charging_volume"])
    frame = history.copy()
    frame["_volume"] = _numeric(frame, volume_column, 0.0)
    return frame.groupby("station_id", as_index=False).agg(
        historical_volume=("_volume", "sum"),
        historical_frequency=("_volume", "size"),
    )


def _prepare_scoring_frame(
    predictions: Any = None,
    stations: Any = None,
    history: Any = None,
) -> pd.DataFrame:
    prediction_frame = _aggregate_predictions(_read_optional_frame(predictions, PREDICTION_ENV))
    station_frame = _read_optional_frame(stations, STATION_ENV)
    history_frame = _aggregate_history(_read_optional_frame(history, HISTORY_ENV))

    if station_frame.empty and prediction_frame.empty:
        LOGGER.warning("No station or prediction data available for recommendation scoring")
        return pd.DataFrame()
    if not station_frame.empty and "station_id" not in station_frame.columns:
        raise ValueError("station data must include station_id")

    if station_frame.empty:
        frame = prediction_frame.copy()
    elif prediction_frame.empty:
        frame = station_frame.copy()
        frame["predicted_load"] = 0.0
        frame["peak_time"] = None
    else:
        frame = station_frame.merge(prediction_frame, on="station_id", how="left")
        frame["predicted_load"] = frame["predicted_load"].fillna(0.0)
    if not history_frame.empty:
        frame = frame.merge(history_frame, on="station_id", how="left")
    for column in ("historical_volume", "historical_frequency"):
        if column not in frame.columns:
            frame[column] = 0.0
        frame[column] = pd.to_numeric(frame[column], errors="coerce").fillna(0.0)
    return frame


def calculate_station_score(
    predictions: Any = None,
    stations: Any = None,
    history: Any = None,
    as_records: bool = False,
) -> pd.DataFrame | list[dict[str, Any]]:
    """Calculate a 0-100 recommendation score per station.

    Higher scores mean lower expected congestion, better available capacity,
    normal station status, and lower charging cost.  Historical volume and
    frequency are mild positive signals so frequently used stations are not
    unfairly hidden when their predicted load is still manageable.
    """
    frame = _prepare_scoring_frame(predictions=predictions, stations=stations, history=history)
    if frame.empty:
        columns = ["station_id", "score", "reason"]
        empty = pd.DataFrame(columns=columns)
        return empty.to_dict("records") if as_records else empty

    device_count = _numeric(frame, _first_column(frame, ["device_count", "charger_count", "total_devices"]), 0.0)
    available = _numeric(frame, _first_column(frame, ["available_chargers", "available_devices", "available_count"]), -1.0)
    available = available.where(available >= 0.0, device_count)
    predicted_load = _numeric(frame, "predicted_load", 0.0)
    congestion_column = _first_column(frame, ["predicted_congestion", "predicted_congestion_level", "congestion_level"])
    if congestion_column:
        congestion_raw = frame[congestion_column]
        if pd.api.types.is_numeric_dtype(congestion_raw):
            congestion = pd.to_numeric(congestion_raw, errors="coerce").fillna(0.0)
        else:
            mapping = {"low": 0.2, "medium": 0.5, "mid": 0.5, "high": 0.8, "critical": 1.0}
            congestion = congestion_raw.astype(str).str.strip().str.lower().map(mapping).fillna(0.0)
    else:
        congestion = predicted_load / device_count.replace(0.0, pd.NA)
        congestion = congestion.fillna(predicted_load)

    electricity_price = _numeric(frame, _first_column(frame, ["electricity_price_cny_kwh", "electricity_price", "price"]), 0.0)
    service_fee = _numeric(frame, _first_column(frame, ["service_fee_cny_kwh", "service_fee"]), 0.0)
    total_price = electricity_price + service_fee

    score = (
        0.28 * _minmax(predicted_load, higher_is_better=False)
        + 0.20 * _minmax(congestion, higher_is_better=False)
        + 0.18 * _minmax(available, higher_is_better=True)
        + 0.14 * _status_score(frame)
        + 0.08 * _minmax(total_price, higher_is_better=False)
        + 0.06 * _minmax(frame["historical_volume"], higher_is_better=True)
        + 0.06 * _minmax(frame["historical_frequency"], higher_is_better=True)
    )
    result = frame.copy()
    result["score"] = (score * 100.0).round(2)
    result["reason"] = [
        _score_reason(load, avail, status)
        for load, avail, status in zip(predicted_load, available, _status_score(frame))
    ]
    result = result.sort_values(["score", "station_id"], ascending=[False, True], kind="mergesort")
    preferred = ["station_id", "score", "reason", "predicted_load", "peak_time"]
    columns = [column for column in preferred if column in result.columns]
    result = result[columns]
    return result.to_dict("records") if as_records else result


def _score_reason(predicted_load: float, available: float, status_score: float) -> str:
    if status_score < 1.0:
        return "Station is not in normal operating status"
    if available > 0 and predicted_load <= available * 4:
        return "Low predicted congestion"
    if available > 0:
        return "Available chargers remain despite forecast load"
    return "Ranked by prediction and station capacity"


def station_recommend_score(*args: Any, **kwargs: Any):
    """Backward-compatible name for task #116."""
    return calculate_station_score(*args, **kwargs)


def recommend_low_load_station(
    predictions: Any = None,
    stations: Any = None,
    history: Any = None,
    top_n: int = 5,
) -> list[dict[str, Any]]:
    """Recommend stations with lower forecast pressure and normal status."""
    scored = calculate_station_score(predictions=predictions, stations=stations, history=history)
    if scored.empty:
        return []
    frame = _prepare_scoring_frame(predictions=predictions, stations=stations, history=history)
    status = _status_score(frame)
    normal_ids = set(frame.loc[status >= 1.0, "station_id"].astype(str))
    scored = scored[scored["station_id"].astype(str).isin(normal_ids)]
    if top_n is not None:
        scored = scored.head(max(0, int(top_n)))
    return [
        {
            "station_id": str(row.station_id),
            "score": float(row.score),
            "reason": str(row.reason),
            "predicted_load": _clean_load(getattr(row, "predicted_load", 0.0)),
            "peak_time": _clean_peak_time(getattr(row, "peak_time", None)),
        }
        for row in scored.itertuples(index=False)
    ]


def _clean_load(value):
    try:
        value = float(value)
    except (TypeError, ValueError):
        return 0.0
    if value != value:  # NaN
        return 0.0
    return round(value, 6)


def _clean_peak_time(value):
    import math

    if value is None:
        return None
    if isinstance(value, float) and (math.isnan(value) or math.isinf(value)):
        return None
    return value


def recommend_low_load_stations(*args: Any, **kwargs: Any) -> list[dict[str, Any]]:
    """Backward-compatible plural name for task #117."""
    kwargs.pop("user_lat", None)
    kwargs.pop("user_lng", None)
    return recommend_low_load_station(*args, **kwargs)
