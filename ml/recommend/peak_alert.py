"""Peak-load detection for recommendation integration."""

from __future__ import annotations

import logging
import os
from pathlib import Path
from typing import Any, Iterable, Optional

import pandas as pd


LOGGER = logging.getLogger(__name__)

PREDICTION_ENV = "CHARGING_PREDICTION_PATH"
DEFAULT_PEAK_THRESHOLD = 80.0


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
            LOGGER.warning("Configured prediction file does not exist: %s", path)
            return pd.DataFrame()
        return pd.read_csv(path)
    if isinstance(data, Iterable) and not isinstance(data, (bytes, str, dict)):
        return pd.DataFrame(list(data))
    if isinstance(data, dict):
        return pd.DataFrame([data])
    raise TypeError("prediction data must be a pandas DataFrame, CSV path, dict, or iterable of dicts")


def _first_column(frame: pd.DataFrame, candidates: Iterable[str]) -> Optional[str]:
    for column in candidates:
        if column in frame.columns:
            return column
    return None


def _alert_level(value: float, threshold: float) -> str:
    if threshold <= 0:
        return "high"
    ratio = value / threshold
    if ratio >= 1.25:
        return "critical"
    if ratio >= 1.0:
        return "high"
    if ratio >= 0.85:
        return "medium"
    return "low"


def detect_peak_station(
    predictions: Any = None,
    threshold: float = DEFAULT_PEAK_THRESHOLD,
    station_thresholds: Optional[dict[str, float]] = None,
    include_medium: bool = False,
    as_records: bool = False,
) -> pd.DataFrame | list[dict[str, Any]]:
    """Identify station forecast rows that exceed the configured peak threshold."""
    frame = _read_optional_frame(predictions, PREDICTION_ENV)
    columns = ["station_id", "peak_time", "predicted_load", "alert_level"]
    if frame.empty:
        empty = pd.DataFrame(columns=columns)
        return empty.to_dict("records") if as_records else empty
    if "station_id" not in frame.columns:
        raise ValueError("prediction data must include station_id")
    load_column = _first_column(frame, ["predicted_load", "predicted_load_kwh", "prediction", "load"])
    if load_column is None:
        raise ValueError("prediction data must include predicted_load or predicted_load_kwh")

    result = frame.copy()
    result["predicted_load"] = pd.to_numeric(result[load_column], errors="coerce").fillna(0.0)
    result["peak_time"] = result["datetime"] if "datetime" in result.columns else None
    if station_thresholds:
        result["_threshold"] = result["station_id"].astype(str).map(station_thresholds).fillna(float(threshold))
    else:
        result["_threshold"] = float(threshold)
    result["alert_level"] = [
        _alert_level(value, limit)
        for value, limit in zip(result["predicted_load"], result["_threshold"])
    ]
    if include_medium:
        result = result[result["alert_level"].isin(["medium", "high", "critical"])]
    else:
        result = result[result["predicted_load"] >= result["_threshold"]]
    result = result.sort_values(["predicted_load", "station_id"], ascending=[False, True], kind="mergesort")
    result = result[columns].reset_index(drop=True)
    return result.to_dict("records") if as_records else result


def flag_upcoming_peak_stations(*args: Any, **kwargs: Any):
    """Backward-compatible name for task #118."""
    return detect_peak_station(*args, **kwargs)


def load_threshold_alert(
    predictions: Any = None,
    threshold: float = DEFAULT_PEAK_THRESHOLD,
    as_records: bool = True,
) -> list[dict[str, Any]] | pd.DataFrame:
    """Generate high-load alert records for operations dashboards."""
    return detect_peak_station(predictions=predictions, threshold=threshold, as_records=as_records)
