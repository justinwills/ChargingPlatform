"""
ml/api_routes.py — Phase 2 stub
FastAPI route handlers for forecasting and station recommendation. See
run_api.py for how this gets mounted.

Owner(s): 邱辰笙
"""

import logging
import os
from pathlib import Path
from typing import Any, Optional

import pandas as pd

from ml.forecast import forecast_1h, forecast_6h, forecast_24h, forecast_per_station
from ml.recommend.scoring import station_recommend_score, recommend_low_load_stations
from ml.recommend.peak_alert import flag_upcoming_peak_stations, load_threshold_alert
from ml.recommend.ops_advice import generate_ops_advice


LOGGER = logging.getLogger(__name__)
PREDICTION_ENV = "CHARGING_PREDICTION_PATH"


def _prediction_records(station_id: Optional[str] = None) -> list[dict[str, Any]]:
    """Load saved model output as JSON-safe records without invoking the model."""
    configured_path = os.getenv(PREDICTION_ENV)
    if not configured_path:
        LOGGER.warning("%s is not configured", PREDICTION_ENV)
        return []

    path = Path(configured_path).expanduser()
    if not path.exists():
        LOGGER.warning("Configured prediction file does not exist: %s", path)
        return []

    frame = pd.read_csv(path, dtype={"station_id": "string"})
    if "station_id" not in frame.columns:
        raise ValueError("prediction data must include station_id")
    load_column = next(
        (column for column in ("predicted_load", "predicted_load_kwh", "prediction", "load") if column in frame.columns),
        None,
    )
    if load_column is None:
        raise ValueError("prediction data must include predicted_load or predicted_load_kwh")

    if station_id is not None:
        frame = frame[frame["station_id"].astype(str) == str(station_id)]
    frame = frame.copy()
    frame["predicted_load"] = pd.to_numeric(frame[load_column], errors="coerce").fillna(0.0)
    if "datetime" in frame.columns:
        frame = frame.sort_values(["station_id", "datetime"], kind="mergesort")
    if "horizon_hour" in frame.columns:
        frame["horizon_hour"] = pd.to_numeric(frame["horizon_hour"], errors="coerce")
    else:
        frame["horizon_hour"] = frame.groupby("station_id", sort=False).cumcount() + 1

    records = []
    for row in frame.itertuples(index=False):
        time_value = getattr(row, "datetime", None)
        horizon_value = getattr(row, "horizon_hour", 0)
        records.append(
            {
                "station_id": str(row.station_id),
                "time": None if pd.isna(time_value) else str(time_value),
                "predicted_load": float(row.predicted_load),
                "horizon_hour": int(horizon_value),
            }
        )
    return records


def _prediction_payload(station_id: Optional[str] = None) -> dict[str, dict[str, list[dict[str, Any]]]]:
    records = _prediction_records(station_id=station_id)
    return {
        "predictions": {
            "1h": [record for record in records if record["horizon_hour"] <= 1],
            "6h": [record for record in records if record["horizon_hour"] <= 6],
            "24h": [record for record in records if record["horizon_hour"] <= 24],
        }
    }


def register_predict_routes(app):
    """
    [Task #122] 智能推荐与系统集成 / 后端接口 / 预测接口
    Owner: 邱辰笙

    建立机器学习预测API，为Web大屏提供未来1小时、6小时和24小时预测结果。

    Wires up GET endpoints backed by ml/forecast.py. Each handler returns 501
    until the corresponding forecast function is implemented.
    """

    @app.get("/api/predict/1h")
    def _predict_1h(station_id: str = None):
        try:
            return forecast_1h(station_id=station_id)
        except NotImplementedError as e:
            return {"error": str(e)}, 501

    @app.get("/api/predict/6h")
    def _predict_6h(station_id: str = None):
        try:
            return forecast_6h(station_id=station_id)
        except NotImplementedError as e:
            return {"error": str(e)}, 501

    @app.get("/api/predict/24h")
    def _predict_24h(station_id: str = None):
        try:
            return forecast_24h(station_id=station_id)
        except NotImplementedError as e:
            return {"error": str(e)}, 501

    @app.get("/api/predict/station/{station_id}")
    def _predict_station(station_id: str):
        try:
            return forecast_per_station(station_id=station_id)
        except NotImplementedError as e:
            return {"error": str(e)}, 501


def register_recommend_routes(app):
    """
    [Task #123] 智能推荐与系统集成 / 后端接口 / 推荐接口
    Owner: 邱辰笙

    建立智能推荐API，根据预测负荷和站点信息返回推荐充电站。

    This is the endpoint the Phase 1 Qt client should eventually call
    (see ml/qt_integration.md, Task #125) instead of sorting stations
    by distance alone.
    """

    @app.get("/api/recommend/stations")
    def _recommend_stations(user_lat: float = None, user_lng: float = None):
        try:
            return recommend_low_load_stations(user_lat=user_lat, user_lng=user_lng)
        except NotImplementedError as e:
            return {"error": str(e)}, 501

    @app.get("/api/recommend/alerts")
    def _recommend_alerts():
        try:
            return flag_upcoming_peak_stations()
        except NotImplementedError as e:
            return {"error": str(e)}, 501

    @app.get("/api/recommend/ops-advice/{station_id}")
    def _ops_advice(station_id: str):
        try:
            return generate_ops_advice(station_id=station_id)
        except NotImplementedError as e:
            return {"error": str(e)}, 501
