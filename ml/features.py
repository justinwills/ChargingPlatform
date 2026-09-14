"""
Machine-learning data-preparation task scaffold.

The updated tasklist uses charging_orders.csv, stations.csv, devices.csv,
users.csv, weather_hourly.csv, and device_status_log.csv.
"""

import pandas as pd


def build_training_dataset(*args, **kwargs):
    """
    [Task #92] 机器学习智能分析子系统 / 数据准备 / 多源数据整合
    Owner: 王清香

    对 charging_orders.csv、stations.csv、devices.csv、users.csv、
    weather_hourly.csv 和 device_status_log.csv 进行关联，为充电负荷预测
    构建统一训练数据集。
    """
    # TODO: implement
    raise NotImplementedError("Task #92: 多源数据整合")


def select_features(*args, **kwargs):
    """
    [Task #93] 机器学习智能分析子系统 / 数据准备 / 特征选择
    Owner: 王清香

    从订单、充电站、充电设备、用户及天气数据中选择充电量、充电时长、
    开始时间、星期、节假日、天气、站点及设备运行状态等特征。
    """
    # TODO: implement
    raise NotImplementedError("Task #93: 特征选择")


def build_time_features(*args, **kwargs):
    """
    [Task #94] 机器学习智能分析子系统 / 数据准备 / 时间特征工程
    Owner: 王清香

    从 created_at 中提取小时、星期、月份等时间特征，并使用 weekday、
    is_weekend、is_holiday 和 is_workday 表示不同时间类型。
    """
    # TODO: implement
    raise NotImplementedError("Task #94: 时间特征工程")


def build_weather_features(*args, **kwargs):
    """
    [Task #95] 机器学习智能分析子系统 / 数据准备 / 天气特征工程
    Owner: 王清香

    将天气类型、温度、湿度、降水量、风速、能见度及降雨状态转换为模型可使用的数值特征。
    """
    # TODO: implement
    raise NotImplementedError("Task #95: 天气特征工程")


def build_station_features(*args, **kwargs):
    """
    [Task #96] 机器学习智能分析子系统 / 数据准备 / 站点特征工程
    Owner: 王清香

    根据 station_id 关联站点设备数量、电价、服务费及站点状态等信息，
    构造站点级预测特征。
    """
    # TODO: implement
    raise NotImplementedError("Task #96: 站点特征工程")


def build_device_features(*args, **kwargs):
    """
    [Task #97] 机器学习智能分析子系统 / 数据准备 / 设备特征工程
    Owner: 王清香

    根据设备状态日志计算设备可用率、故障次数、健康评分、平均输出功率等设备运行特征。
    """
    # TODO: implement
    raise NotImplementedError("Task #97: 设备特征工程")


def build_user_vehicle_features(*args, **kwargs):
    """
    [Task #98] 机器学习智能分析子系统 / 数据准备 / 用户车辆特征工程
    Owner: 王清香

    根据用户数据提取车辆类型、电池容量、会员等级等用户及车辆相关特征。
    """
    # TODO: implement
    raise NotImplementedError("Task #98: 用户车辆特征工程")


def build_historical_load(*args, **kwargs):
    """
    [Task #99] 机器学习智能分析子系统 / 数据准备 / 历史负荷构造
    Owner: 薛学刚

    根据 energy_kwh 按小时或站点进行聚合，构造小时级及站点级历史充电负荷数据。
    """
    # TODO: implement
    raise NotImplementedError("Task #99: 历史负荷构造")


def build_lag_features(*args, **kwargs):
    """
    [Task #100] 机器学习智能分析子系统 / 数据准备 / 滞后特征构造
    Owner: 薛学刚

    根据历史负荷生成前1小时、前2小时、前3小时、前24小时等滞后特征。
    """
    # TODO: implement
    raise NotImplementedError("Task #100: 滞后特征构造")
