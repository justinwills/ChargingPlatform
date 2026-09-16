"""
bigdata/api_routes.py — Phase 2 实现
Flask: Web 大屏 /api/stats/* 数据接口（数据可视化 + 页面交互）。

在 main 已声明的 /api/stats/* 路由契约（名称与参数）基础上实现真实数据：
统计逻辑来自 bigdata/dashboard（pandas 分析），机器学习由 /api/predict/*
与 /api/recommend/*（ml/api_routes.py）提供，本文件只负责大屏可视化数据。

Owner(s): 洪维文（Web大屏数据可视化）, 邱辰笙（数据接口）
"""

import os
from functools import lru_cache

import pandas as pd
from flask import request

from .dashboard import charging_stats as cs
from .dashboard import station_ranking as sr

DATA_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "data", "raw")


def _ok(data):
    return {"code": 0, "msg": "ok", "data": data}


def _err(msg, status=501):
    return {"code": 1, "msg": str(msg), "data": None}, status


def _int_arg(name, default):
    """Read an integer query parameter while keeping a safe default."""
    value = request.args.get(name, default=default, type=int)
    return default if value is None else value


def _comfort(_status):
    """把英文状态映射为中文（在用/闲置/故障）。"""
    return {"online": "在用", "idle": "闲置", "fault": "故障"}.get(str(_status), str(_status))


@lru_cache(maxsize=4)
def _devices() -> pd.DataFrame:
    return pd.read_csv(os.path.join(DATA_DIR, "devices.csv"))


@lru_cache(maxsize=4)
def _status_log() -> pd.DataFrame:
    return pd.read_csv(os.path.join(DATA_DIR, "device_status_log.csv"))


# ---------------------------------------------------------------------------
# 数据可视化：充电量 / 营收 / 充电次数 趋势
# ---------------------------------------------------------------------------
def charging_volume_trend(days: int = 7):
    """[Task #80] 充电量趋势图"""
    return {"days": days, "points": cs.charging_volume_trend(days=days)}


def revenue_trend(days: int = 7):
    """[Task #81] 营收趋势图"""
    return {"days": days, "points": cs.revenue_trend(days=days)}


def session_count_trend(days: int = 7):
    """[Task #82] 充电次数趋势图"""
    return {"days": days, "points": cs.session_count_trend(days=days)}


def station_distribution():
    """[Task #88] 充电站分布展示"""
    return sr.station_distribution()


def device_status_chart(days: int = 7):
    """[Task #89] 设备运行状态图（取自 device_status_log）"""
    log = _status_log().copy()
    log["record_time"] = pd.to_datetime(log["record_time"], errors="coerce")
    if days and log["record_time"].notna().any():
        last = log["record_time"].max()
        if pd.notna(last):
            log = log[log["record_time"] >= (last - pd.Timedelta(days=days))]
    if log.empty:
        return []
    g = log.groupby("station_id").agg(
        avg_availability=("availability_rate", "mean"),
        total_faults=("fault_count", "sum"),
        successful_sessions=("successful_sessions", "sum"),
        failed_sessions=("failed_sessions", "sum"),
        avg_health=("health_score", "mean"),
    ).reset_index()
    return g.round(4).to_dict("records")


def filtered_stats(date=None, weekday=None, station_id=None, device_id=None, is_holiday=None, weather_type=None, **kwargs):
    """[Task #90] 图表筛选：按日期 / 星期 / 充电站 / 设备 / 节假日过滤订单统计。"""
    df = cs.load_orders()
    if station_id:
        df = df[df["station_id"].astype(str) == str(station_id)]
    if date:
        df = df[df["created_at"].dt.strftime("%Y-%m-%d") == str(date)]
    if weekday:
        wd_map = {"Mon": 0, "Tue": 1, "Wed": 2, "Thu": 3, "Fri": 4, "Sat": 5, "Sun": 6}
        df = df[df["created_at"].dt.weekday == wd_map.get(str(weekday), -1)]
    points = (
        df.resample("D", on="created_at")
        .agg(energy_kwh=("energy_kwh", "sum"), sessions=("session_id", "count"), revenue=("fee_amount_cny", "sum"))
        .dropna(how="all")
        .reset_index()
    )
    points = points.rename(columns={"created_at": "date"})
    points["date"] = points["date"].dt.strftime("%Y-%m-%d")
    return {
        "total_energy_kwh": float(df["energy_kwh"].sum()),
        "total_sessions": int(len(df)),
        "total_revenue": float(df["fee_amount_cny"].sum()),
        "points": points.to_dict("records"),
    }


