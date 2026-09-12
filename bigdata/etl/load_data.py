"""
Load Data — Phase 2

Owner(s): 王清香, 薛学刚
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
    [Task #61] 大数据可视化大屏（Web端） / 数据导入 / 充电订单数据导入
    Owner: 王清香

    读取教师提供的nvv2t.csv数据集，导入充电会话、充电电量、充电费用、充电时间、用户及充电站等数据，为大数据可视化提供数据来源。
    """
    # TODO: implement
    raise NotImplementedError("Task #61: 充电订单数据导入")


def load_stations(path=None) -> pd.DataFrame:
    """
    [Task #62] 大数据可视化大屏（Web端） / 数据导入 / 充电站数据导入
    Owner: 薛学刚

    读取教师提供的nvv2t_md_end.csv数据集，获取充电站ID、站点名称、地址、电桩数量、开放时间等信息，
    清洗后返回DataFrame并保存到data/processed/stations.csv。

    字段映射：
        stationId    -> 充电站ID
        station_name -> 站点名称
        address      -> 地址
        device_count -> 电桩数量
        open_time    -> 开放时间
    """
    path = Path(path) if path else RAW_DIR / "nvv2t_md_end.csv"
    if not path.exists():
        raise FileNotFoundError(f"充电站数据文件不存在: {path}")

    df = pd.read_csv(path)
    df.columns = df.columns.str.strip()

    for col in ("stationId", "locationId", "device_count"):
        df[col] = pd.to_numeric(df[col], errors="coerce")
    for col in df.select_dtypes(include="object").columns:
        df[col] = df[col].str.strip()

    df = df[df["stationId"].notna()]
    df = df[df["station_name"].notna() & (df["station_name"] != "")]
    df = df.drop_duplicates(subset=["stationId"])
    df = df.sort_values("stationId").reset_index(drop=True)

    _save_processed(df, "stations.csv")
    return df


def load_battery_telemetry(path=None) -> pd.DataFrame:
    """
    [Task #63] 大数据可视化大屏（Web端） / 数据导入 / 电池运行数据导入
    Owner: 薛学刚

    读取教师提供的dsv13r2.csv数据集，获取SOC、电池电压、充电电流、电池温度、可用能量及可用容量等
    车辆运行数据，清洗后返回DataFrame并保存到data/processed/battery_telemetry.csv。

    字段映射：
        soc                   -> 荷电状态SOC
        pack_voltage          -> 电池组电压
        charge_current        -> 充电电流
        max/min_temperature   -> 电池最高/最低温度
        available_energy      -> 可用能量
        available_capacity    -> 可用容量
    """
    path = Path(path) if path else RAW_DIR / "dsv13r2.csv"
    if not path.exists():
        raise FileNotFoundError(f"电池运行数据文件不存在: {path}")

    df = pd.read_csv(path)
    df.columns = df.columns.str.strip()

    rename_map = {
        "pack_voltage (V)": "pack_voltage",
        "charge_current (A)": "charge_current",
        "max_cell_voltage (V)": "max_cell_voltage",
        "min_cell_voltage (V)": "min_cell_voltage",
        "max_temperature (℃)": "max_temperature",
        "min_temperature (℃)": "min_temperature",
        "available_energy (kw)": "available_energy",
        "available_capacity (Ah)": "available_capacity",
    }
    df = df.rename(columns=rename_map)

    df["esd"] = pd.to_numeric(df["esd"], errors="coerce")
    df["record_time"] = pd.to_datetime(df["record_time"], unit="ms", errors="coerce")

    numeric_cols = [
        "soc", "pack_voltage", "charge_current", "max_cell_voltage",
        "min_cell_voltage", "max_temperature", "min_temperature",
        "available_energy", "available_capacity",
    ]
    for col in numeric_cols:
        df[col] = pd.to_numeric(df[col], errors="coerce")

    df = df[df["esd"].notna()]
    df = df[df["soc"].notna() & df["pack_voltage"].notna() & df["available_capacity"].notna()]
    df = df[(df["soc"] >= 0) & (df["soc"] <= 100)]
    df = df.drop_duplicates()
    df = df.sort_values(["esd", "record_time"]).reset_index(drop=True)

    _save_processed(df, "battery_telemetry.csv")
    return df


