"""Prediction service backing the Web dashboard forecast API.

[Task #122] 智能推荐与系统集成 / 后端接口 / 预测接口
Owner: 邱辰笙

Loads the saved Spark PipelineModel (or pickle) together with the latest
station-hour history, produces 1h / 6h / 24h recursive forecasts through
``ml/forecast.py``, and returns chart-friendly JSON records.

The service degrades gracefully: when no trained model is available it falls
back to the persistence forecast already provided by ``forecast_load``, so the
API keeps answering before a model has been trained.  Spark is only started
when a Spark PipelineModel directory or a Spark-written history directory is
used; a plain CSV history and pickle model keep the service runnable on a
laptop that has neither PySpark nor a HDFS NameNode.
"""

from __future__ import annotations

import os
import pickle
import sys
from datetime import date, datetime, timezone
from pathlib import Path
from typing import Any, Optional

import numpy as np
import pandas as pd

PROJECT_ROOT = Path(__file__).resolve().parents[1]
if __package__ in (None, ""):
    sys.path.insert(0, str(PROJECT_ROOT))

from ml.forecast import forecast_1h, forecast_6h, forecast_24h

DEFAULT_MODEL_PATH = str(PROJECT_ROOT / "ml" / "model_registry" / "random_forest_10trees_depth6")
DEFAULT_HISTORY_PATH = str(PROJECT_ROOT / "data" / "processed" / "ml_training_dataset.csv")
DEFAULT_INFERENCE_DIR = PROJECT_ROOT / "data" / "processed"

MODEL_PATH_ENV = "CHARGING_MODEL_PATH"
HISTORY_PATH_ENV = "CHARGING_HISTORY_PATH"
ACTIVE_MODEL_FILE = PROJECT_ROOT / "ml" / "model_registry" / "active.txt"

VALID_HORIZONS = (1, 6, 24)


def discover_model_path() -> str:
    """Resolve the active trained model inside ``ml/model_registry``.

    Precedence: ``CHARGING_MODEL_PATH`` environment variable, the optional
    ``active.txt`` pointer file, a registry that contains exactly one model,
    then the default Random Forest model.
    """
    configured = os.environ.get(MODEL_PATH_ENV)
    if configured:
        return configured
    registry = PROJECT_ROOT / "ml" / "model_registry"
    if ACTIVE_MODEL_FILE.is_file():
        name = ACTIVE_MODEL_FILE.read_text(encoding="utf-8").strip()
        if name:
            candidate = Path(name) if os.path.isabs(name) else registry / name
            if candidate.exists():
                return str(candidate)
    if registry.is_dir():
        models = [path for path in registry.iterdir() if path.is_dir()]
        if len(models) == 1:
            return str(models[0])
    return DEFAULT_MODEL_PATH


def _load_model(model_path: str, spark: Any = None) -> Any:
    """Load a Spark PipelineModel directory or a pickle file.

    Returns ``None`` when a Spark PipelineModel directory is requested but no
    Spark session is available; pickle files never need Spark.
    """
    path = Path(model_path)
    if path.is_dir():
        if spark is None:
            return None
        from pyspark.ml import PipelineModel

        return PipelineModel.load(str(path.expanduser()))
    with path.open("rb") as handle:
        return pickle.load(handle)


def _read_history_csv(path: str) -> pd.DataFrame:
    """Read a single CSV or a Spark ``part-*.csv`` directory into pandas.

    Spark CSV directories repeat the header line in every part file; those
    duplicated rows are filtered before the parts are concatenated.
    """
    target = Path(path).expanduser()
    if not target.exists():
        raise FileNotFoundError(f"Forecast history does not exist: {target}")
    if target.is_file():
        return pd.read_csv(target)
    part_files = sorted(target.glob("part-*.csv"))
    if not part_files:
        raise FileNotFoundError(f"No part-*.csv found in history directory: {target}")
    frames = []
    for part in part_files:
        frame = pd.read_csv(part)
        if "datetime" in frame.columns:
            frame = frame[frame["datetime"].astype(str) != "datetime"]
        frames.append(frame)
    return pd.concat(frames, ignore_index=True)


def _json_value(value: Any) -> Any:
    """Convert numpy/pandas scalars into plain JSON-friendly Python values."""
    if isinstance(value, (pd.Timestamp, datetime)):
        return value.isoformat()
    if isinstance(value, date):
        return value.isoformat()
    if isinstance(value, (np.floating, float)):
        return float(value)
    if isinstance(value, (np.integer, int)):
        return int(value)
    if isinstance(value, bool):
        return bool(value)
    if isinstance(value, dict):
        return {str(key): _json_value(item) for key, item in value.items()}
    if isinstance(value, (list, tuple)):
        return [_json_value(item) for item in value]
    return value


def _json_records(value: Any) -> list[dict[str, Any]]:
    rows = value.to_dict("records") if isinstance(value, pd.DataFrame) else list(value)
    return [{str(key): _json_value(item) for key, item in row.items()} for row in rows]