# ---------------------------------------------------------------------------
# 路由注册（沿用 main 声明的 /api/stats/* 契约 + 大屏额外图表接口）
# ---------------------------------------------------------------------------
def register_stats_routes(app):
    """
    [Task #121] 智能推荐与系统集成 / 后端接口 / 数据接口
    Web 大屏数据接口：充电量、费用、站点统计、设备运行、天气影响等。
    """

    @app.get("/api/stats/overview")
    def _overview():
        return _ok(cs.overview())

    @app.get("/api/stats/charging-volume-trend")
    def _charging_volume_trend():
        days = _int_arg("days", 7)
        return _ok(charging_volume_trend(days=days))

    @app.get("/api/stats/revenue-trend")
    def _revenue_trend():
        days = _int_arg("days", 7)
        return _ok(revenue_trend(days=days))

    @app.get("/api/stats/session-count-trend")
    def _session_count_trend():
        days = _int_arg("days", 7)
        return _ok(session_count_trend(days=days))

    @app.get("/api/stats/hourly-distribution")
    def _hourly_distribution():
        days = _int_arg("days", 7)
        return _ok(cs.hourly_distribution(days=days))

    @app.get("/api/stats/weekday-pattern")
    def _weekday_pattern():
        days = _int_arg("days", 7)
        return _ok(list(cs.weekday_pattern(days=days)))

    @app.get("/api/stats/weekday-heatmap")
    def _weekday_heatmap():
        days = _int_arg("days", 7)
        return _ok(cs.weekday_heatmap(days=days))

    @app.get("/api/stats/user-metrics")
    def _user_metrics():
        days = _int_arg("days", 7)
        return _ok(cs.user_metrics(days=days))

    @app.get("/api/stats/weather-impact")
    def _weather_impact():
        days = _int_arg("days", 7)
        return _ok(list(cs.weather_impact(days=days)))

    @app.get("/api/stats/station-ranking")
    def _station_ranking():
        days = _int_arg("days", 30)
        limit = _int_arg("limit", 10)
        return _ok(sr.station_ranking(days=days, limit=limit))

    @app.get("/api/stats/station-distribution")
    def _station_distribution():
        return _ok(sr.station_distribution())

    @app.get("/api/stats/battery/device-operations")
    def _device_operations():
        days = _int_arg("days", 7)
        return _ok(device_status_chart(days=days))

    @app.get("/api/stats/device-status")
    def _device_status():
        days = _int_arg("days", 7)
        return _ok(device_status_chart(days=days))

    @app.get("/api/stats/device-status-chart")
    def _device_status_chart():
        """Compatibility alias used by the Phase 2 dashboard."""
        days = _int_arg("days", 7)
        return _ok(device_status_chart(days=days))

    @app.get("/api/stats/filtered")
    def _filtered_stats():
        return _ok(
            filtered_stats(
                date=request.args.get("date"),
                weekday=request.args.get("weekday"),
                station_id=request.args.get("station_id"),
                device_id=request.args.get("device_id"),
                is_holiday=request.args.get("is_holiday", type=int),
                weather_type=request.args.get("weather_type"),
            )
        )

    @app.get("/api/stats")
    def _compat_stats():
        """Phase 1 C++ ChargingServer 大屏兼容格式（电桩状态饼图 + 在线率）。"""
        days = _int_arg("days", 7)
        rev = cs.revenue_today_this_month_total()
        names = ["在用", "闲置", "故障"]
        counts = _devices()["current_status"].map(_comfort).value_counts()
        pile_status = {n: int(counts.get(n, 0)) for n in names}
        station_info = {int(s["station_id"]): s for _, s in sr.load_stations().iterrows()}
        stations_out = [
            {
                "name": o["name"],
                "pileCount": o["pileCount"],
                "freePileCount": o["freePileCount"],
                "onlineRate": o["onlineRate"],
            }
            for o in sr.station_online_stats()
            if int(o["station_id"]) in station_info
        ]
        return _ok(
            {
                "revenueToday": rev["revenue_today"],
                "revenueThisMonth": rev["revenue_30d"],
                "revenueTotal": rev["revenue_total"],
                "trendDays": days,
                "revenueTrend": cs.revenue_trend(days=days),
                "pileStatus": pile_status,
                "stations": stations_out,
            }
        )
