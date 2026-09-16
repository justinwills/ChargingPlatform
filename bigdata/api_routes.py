"""
FastAPI route task scaffold for the Web dashboard.

Owners: 洪维斌, 邱辰笙
"""

from __future__ import annotations

import os
from pathlib import Path
from typing import Any

import pandas as pd


DATA_ROOT = Path(__file__).resolve().parent.parent / "data" / "raw"


def _payload(data: Any, *, msg: str = "ok") -> dict[str, Any]:
    return {"code": 0, "msg": msg, "data": data}


def _read_csv(filename: str) -> pd.DataFrame:
    path = DATA_ROOT / filename
    if not path.exists():
        return pd.DataFrame()
    return pd.read_csv(path)


def _as_int(value: Any, default: int = 0) -> int:
    try:
        return int(value)
    except (TypeError, ValueError):
        return default


def _as_float(value: Any, default: float = 0.0) -> float:
    try:
        return float(value)
    except (TypeError, ValueError):
        return default


def _date_window(frame: pd.DataFrame, days: int = 7) -> pd.DataFrame:
    if frame.empty or "created_at" not in frame.columns:
        return frame
    frame = frame.copy()
    frame["created_at"] = pd.to_datetime(frame["created_at"], errors="coerce")
    frame = frame.dropna(subset=["created_at"])
    if days is not None:
        cutoff = pd.Timestamp.now(tz=None).normalize() - pd.Timedelta(days=max(1, int(days)) - 1)
        frame = frame[frame["created_at"] >= cutoff]
    return frame


def _series_by_day(frame: pd.DataFrame, value_column: str, label_column: str = "created_at") -> list[dict[str, Any]]:
    if frame.empty or label_column not in frame.columns:
        return []
    series = frame.copy()
    series[label_column] = pd.to_datetime(series[label_column], errors="coerce")
    series = series.dropna(subset=[label_column])
    if series.empty:
        return []
    grouped = series.groupby(series[label_column].dt.strftime("%Y-%m-%d"), sort=True)[value_column].sum()
    return [
        {"date": str(date), "value": round(float(value), 3)}
        for date, value in grouped.items()
    ]


def charging_volume_trend(days: int = 7, **kwargs):
    """
    [Task #80] 大数据可视化大屏（Web端）/ 数据可视化 / 充电量趋势图
    Owner: 邱辰笙

    返回按日期统计的充电量趋势，用于 ECharts 折线图。
    """
    frame = _read_csv("charging_orders.csv")
    if frame.empty or "energy_kwh" not in frame.columns:
        return _payload({"labels": [], "values": [], "unit": "kwh"})
    filtered = _date_window(frame, days=days)
    grouped = filtered.groupby(filtered["created_at"].dt.strftime("%Y-%m-%d"), sort=True)["energy_kwh"].sum()
    return _payload(
        {
            "labels": [str(day) for day in grouped.index],
            "values": [round(float(value), 3) for value in grouped.values],
            "unit": "kwh",
        }
    )


def revenue_trend(days: int = 7, **kwargs):
    """
    [Task #81] 大数据可视化大屏（Web端）/ 数据可视化 / 营收趋势图
    Owner: 邱辰笙

    根据 fee_amount_cny 统计不同时间段的充电费用变化。
    """
    frame = _read_csv("charging_orders.csv")
    if frame.empty or "fee_amount_cny" not in frame.columns:
        return _payload({"labels": [], "values": [], "unit": "cny"})
    filtered = _date_window(frame, days=days)
    grouped = filtered.groupby(filtered["created_at"].dt.strftime("%Y-%m-%d"), sort=True)["fee_amount_cny"].sum()
    return _payload(
        {
            "labels": [str(day) for day in grouped.index],
            "values": [round(float(value), 2) for value in grouped.values],
            "unit": "cny",
        }
    )


def session_count_trend(days: int = 7, **kwargs):
    """
    [Task #82] 大数据可视化大屏（Web端）/ 数据可视化 / 充电次数趋势图
    Owner: 洪维斌

    返回按日期统计的充电会话数。
    """
    frame = _read_csv("charging_orders.csv")
    if frame.empty or "created_at" not in frame.columns:
        return _payload({"labels": [], "values": [], "unit": "sessions"})
    filtered = _date_window(frame, days=days)
    grouped = filtered.groupby(filtered["created_at"].dt.strftime("%Y-%m-%d"), sort=True).size()
    return _payload(
        {
            "labels": [str(day) for day in grouped.index],
            "values": [int(value) for value in grouped.values],
            "unit": "sessions",
        }
    )


def station_distribution(*args, **kwargs):
    """
    [Task #88] 大数据可视化大屏（Web端）/ 数据可视化 / 充电站分布展示
    Owner: 洪维斌

    返回站点位置和设备数量，便于地图/表格展示。
    """
    frame = _read_csv("stations.csv")
    if frame.empty:
        return _payload([])
    columns = [
        column for column in [
            "station_id",
            "station_name",
            "address",
            "latitude",
            "longitude",
            "device_count",
            "facility_type",
            "city",
        ]
        if column in frame.columns
    ]
    result = frame[columns].copy()
    if "device_count" in result.columns:
        result["device_count"] = pd.to_numeric(result["device_count"], errors="coerce").fillna(0).astype(int)
    if "latitude" in result.columns:
        result["latitude"] = pd.to_numeric(result["latitude"], errors="coerce")
    if "longitude" in result.columns:
        result["longitude"] = pd.to_numeric(result["longitude"], errors="coerce")
    return _payload(result.to_dict(orient="records"))


