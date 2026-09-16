"""Operations guidance for high-load charging stations."""

from __future__ import annotations

import os
from pathlib import Path
from typing import Any, Iterable, Optional

import pandas as pd


PREDICTION_ENV = "CHARGING_PREDICTION_PATH"
STATION_ENV = "CHARGING_STATION_PATH"
HISTORY_ENV = "CHARGING_HISTORY_PATH"


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


def _as_float(series: pd.Series, default: float = 0.0) -> pd.Series:
    return pd.to_numeric(series, errors="coerce").fillna(default).astype(float)


def generate_ops_advice(
    station_id: str = None,
    predictions: Any = None,
    stations: Any = None,
    history: Any = None,
    threshold: float = 80.0,
    **kwargs: Any,
) -> dict[str, Any]:
    """
    [Task #120] 智能推荐与系统集成 / 运营分析 / 运维建议
    Owner: 特布新

    根据站点历史数据和预测负荷结果生成高峰前运维、资源调配等建议。
    """
    prediction_frame = _read_optional_frame(predictions, PREDICTION_ENV)
    station_frame = _read_optional_frame(stations, STATION_ENV)
    history_frame = _read_optional_frame(history, HISTORY_ENV)

    if station_id is None and prediction_frame.empty:
        raise ValueError("station_id or prediction data is required")

    if not prediction_frame.empty and "station_id" in prediction_frame.columns:
        prediction_frame = prediction_frame.copy()
        if station_id is not None:
            prediction_frame = prediction_frame[prediction_frame["station_id"].astype(str) == str(station_id)]
        if prediction_frame.empty:
            prediction_frame = pd.DataFrame(
                [{"station_id": str(station_id), "predicted_load": 0.0, "datetime": None}]
            )

    station_row = pd.DataFrame()
    if not station_frame.empty and "station_id" in station_frame.columns:
        station_row = station_frame[station_frame["station_id"].astype(str) == str(station_id)].copy()
    if station_row.empty and station_id is not None:
        station_row = pd.DataFrame([
            {
                "station_id": str(station_id),
                "device_count": 0,
                "available_chargers": 0,
                "station_status": "unknown",
            }
        ])

    if prediction_frame.empty and station_row.empty:
        raise ValueError(f"No data found for station {station_id}")

    predicted_series = pd.Series([0.0])
    if not prediction_frame.empty:
        load_column = _first_column(prediction_frame, ["predicted_load", "predicted_load_kwh", "prediction", "load"])
        if load_column is not None:
            predicted_series = _as_float(prediction_frame[load_column], 0.0)
        else:
            predicted_series = pd.Series([0.0] * len(prediction_frame))
    predicted_load = float(predicted_series.max()) if len(predicted_series) else 0.0

    station_values = station_row.iloc[0] if not station_row.empty else {}
    device_count = float(station_values.get("device_count", station_values.get("charger_count", station_values.get("total_devices", 0.0))))
    available = float(station_values.get("available_chargers", station_values.get("available_devices", station_values.get("available_count", max(device_count, 0.0)))))
    status = str(station_values.get("station_status", station_values.get("status", "unknown"))).lower()

    if history_frame.empty or "station_id" not in history_frame.columns:
        historical_volume = 0.0
        historical_frequency = 0.0
    else:
        history_match = history_frame[history_frame["station_id"].astype(str) == str(station_id)]
        volume_column = _first_column(history_match, ["energy_kwh", "historical_volume", "charge_volume", "charging_volume"])
        historical_volume = float(_as_float(history_match[volume_column], 0.0).sum()) if volume_column else 0.0
        historical_frequency = float(len(history_match))

    if predicted_load >= threshold:
        priority = "high"
        action = "立即启动高峰前运维：疏导可用充电桩、提醒用户错峰充电，并检查站内设备温度和供电状态。"
    elif predicted_load >= threshold * 0.85:
        priority = "medium"
        action = "关注站点负荷上升：提前调度备用桩、通知用户选择相邻站点充电。"
    else:
        priority = "low"
        action = "正常运营，继续监控负荷变化并保留备用充电资源。"

    advice_items = [action]
    if available > 0:
        advice_items.append(f"当前可用桩数为 {available}，建议保留至少 {max(1, int(available * 0.3))} 个桩作为备用。")
    else:
        advice_items.append("当前站点无可用备桩，建议启动跨站协同调度或引导用户前往邻近站点。")

    if status not in {"normal", "open", "online", "active", "available", "1", "true"}:
        advice_items.append(f"站点状态为 {status}，建议优先检查设备在线状态与安全巡检。")
    if historical_volume > 0:
        advice_items.append(f"历史充电量为 {historical_volume:.1f} kWh，近峰值时段已表现出较高使用强度。")

    answer = {
        "station_id": str(station_id),
        "predicted_load": round(predicted_load, 2),
        "threshold": float(threshold),
        "priority": priority,
        "available_chargers": round(float(available), 2),
        "historical_volume_kwh": round(float(historical_volume), 2),
        "historical_frequency": round(float(historical_frequency), 2),
        "advice": advice_items,
    }
    return answer

