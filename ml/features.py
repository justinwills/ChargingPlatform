"""
Features — Phase 2 stub

Owner(s): 王清香, 薛学刚
"""

import pandas as pd


def select_features(*args, **kwargs):
    """
    [Task #83] 机器学习智能分析子系统 / 数据准备 / 特征选择
    Owner: 王清香

    从教师提供的nvv2t.csv中选择充电量、充电时长、开始时间、星期、充电站等特征，用于建立充电需求预测模型。
    """
    # TODO: implement
    raise NotImplementedError("Task #83: 特征选择")


def build_time_features(*args, **kwargs):
    """
    [Task #84] 机器学习智能分析子系统 / 数据准备 / 时间特征工程
    Owner: 王清香

    从充电时间中提取小时、星期、是否周末等特征，并转换为机器学习模型可使用的数值特征。
    """
    # TODO: implement
    raise NotImplementedError("Task #84: 时间特征工程")


def build_historical_load(*args, **kwargs):
    """
    [Task #85] 机器学习智能分析子系统 / 数据准备 / 历史负荷构造
    Owner: 薛学刚

    根据充电会话的kwhTotal按照时间窗口聚合，构造小时级或站点级历史充电负荷数据。
    """
    # TODO: implement
    raise NotImplementedError("Task #85: 历史负荷构造")


def build_lag_features(*args, **kwargs):
    """
    [Task #86] 机器学习智能分析子系统 / 数据准备 / 滞后特征构造
    Owner: 薛学刚

    根据历史负荷生成前1小时、前2小时、前3小时、前24小时等滞后特征，用于时序负荷预测。
    """
    # TODO: implement
    raise NotImplementedError("Task #86: 滞后特征构造")