class PredictionService:
    """JIT-load the active model and history, then serve forecast horizons."""

    def __init__(
        self,
        model_path: Optional[str] = None,
        history_path: Optional[str] = None,
        model: Any = None,
        history: Any = None,
    ):
        self.model_path = model_path if model_path is not None else (None if model is not None else discover_model_path())
        self.history_path = (
            history_path
            if history_path is not None
            else None if history is not None else (os.environ.get(HISTORY_PATH_ENV) or DEFAULT_HISTORY_PATH)
        )
        self._model = model
        self._history = history
        self._spark = None
        self._model_name = None
        self._fallback = False
        self._load_error: Optional[str] = None
        self._source = "model"
        self._loaded = False

    @property
    def model_name(self) -> Optional[str]:
        return self._model_name or (Path(self.model_path).name if self.model_path else None)

    @property
    def is_fallback(self) -> bool:
        return self._fallback

    @property
    def load_error(self) -> Optional[str]:
        return self._load_error

    @property
    def source(self) -> str:
        """How the latest response was produced (live model or cached model output)."""
        return self._source

    def _cached_inference_path(self, horizon: int) -> Optional[Path]:
        configured = os.environ.get("CHARGING_INFERENCE_DIR")
        directory = Path(configured).expanduser() if configured else DEFAULT_INFERENCE_DIR
        candidate = directory / f"inference_{horizon}h.csv"
        return candidate if candidate.is_file() else None

    def _read_cached_forecast(self, horizon: int, station_id: Optional[str] = None) -> list[dict[str, Any]]:
        path = self._cached_inference_path(horizon)
        if path is None:
            return []
        frame = pd.read_csv(path)
        if station_id is not None:
            frame = frame[frame["station_id"].astype(str) == str(station_id)]
        if frame.empty:
            raise ValueError("No history rows available for the requested station")
        frame["datetime"] = pd.to_datetime(frame["datetime"], errors="coerce")
        frame = frame.dropna(subset=["datetime"])
        return _json_records(frame)

    def _ensure_spark(self) -> Any:
        if self._spark is None:
            from pyspark.sql import SparkSession

            self._spark = (
                SparkSession.builder.appName("ChargingPlatform-PredictApi")
                .config("spark.ui.enabled", "false")
                .config("spark.driver.bindAddress", "127.0.0.1")
                .config("spark.driver.host", "127.0.0.1")
                .config("spark.sql.session.timeZone", "Asia/Shanghai")
                .config("spark.sql.execution.arrow.pyspark.enabled", "false")
                .getOrCreate()
            )
        return self._spark

    def _load(self) -> None:
        if self._loaded:
            return
        self._load_error = None
        spark = self._spark
        model = self._model
        model_path = self.model_path
        history_path = self.history_path

        need_spark_model = model_path is not None and Path(model_path).is_dir()
        need_spark_history = history_path is not None and Path(history_path).is_dir()
        if (need_spark_model or need_spark_history) and spark is None:
            try:
                spark = self._ensure_spark()
            except Exception as error:  # pragma: no cover - environment dependent
                self._load_error = f"PySpark unavailable: {error}"
                spark = None
        self._spark = spark

        if model is None and model_path is not None:
            try:
                model = _load_model(model_path, spark=spark)
            except Exception as error:
                self._load_error = str(error)
                model = None

        # Windows development environments often do not have PySpark, while
        # the teacher VM has already generated trained-model inference CSVs.
        # Use those artifacts instead of silently switching to persistence.
        default_model = Path(DEFAULT_MODEL_PATH).expanduser().resolve()
        requested_model = Path(model_path).expanduser().resolve() if model_path else None
        can_use_default_cache = requested_model == default_model
        if model is None and can_use_default_cache and self._cached_inference_path(24) is not None:
            self._source = "cached-trained-model"
            self._load_error = None
            self._model = None
            self._model_name = Path(model_path).name
            self._fallback = False
            self._loaded = True
            return

        if self._history is None and history_path is not None:
            if need_spark_history and spark is not None:
                self._history = spark.read.option("header", True).option("inferSchema", True).csv(history_path)
            else:
                self._history = _read_history_csv(history_path)

        self._model = model
        self._model_name = (
            Path(model_path).name
            if model_path is not None
            else getattr(model, "name", "persistence-fallback")
        )
        self._fallback = model is None
        self._source = "model" if model is not None else "persistence-fallback"
        self._loaded = True

    def forecast(self, horizon: int, station_id: Optional[str] = None, as_records: bool = True) -> Any:
        """Predict ``horizon`` future hours for every station (or one station)."""
        if horizon not in VALID_HORIZONS:
            raise ValueError(f"horizon must be one of {VALID_HORIZONS}")
        self._load()
        if self._source == "cached-trained-model":
            records = self._read_cached_forecast(horizon, station_id=station_id)
            return records if as_records else pd.DataFrame(records)
        function = {1: forecast_1h, 6: forecast_6h, 24: forecast_24h}[horizon]
        result = function(self._model, self._history, station_id=station_id, as_records=as_records, spark=self._spark)
        return _json_records(result) if as_records else result


def build_forecast_payload(service: PredictionService, horizon: int, station_id: Optional[str] = None):
    """Run a forecast and wrap it in the dashboard-friendly JSON contract."""
    try:
        records = service.forecast(horizon, station_id=station_id, as_records=True)
    except ValueError as error:
        message = str(error)
        status = 404 if "No history rows" in message or "requested station" in message else 400
        return {"code": 1, "msg": message, "error": message}, status
    except Exception as error:
        return {"code": 1, "msg": f"prediction failed: {error}", "error": str(error)}, 500
    payload = {
        "code": 0,
        "msg": "ok",
        "generatedAt": datetime.now(timezone.utc).isoformat(),
        "horizonHours": int(horizon),
        "model": service.model_name or "persistence-fallback",
        "isFallback": bool(service.is_fallback),
        "source": service.source,
        "data": records,
    }
    if service.load_error:
        payload["warning"] = service.load_error
    return payload


if __name__ == "__main__":
    import json

    service = PredictionService()
    for hours in VALID_HORIZONS:
        print(json.dumps(build_forecast_payload(service, hours), ensure_ascii=False, indent=2))
