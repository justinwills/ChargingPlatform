"""
Charging Stats — Phase 2 implementation

Owner(s): 邱辰笙
"""

import os
from functools import lru_cache

import pandas as pd

_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DATA_DIR = os.path.join(_ROOT, "data", "raw")
ORDERS_CSV = os.path.join(_ROOT, "data", "processed", "charging_orders.csv")


@lru_cache(maxsize=4)
def load_orders() -> pd.DataFrame:
    """读取并清洗充电订单(缓存)。"""
    df = pd.read_csv(ORDERS_CSV, parse_dates=["created_at", "ended_at"])
    df = df.sort_values("created_at").reset_index(drop=True)
    return df


def _slice_days(df: pd.DataFrame, days: int):
    last = df["created_at"].max()
    if days and days > 0:
        cutoff = last - pd.Timedelta(days=days)
        return df[df["created_at"] >= cutoff]
    return df


def revenue_today_this_month_total(df: pd.DataFrame = None):
    """[Task #66] 充电业务指标: 今日/近30日/总营收。"""
    if df is None:
        df = load_orders()
    if df.empty:
        return {"revenue_today": 0.0, "revenue_30d": 0.0, "revenue_total": 0.0}
    last = df["created_at"].max()
    today = df[df["created_at"].dt.date == last.date()]
    month = _slice_days(df, 30)
    return {
        "revenue_today": round(float(today["fee_amount_cny"].sum()), 2),
        "revenue_30d": round(float(month["fee_amount_cny"].sum()), 2),
        "revenue_total": round(float(df["fee_amount_cny"].sum()), 2),
    }


def charging_volume_trend(days: int = 7, df: pd.DataFrame = None):
    """[Task #70] 充电量趋势: 按天汇总充电电量。"""
    if df is None:
        df = load_orders()
    d = _slice_days(df, days)
    g = d.groupby(d["created_at"].dt.date)["energy_kwh"].sum()
    return [{"date": str(dt), "energy_kwh": round(float(v), 3)} for dt, v in g.items()]


def revenue_trend(days: int = 7, df: pd.DataFrame = None):
    """[Task #71] 营收趋势: 按天汇总充电费用。"""
    if df is None:
        df = load_orders()
    d = _slice_days(df, days)
    g = d.groupby(d["created_at"].dt.date)["fee_amount_cny"].sum()
    return [{"date": str(dt), "revenue": round(float(v), 2)} for dt, v in g.items()]


def session_count_trend(days: int = 7, df: pd.DataFrame = None):
    """[Task #72] 充电次数趋势: 按天统计会话数。"""
    if df is None:
        df = load_orders()
    d = _slice_days(df, days)
    g = d.groupby(d["created_at"].dt.date).size()
    return [{"date": str(dt), "count": int(v)} for dt, v in g.items()]


def hourly_distribution(days: int = None, df: pd.DataFrame = None):
    """[Task #74] 充电时段分析: 24小时充电电量与会话数分布(近 days 天, 默认全周期)。"""
    if df is None:
        df = load_orders()
    d = _slice_days(df, days)
    g = d.groupby(d["created_at"].dt.hour).agg(energy=("energy_kwh", "sum"),
                                               sessions=("energy_kwh", "count"))
    out = []
    for h in range(24):
        row = g.loc[h] if h in g.index else None
        out.append({"hour": h, "energy_kwh": round(float(row["energy"]), 3) if row is not None else 0.0,
                    "sessions": int(row["sessions"]) if row is not None else 0})
    return out


def weekday_heatmap(days: int = None, df: pd.DataFrame = None):
    """[Task #75] 星期热力图: 星期 x 小时 的充电会话数矩阵(近 days 天)。"""
    if df is None:
        df = load_orders()
    d = _slice_days(df, days).copy()
    d["dow"] = d["created_at"].dt.dayofweek
    d["hod"] = d["created_at"].dt.hour
    tab = d.pivot_table(index="dow", columns="hod", values="session_id", aggfunc="count", fill_value=0)
    rows = []
    for dow in range(7):
        for hod in range(24):
            rows.append({"weekday": int(dow), "hour": int(hod),
                         "count": int(tab.loc[dow, hod]) if dow in tab.index and hod in tab.columns else 0})
    return rows


def user_metrics(days: int = None, df: pd.DataFrame = None):
    """[Task #68] 用户指标: 活跃用户、人均充电量等(近 days 天)。"""
    if df is None:
        df = load_orders()
    d = _slice_days(df, days)
    if d.empty:
        return {"active_users": 0, "avg_energy_per_user_kwh": 0.0}
    g = d.groupby("user_id")
    return {
        "active_users": int(d["user_id"].nunique()),
        "avg_energy_per_user_kwh": round(float(g["energy_kwh"].sum().mean()), 3),
        "avg_sessions_per_user": round(float(g.size().mean()), 3),
    }


def weekday_pattern(days: int = None, df: pd.DataFrame = None):
    """[Task #69] 星期充电规律: 各星期充电电量合计(近 days 天, 默认全周期)。"""
    if df is None:
        df = load_orders()
    d = _slice_days(df, days)
    names = ["Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"]
    g = d.groupby(d["created_at"].dt.dayofweek)["energy_kwh"].sum()
    return [{"weekday": names[dw], "energy_kwh": round(float(g[dw]), 3)} for dw in range(7) if dw in g.index]


WEATHER_CSV = os.path.join(DATA_DIR, "weather_hourly.csv")


@lru_cache(maxsize=4)
def load_weather() -> pd.DataFrame:
    return pd.read_csv(WEATHER_CSV)


def weather_impact(days: int = None, df: pd.DataFrame = None, weather: pd.DataFrame = None):
    """[Task #80] 天气影响分析: 按天气类型聚合充电需求(近 days 天)。"""
    if df is None:
        df = load_orders()
    if weather is None:
        weather = load_weather()
    d = _slice_days(df, days)
    if d.empty or weather.empty:
        return []
    keep = ["weather_id", "weather_type", "weather_name", "temperature_c",
            "humidity_pct", "precipitation_mm"]
    m = d.merge(weather[keep], on="weather_id", how="left")
    g = m.groupby(["weather_type", "weather_name"]).agg(
        sessions=("session_id", "count"),
        energy_kwh=("energy_kwh", "sum"),
        fee_cny=("fee_amount_cny", "sum"),
        avg_temp_c=("temperature_c", "mean"),
        avg_precip_mm=("precipitation_mm", "mean"),
    ).reset_index().sort_values("sessions", ascending=False)
    return g.round(3).to_dict("records")


def overview(df: pd.DataFrame = None):
    """大屏 KPI 总览。"""
    if df is None:
        df = load_orders()
    if df.empty:
        return {}
    totals = revenue_today_this_month_total(df)
    days = (df["created_at"].max() - df["created_at"].min()).days + 1
    return {
        "total_sessions": int(len(df)),
        "total_energy_kwh": round(float(df["energy_kwh"].sum()), 3),
        **totals,
        "active_stations": int(df["station_id"].nunique()),
        "active_users": int(df["user_id"].nunique()),
        "avg_duration_hrs": round(float(df["charge_time_hrs"].mean()), 3),
        "avg_order_fee_cny": round(float(df["fee_amount_cny"].mean()), 3),
        "span_days": int(days),
    }