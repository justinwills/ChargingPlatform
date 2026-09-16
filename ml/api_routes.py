"""
ml/api_routes.py — Phase 2
Flask route handlers for forecasting and station recommendation. See
run_api.py for how this gets mounted.

Owner(s): 邱辰笙
"""

import os
from datetime import datetime, timezone
from pathlib import Path

import pandas as pd
from flask import request

from ml.prediction_service import PredictionService, build_forecast_payload
from ml.forecast import forecast_1h, forecast_6h, forecast_24h, forecast_per_station
from ml.recommend.scoring import station_recommend_score, recommend_low_load_stations
from ml.recommend.peak_alert import flag_upcoming_peak_stations, load_threshold_alert
from ml.recommend.ops_advice import generate_ops_advice


PROJECT_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_STATION_PATH = PROJECT_ROOT / "data" / "processed" / "stations.csv"


def _recommendation_predictions() -> pd.DataFrame:
    """Predictions DataFrame shared by the recommendation endpoints.

    Pulls the same 24h forecast the Web dashboard consumes so the smart
    recommendation is driven by predicted load rather than raw distance.
    """
    service = PredictionService()
    try:
        records = service.forecast(24, as_records=True)
    except Exception as error:  # pragma: no cover - environment dependent
        print(f"recommend: forecast unavailable: {error}", flush=True)
        return pd.DataFrame()
    return pd.DataFrame(records)


def _station_frame() -> pd.DataFrame:
    configured = os.getenv("CHARGING_STATION_PATH")
    path = Path(configured) if configured else DEFAULT_STATION_PATH
    if not path.exists():
        return pd.DataFrame()
    frame = pd.read_csv(path)
    if "station_id" not in frame.columns and "stationId" in frame.columns:
        frame["station_id"] = frame["stationId"]
    return frame


def _enrich_recommendations(records: list[dict], stations: pd.DataFrame) -> list[dict]:
    if not records or stations.empty:
        return records
    lookup = {
        str(station_id): index
        for index, station_id in stations["station_id"].astype(str).items()
    }
    for record in records:
        info = stations.iloc[lookup[str(record["station_id"])]] if str(record["station_id"]) in lookup else None
        if info is None:
            continue
        record["station_name"] = _safe_value(info.get("station_name"))
        record["address"] = _safe_value(info.get("address"))
    return records


def _safe_value(value) -> str:
    if value is None:
        return ""
    value = str(value).strip()
    return "" if value.lower() in {"nan", "none", ""} else value


def register_predict_routes(app):
    """
    [Task #122] 智能推荐与系统集成 / 后端接口 / 预测接口
    Owner: 邱辰笙

    建立机器学习预测API，为Web大屏提供未来1小时、6小时和24小时预测结果。

    Each GET endpoint loads the saved Spark PipelineModel (or pickle) plus the
    latest station-hour history through ``PredictionService`` and returns the
    recursive forecast produced by ``ml/forecast.py``.  The response follows
    the dashboard snapshot contract: ``{code, msg, generatedAt, horizonHours,
    model, data}``, where ``data`` contains one record per station/hour.
    """

    service = PredictionService()

    @app.get("/api/predict/1h")
    def _predict_1h():
        return build_forecast_payload(service, 1, station_id=request.args.get("station_id"))

    @app.get("/api/predict/6h")
    def _predict_6h():
        return build_forecast_payload(service, 6, station_id=request.args.get("station_id"))

    @app.get("/api/predict/24h")
    def _predict_24h():
        return build_forecast_payload(service, 24, station_id=request.args.get("station_id"))

    @app.get("/api/predict/station/<station_id>")
    def _predict_station(station_id: str):
        return build_forecast_payload(service, 24, station_id=station_id)


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
    def _recommend_stations():
        try:
            predictions = _recommendation_predictions()
            stations = _station_frame()
            records = recommend_low_load_stations(
                predictions=predictions,
                stations=stations,
                user_lat=request.args.get("user_lat", type=float),
                user_lng=request.args.get("user_lng", type=float),
                top_n=10,
            )
            return {
                "code": 0,
                "msg": "ok",
                "generatedAt": datetime.now(timezone.utc).isoformat(),
                "data": _enrich_recommendations(records, stations),
            }
        except NotImplementedError as e:
            return {"error": str(e)}, 501

    @app.get("/api/recommend/alerts")
    def _recommend_alerts():
        try:
            predictions = _recommendation_predictions()
            alerts = flag_upcoming_peak_stations(predictions=predictions, as_records=True)
            return {
                "code": 0,
                "msg": "ok",
                "generatedAt": datetime.now(timezone.utc).isoformat(),
                "data": alerts,
            }
        except NotImplementedError as e:
            return {"error": str(e)}, 501

    @app.get("/api/recommend/ops-advice/<station_id>")
    def _ops_advice(station_id: str):
        try:
            return generate_ops_advice(station_id=station_id)
        except NotImplementedError as e:
            return {"error": str(e)}, 501
