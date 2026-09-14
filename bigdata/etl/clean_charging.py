"""
Charging data preprocessing task scaffold.

Owners: 洪维斌
"""

import pandas as pd


def process_charging_time(*args, **kwargs):
    """
    [Task #68] 大数据可视化大屏（Web端）/ 数据预处理 / 充电时间处理
    Owner: 洪维斌

    对 charging_orders.csv 中的 created_at、ended_at、charge_time_hrs 等字段
    进行转换和规范化，并提取小时、星期、是否周末、是否节假日及是否工作日等时间特征。
    """
    # TODO: implement
    raise NotImplementedError("Task #68: 充电时间处理")


def clean_charging_data(*args, **kwargs):
    """
    [Task #69] 大数据可视化大屏（Web端）/ 数据预处理 / 充电数据清洗
    Owner: 洪维斌

    对订单中的充电电量、费用、充电时长等字段进行缺失值、重复值和异常值检查，
    保证可视化数据质量。
    """
    # TODO: implement
    raise NotImplementedError("Task #69: 充电数据清洗")


def process_weather_data(*args, **kwargs):
    """
    [Task #70] 大数据可视化大屏（Web端）/ 数据预处理 / 天气数据处理
    Owner: 洪维斌

    对 weather_hourly.csv 进行时间标准化，并根据 weather_id、location_id
    和时间字段与充电订单进行关联。
    """
    # TODO: implement
    raise NotImplementedError("Task #70: 天气数据处理")


def process_device_status_data(*args, **kwargs):
    """
    [Task #71] 大数据可视化大屏（Web端）/ 数据预处理 / 设备状态数据处理
    Owner: 洪维斌

    对 device_status_log.csv 中的设备运行状态、故障次数、可用率和健康评分
    进行清洗和统计。
    """
    # TODO: implement
    raise NotImplementedError("Task #71: 设备状态数据处理")
