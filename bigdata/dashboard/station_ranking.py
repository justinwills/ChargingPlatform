"""
Station Ranking — Phase 2 implementation

Owner(s): 洪维斌
"""

import os
from functools import lru_cache

import pandas as pd

DATA_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(__file__))), "data", "raw")
STATIONS_CSV = os.path.join(DATA_DIR, "stations.csv")
DEVICES_CSV = os.path.join(DATA_DIR, "devices.csv")
ORDERS_CSV = os.path.join(DATA_DIR, "charging_orders.csv")


@lru_cache(maxsize=4)
def load_stations() -> pd.DataFrame:
    return pd.read_csv(STATIONS_CSV)


@lru_cache(maxsize=4)
def _devices() -> pd.DataFrame:
    return pd.read_csv(DEVICES_CSV)


def station_online_stats():
    """
    每个站点的电桩在线快照 (供大屏"充电站在线率"表格使用)。

    返回 [{station_id, name, pileCount, freePileCount, onlineRate}],
    其中 freePileCount 用在线电桩数近似空余桩, onlineRate 为在线电桩占比(%).
    """
    devices = _devices()
    stations = load_stations()
    status_map = {"online": "在用", "idle": "闲置", "fault": "故障"}
    devices = devices.copy()
    devices["status_cn"] = devices["current_status"].map(status_map).fillna(devices["current_status"])
    online_per = devices.groupby("station_id")["current_status"].apply(
        lambda s: float((s == "online").mean()) * 100).to_dict()

    out = []
    for _, s in stations.iterrows():
        sid = int(s["station_id"])
        pc = int(s["device_count"]) if not pd.isna(s["device_count"]) else 0
        rate = online_per.get(sid, 0.0)
        out.append({
            "station_id": sid,
            "name": str(s["station_name"]),
            "pileCount": pc,
            "freePileCount": int(round(pc * rate / 100.0)),
            "onlineRate": round(rate, 1),
        })
    return out


def station_ranking(days: int = None, df: pd.DataFrame = None, limit: int = 10):
    """
    [Task #73] 充电站排名
    按充电电量汇总各站点(近 days 天, 默认全周期), 返回电量/次数/营收排名 Top N。
    """
    orders = df if df is not None else pd.read_csv(
        ORDERS_CSV, parse_dates=["created_at"])
    if days:
        cutoff = orders["created_at"].max() - pd.Timedelta(days=days)
        orders = orders[orders["created_at"] >= cutoff]
    stations = load_stations()
    names = {int(s["station_id"]): str(s["station_name"]) for _, s in stations.iterrows()}

    g = orders.groupby("station_id").agg(
        energy_kwh=("energy_kwh", "sum"),
        sessions=("energy_kwh", "count"),
        revenue_cny=("fee_amount_cny", "sum"),
    ).sort_values("energy_kwh", ascending=False).head(limit)

    return [
        {
            "station_id": int(sid),
            "station_name": names.get(int(sid), str(sid)),
            "energy_kwh": round(float(r["energy_kwh"]), 3),
            "sessions": int(r["sessions"]),
            "revenue_cny": round(float(r["revenue_cny"]), 2),
        }
        for sid, r in g.iterrows()
    ]


def station_distribution():
    """[Task #76] 充电站分布: 站点元数据 + 在线率快照(大屏"充电站在线率"表格)。"""
    stations = load_stations()
    online = {o["station_id"]: o for o in station_online_stats()}
    out = []
    for _, s in stations.iterrows():
        sid = int(s["station_id"])
        o = online.get(sid, {})
        out.append({
            "station_id": sid,
            "station_name": str(s["station_name"]),
            "address": str(s["address"]) if not pd.isna(s["address"]) else "",
            "city": str(s["city"]) if not pd.isna(s["city"]) else "",
            "latitude": float(s["latitude"]),
            "longitude": float(s["longitude"]),
            "device_count": int(s["device_count"]) if not pd.isna(s["device_count"]) else 0,
            "station_status": str(s["station_status"]) if not pd.isna(s["station_status"]) else "",
            "name": o.get("name", str(s["station_name"])),
            "pileCount": o.get("pileCount", 0),
            "freePileCount": o.get("freePileCount", 0),
            "onlineRate": o.get("onlineRate", 0.0),
        })
    return out
