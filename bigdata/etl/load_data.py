"""
Load Data — Phase 2 stub

Owner(s): 王清香, 薛学刚
"""

from pathlib import Path

import pandas as pd


def load_charging_orders(input_path=None, output_path=None):
    """
    [Task #61] 大数据可视化大屏（Web端） / 数据导入 / 充电订单数据导入
    Owner: 王清香

    读取教师提供的nvv2t.csv数据集，导入充电会话、充电电量、充电费用、充电时间、用户及充电站等数据，为大数据可视化提供数据来源。
    """
    project_root = Path(__file__).resolve().parents[2]
    source_path = Path(input_path) if input_path else project_root / "data" / "raw" / "nvv2t.csv"
    destination_path = (
        Path(output_path)
        if output_path
        else project_root / "data" / "processed" / "charging_orders.csv"
    )

    if not source_path.is_file():
        raise FileNotFoundError(f"Charging orders CSV not found: {source_path}")

    orders = pd.read_csv(source_path, encoding="utf-8-sig")
    orders.columns = orders.columns.astype(str).str.strip()
    orders = orders.dropna(how="all").reset_index(drop=True)

    destination_path.parent.mkdir(parents=True, exist_ok=True)
    orders.to_csv(destination_path, index=False, encoding="utf-8")
    return orders


def load_stations(input_path=None, output_path=None):
    """
    [Task #62] 大数据可视化大屏（Web端） / 数据导入 / 充电站数据导入
    Owner: 薛学刚

    读取教师提供的nvv2t_md_end.csv数据集，获取充电站ID、站点名称、地址、电桩数量、开放时间等信息。
    """
    project_root = Path(__file__).resolve().parents[2]
    source_path = (
        Path(input_path)
        if input_path
        else project_root / "data" / "raw" / "nvv2t_md_end.csv"
    )
    destination_path = (
        Path(output_path)
        if output_path
        else project_root / "data" / "processed" / "charging_stations.csv"
    )

    if not source_path.is_file():
        raise FileNotFoundError(f"Charging stations CSV not found: {source_path}")

    stations = pd.read_csv(source_path, encoding="utf-8-sig")
    stations.columns = stations.columns.astype(str).str.strip()
    stations = stations.dropna(how="all").reset_index(drop=True)

    destination_path.parent.mkdir(parents=True, exist_ok=True)
    stations.to_csv(destination_path, index=False, encoding="utf-8")
    return stations


def load_battery_telemetry(*args, **kwargs):
    """
    [Task #63] 大数据可视化大屏（Web端） / 数据导入 / 电池运行数据导入
    Owner: 薛学刚

    读取教师提供的dsv13r2.csv数据集，获取SOC、电池电压、充电电流、电池温度、可用能量及可用容量等车辆运行数据。
    """
    # TODO: implement
    raise NotImplementedError("Task #63: 电池运行数据导入")