def device_status_chart(*args, **kwargs):
    """
    [Task #89] 大数据可视化大屏（Web端）/ 数据可视化 / 设备运行状态图
    Owner: 洪维斌

    汇总设备在线率、故障天数、健康评分和输出功率指标。这里以设备状态日志为核心数据源。
    """
    status = _read_csv("device_status_log.csv")
    devices = _read_csv("devices.csv")
    summary: dict[str, Any] = {
        "total_devices": int(devices["device_id"].nunique()) if not devices.empty and "device_id" in devices.columns else 0,
        "avg_availability_rate": 0.0,
        "avg_health_score": 0.0,
        "avg_output_power_kw": 0.0,
        "fault_count": 0,
    }
    if not status.empty:
        if "availability_rate" in status.columns:
            summary["avg_availability_rate"] = round(float(pd.to_numeric(status["availability_rate"], errors="coerce").mean()), 4)
        if "health_score" in status.columns:
            summary["avg_health_score"] = round(float(pd.to_numeric(status["health_score"], errors="coerce").mean()), 2)
        if "avg_output_power_kw" in status.columns:
            summary["avg_output_power_kw"] = round(float(pd.to_numeric(status["avg_output_power_kw"], errors="coerce").mean()), 2)
        if "fault_count" in status.columns:
            summary["fault_count"] = int(pd.to_numeric(status["fault_count"], errors="coerce").fillna(0).sum())
    by_status = []
    if not devices.empty and "current_status" in devices.columns:
        counts = devices["current_status"].fillna("unknown").astype(str).str.lower().value_counts()
        by_status = [{"status": str(status_name), "count": int(count)} for status_name, count in counts.items()]
    battery_analysis = {
        "avg_health_score": summary["avg_health_score"],
        "avg_output_power_kw": summary["avg_output_power_kw"],
        "avg_availability_rate": summary["avg_availability_rate"],
        "fault_count": summary["fault_count"],
    }
    return _payload({"summary": summary, "by_status": by_status, "battery_analysis": battery_analysis})


def filtered_stats(
    date: str = None,
    weekday: int | str = None,
    station_id: str = None,
    device_id: str = None,
    is_holiday: int = None,
    weather_type: str = None,
    **kwargs,
):
    """
    [Task #90] 大数据可视化大屏（Web端）/ 页面交互 / 图表筛选
    Owner: 洪维斌

    支持按日期、星期、站点、设备、节假日和天气过滤后返回 24 小时分布。
    """
    frame = _read_csv("charging_orders.csv")
    if frame.empty:
        return _payload([])

    filtered = frame.copy()
    if "created_at" in filtered.columns and date is not None:
        filtered["created_at"] = pd.to_datetime(filtered["created_at"], errors="coerce")
        filtered = filtered[filtered["created_at"].dt.strftime("%Y-%m-%d") == str(date)]

    if weekday is not None:
        weekday_value = str(weekday)
        for column in ["weekday", "start_weekday"]:
            if column in filtered.columns:
                filtered = filtered[filtered[column].astype(str) == weekday_value]
                break

    if station_id is not None and "station_id" in filtered.columns:
        filtered = filtered[filtered["station_id"].astype(str) == str(station_id)]
    if device_id is not None and "device_id" in filtered.columns:
        filtered = filtered[filtered["device_id"].astype(str) == str(device_id)]
    if is_holiday is not None and "is_holiday" in filtered.columns:
        filtered = filtered[filtered["is_holiday"].astype(str) == str(is_holiday)]

    if weather_type is not None:
        weather = _read_csv("weather_hourly.csv")
        if not weather.empty and "weather_id" in weather.columns and "weather_type" in weather.columns:
            weather_map = weather[["weather_id", "weather_type"]].dropna().drop_duplicates()
            filtered = filtered.merge(weather_map, on="weather_id", how="left")
            filtered = filtered[filtered["weather_type"].astype(str).str.lower() == str(weather_type).lower()]

    if "start_hour" in filtered.columns:
        filtered = filtered[filtered["start_hour"].between(0, 23)]
        counts = filtered["start_hour"].value_counts().sort_index()
        series = [
            {"hour": int(hour), "charging_sessions": int(counts.get(hour, 0))}
            for hour in range(24)
        ]
        return _payload(series)

    return _payload([])


def register_stats_routes(app):
    """
    [Task #121] 智能推荐与系统集成 / 后端接口 / 数据接口
    Owner: 邱辰笙

    建立后端接口，为 Web 大屏提供充电量、费用、站点统计、设备运行、
    天气影响等数据。
    """

    @app.get("/api/stats/charging-volume-trend")
    def _charging_volume_trend(days: int = 7):
        return charging_volume_trend(days=days)

    @app.get("/api/stats/revenue-trend")
    def _revenue_trend(days: int = 7):
        return revenue_trend(days=days)

    @app.get("/api/stats/session-count-trend")
    def _session_count_trend(days: int = 7):
        return session_count_trend(days=days)

    @app.get("/api/stats/station-distribution")
    def _station_distribution():
        return station_distribution()

    @app.get("/api/stats/device-status-chart")
    def _device_status_chart():
        return device_status_chart()

    @app.get("/api/stats/filtered")
    def _filtered_stats(
        date: str = None,
        weekday: str = None,
        station_id: str = None,
        device_id: str = None,
        is_holiday: int = None,
        weather_type: str = None,
    ):
        return filtered_stats(
            date=date,
            weekday=weekday,
            station_id=station_id,
            device_id=device_id,
            is_holiday=is_holiday,
            weather_type=weather_type,
        )
