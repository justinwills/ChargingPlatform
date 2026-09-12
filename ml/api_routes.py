"""
ml/api_routes.py — Phase 2 stub
FastAPI route handlers for forecasting and station recommendation. See
run_api.py for how this gets mounted.

Owner(s): 邱辰笙
"""

from ml.forecast import forecast_1h, forecast_6h, forecast_24h, forecast_per_station
from ml.recommend.scoring import station_recommend_score, recommend_low_load_stations
from ml.recommend.peak_alert import flag_upcoming_peak_stations, load_threshold_alert
from ml.recommend.ops_advice import generate_ops_advice


def register_predict_routes(app):
    """
    [Task #108] 智能推荐与系统集成 / 后端接口 / 预测接口
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
    [Task #109] 智能推荐与系统集成 / 后端接口 / 推荐接口
    Owner: 邱辰笙

    建立智能推荐API，根据预测负荷和站点信息返回推荐充电站。

    This is the endpoint the Phase 1 Qt client should eventually call
    (see ml/qt_integration.md, Task #111) instead of sorting stations
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
