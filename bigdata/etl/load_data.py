"""
Load Data - Phase 2 task scaffold.

The updated tasklist uses six raw CSV tables:
charging_orders.csv, stations.csv, devices.csv, users.csv,
weather_hourly.csv, and device_status_log.csv.
"""

from pathlib import Path

import pandas as pd

DATA_ROOT = Path(__file__).resolve().parents[2] / "data"
RAW_DIR = DATA_ROOT / "raw"
PROCESSED_DIR = DATA_ROOT / "processed"


def _save_processed(df: pd.DataFrame, filename: str) -> Path:
    PROCESSED_DIR.mkdir(parents=True, exist_ok=True)
    out = PROCESSED_DIR / filename
    df.to_csv(out, index=False, encoding="utf-8-sig")
    return out


def load_charging_orders(*args, **kwargs):
    """
    [Task #61] 大数据可视化大屏（Web端）/ 数据导入 / 充电订单数据导入
    Owner: 王清香

    读取 charging_orders.csv，导入充电会话、用户、充电站、充电桩、
    充电时间、充电电量、充电费用、星期及节假日等订单数据。
    """
    # TODO: implement
    raise NotImplementedError("Task #61: 充电订单数据导入")


def load_stations(path=None) -> pd.DataFrame:
    """
    [Task #62] 大数据可视化大屏（Web端）/ 数据导入 / 充电站数据导入
    Owner: 薛学刚

    读取 stations.csv，获取充电站ID、站点名称、地址、经纬度、电桩数量、
    开放时间、电价、服务费及站点状态等信息。
    """
    # TODO: implement
    raise NotImplementedError("Task #62: 充电站数据导入")


def load_devices(path=None) -> pd.DataFrame:
    """
    [Task #63] 大数据可视化大屏（Web端）/ 数据导入 / 充电设备数据导入
    Owner: 薛学刚

    读取 devices.csv，获取电桩ID、所属充电站、充电类型、额定功率、
    接口数量、制造商及当前运行状态等信息。
    """
    # TODO: implement
    raise NotImplementedError("Task #63: 充电设备数据导入")


def load_users(path=None) -> pd.DataFrame:
    """
    [Task #64] 大数据可视化大屏（Web端）/ 数据导入 / 用户数据导入
    Owner: 薛学刚

    读取 users.csv，获取用户ID、注册时间、城市、会员等级、车辆类型、
    电池容量、使用平台及用户状态等信息。
    """
    # TODO: implement
    raise NotImplementedError("Task #64: 用户数据导入")


def load_weather(path=None) -> pd.DataFrame:
    """
    [Task #65] 大数据可视化大屏（Web端）/ 数据导入 / 天气数据导入
    Owner: 薛学刚

    读取 weather_hourly.csv，获取小时级天气类型、温度、湿度、降水量、
    风速、能见度及降雨状态等数据。
    """
    # TODO: implement
    raise NotImplementedError("Task #65: 天气数据导入")


def load_device_status(path=None) -> pd.DataFrame:
    """
    [Task #66] 大数据可视化大屏（Web端）/ 数据导入 / 电桩运行数据导入
    Owner: 薛学刚

    读取 device_status_log.csv，获取电桩在线时长、离线时长、故障时长、
    故障次数、成功充电次数、平均输出功率、可用率、健康评分及维护状态等数据。
    """
    # TODO: implement
    raise NotImplementedError("Task #66: 电桩运行数据导入")


def associate_charging_data(*args, **kwargs):
    """
    [Task #67] 大数据可视化大屏（Web端）/ 数据关联 / 多表数据关联
    Owner: 薛学刚

    根据 user_id、station_id、device_id、location_id 和 weather_id 等关联字段，
    对订单、用户、充电站、充电设备及天气数据进行关联。
    """
    # TODO: implement
    raise NotImplementedError("Task #67: 多表数据关联")
