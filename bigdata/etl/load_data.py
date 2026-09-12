"""
Load Data — Phase 2 stub

Owner(s): 王清香, 薛学刚
"""

import pandas as pd


def load_charging_orders(*args, **kwargs):
    """
    [Task #61] 大数据可视化大屏（Web端） / 数据导入 / 充电订单数据导入
    Owner: 王清香

    读取教师提供的nvv2t.csv数据集，导入充电会话、充电电量、充电费用、充电时间、用户及充电站等数据，为大数据可视化提供数据来源。
    """
    # TODO: implement
    raise NotImplementedError("Task #61: 充电订单数据导入")


def load_stations(*args, **kwargs):
    """
    [Task #62] 大数据可视化大屏（Web端） / 数据导入 / 充电站数据导入
    Owner: 薛学刚

    读取教师提供的nvv2t_md_end.csv数据集，获取充电站ID、站点名称、地址、电桩数量、开放时间等信息。
    """
    # TODO: implement
    raise NotImplementedError("Task #62: 充电站数据导入")


def load_battery_telemetry(*args, **kwargs):
    """
    [Task #63] 大数据可视化大屏（Web端） / 数据导入 / 电池运行数据导入
    Owner: 薛学刚

    读取教师提供的dsv13r2.csv数据集，获取SOC、电池电压、充电电流、电池温度、可用能量及可用容量等车辆运行数据。
    """
    # TODO: implement
    raise NotImplementedError("Task #63: 电池运行数据导入")


