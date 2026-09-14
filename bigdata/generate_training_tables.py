"""Generate six connected CSV tables for charging-platform analytics and ML.

The generator uses the existing charging-session and station CSV files only to
calibrate station popularity, time-of-day behaviour, and station metadata.  It
does not overwrite the source files.  All generated rows are synthetic and are
written to a separate output directory.

Weather affects the Poisson demand process directly, so rainy station-hours
have fewer expected charging sessions than dry station-hours.  The fixed random
seed makes the generated dataset reproducible.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import numpy as np
import pandas as pd


WEATHER_DEMAND_FACTORS = {
    "sunny": 1.00,
    "cloudy": 0.94,
    "overcast": 0.84,
    "fog": 0.70,
    "light_rain": 0.64,
    "heavy_rain": 0.36,
    "snow": 0.48,
}

WEATHER_LABELS_ZH = {
    "sunny": "晴",
    "cloudy": "多云",
    "overcast": "阴",
    "fog": "雾",
    "light_rain": "小雨",
    "heavy_rain": "大雨",
    "snow": "雪",
}

WEEKDAY_NAMES = ("Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun")

# The source period spans late 2014 through 2015.  These ranges follow the
# official 2015 holiday arrangement, including the September 3 anniversary
# holiday.  Weekend days attached to a holiday block are labelled as part of
# the holiday period because they behave as continuous days off for demand.
HOLIDAY_PERIODS_2015 = (
    ("2015-01-01", "2015-01-03", "元旦"),
    ("2015-02-18", "2015-02-24", "春节"),
    ("2015-04-04", "2015-04-06", "清明节"),
    ("2015-05-01", "2015-05-03", "劳动节"),
    ("2015-06-20", "2015-06-22", "端午节"),
    ("2015-09-03", "2015-09-05", "抗战胜利纪念日"),
    ("2015-09-26", "2015-09-27", "中秋节"),
    ("2015-10-01", "2015-10-07", "国庆节"),
)

MAKEUP_WORKDAYS_2015 = {
    "2015-01-04",
    "2015-02-15",
    "2015-02-28",
    "2015-09-06",
    "2015-10-10",
}


def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--orders", required=True, help="Existing nvv2t.csv path")
    parser.add_argument(
        "--stations", required=True, help="Existing nvv2t_md_end.csv path"
    )
    parser.add_argument("--output-dir", required=True)
    parser.add_argument("--target-orders", type=int, default=24000)
    parser.add_argument("--users", type=int, default=600)
    parser.add_argument("--seed", type=int, default=20260914)
    return parser.parse_args()


def _timestamp_text(series: pd.Series) -> pd.Series:
    return pd.to_datetime(series).dt.strftime("%Y-%m-%d %H:%M:%S")


def _date_text(series: pd.Series) -> pd.Series:
    return pd.to_datetime(series).dt.strftime("%Y-%m-%d")


def _calendar_features(values: pd.Series) -> pd.DataFrame:
    dates = pd.to_datetime(values).dt.normalize()
    date_text = dates.dt.strftime("%Y-%m-%d")
    holiday_name = pd.Series("非节假日", index=values.index, dtype="object")
    for start, end, name in HOLIDAY_PERIODS_2015:
        mask = dates.between(pd.Timestamp(start), pd.Timestamp(end))
        holiday_name.loc[mask] = name
    is_weekend = dates.dt.weekday.ge(5)
    is_holiday = holiday_name.ne("非节假日")
    is_makeup_workday = date_text.isin(MAKEUP_WORKDAYS_2015)
    is_workday = ((~is_weekend) & (~is_holiday)) | is_makeup_workday
    return pd.DataFrame(
        {
            "is_weekend": is_weekend.astype("int8"),
            "is_holiday": is_holiday.astype("int8"),
            "holiday_name": holiday_name,
            "is_workday": is_workday.astype("int8"),
        },
        index=values.index,
    )


def _weather_probabilities(month: int) -> tuple[list[str], list[float]]:
    conditions = [
        "sunny",
        "cloudy",
        "overcast",
        "fog",
        "light_rain",
        "heavy_rain",
        "snow",
    ]
    if month in (6, 7, 8):
        probabilities = [0.30, 0.24, 0.14, 0.02, 0.20, 0.10, 0.00]
    elif month in (12, 1, 2):
        probabilities = [0.43, 0.25, 0.13, 0.09, 0.06, 0.01, 0.03]
    else:
        probabilities = [0.39, 0.27, 0.14, 0.05, 0.11, 0.04, 0.00]
    return conditions, probabilities


def build_stations(
    source_stations: pd.DataFrame,
    rng: np.random.Generator,
) -> pd.DataFrame:
    stations = source_stations.rename(
        columns={
            "stationId": "station_id",
            "locationId": "location_id",
            "facilityType": "facility_type",
            "update_time": "updated_on",
        }
    ).copy()
    stations["station_id"] = pd.to_numeric(stations["station_id"]).astype("int64")
    stations["location_id"] = pd.to_numeric(stations["location_id"]).astype("int64")
    stations["facility_type"] = pd.to_numeric(
        stations["facility_type"], errors="coerce"
    ).fillna(1).astype("int64")
    stations["device_count"] = pd.to_numeric(
        stations["device_count"], errors="coerce"
    ).fillna(1).clip(lower=1).astype("int64")

    location_ids = sorted(stations["location_id"].unique())
    location_coordinates = {
        location_id: (
            34.7466 + rng.uniform(-0.115, 0.115),
            113.6254 + rng.uniform(-0.145, 0.145),
        )
        for location_id in location_ids
    }
    stations["latitude"] = [
        location_coordinates[value][0] + rng.normal(0, 0.004)
        for value in stations["location_id"]
    ]
    stations["longitude"] = [
        location_coordinates[value][1] + rng.normal(0, 0.004)
        for value in stations["location_id"]
    ]
    stations["city"] = "郑州市"
    stations["electricity_price_cny_kwh"] = np.round(
        rng.uniform(0.58, 0.92, len(stations)), 2
    )
    stations["service_fee_cny_kwh"] = np.round(
        rng.uniform(0.18, 0.45, len(stations)), 2
    )
    stations["station_status"] = "active"
    stations["updated_on"] = pd.to_datetime(
        stations["updated_on"], dayfirst=True, errors="coerce"
    ).dt.strftime("%Y-%m-%d")
    stations["latitude"] = stations["latitude"].round(6)
    stations["longitude"] = stations["longitude"].round(6)

    return stations[
        [
            "station_id",
            "location_id",
            "station_name",
            "address",
            "city",
            "latitude",
            "longitude",
            "facility_type",
            "device_count",
            "open_time",
            "electricity_price_cny_kwh",
            "service_fee_cny_kwh",
            "station_status",
            "updated_on",
        ]
    ].sort_values("station_id", ignore_index=True)


def build_devices(
    stations: pd.DataFrame,
    start_date: pd.Timestamp,
    end_date: pd.Timestamp,
    rng: np.random.Generator,
) -> pd.DataFrame:
    rows: list[dict[str, object]] = []
    manufacturers = ("星星充电", "特来电", "华为数字能源", "盛弘电气")
    for station in stations.itertuples(index=False):
        fast_share = float(np.clip(0.28 + 0.06 * station.facility_type, 0.30, 0.68))
        for sequence in range(1, int(station.device_count) + 1):
            charger_type = "DC_FAST" if rng.random() < fast_share else "AC_SLOW"
            if charger_type == "DC_FAST":
                rated_power = int(rng.choice([60, 80, 120, 160], p=[0.30, 0.30, 0.30, 0.10]))
            else:
                rated_power = int(rng.choice([7, 11, 22], p=[0.62, 0.25, 0.13]))
            commissioned_at = start_date - pd.Timedelta(
                days=int(rng.integers(60, 5 * 365))
            )
            last_maintenance = end_date - pd.Timedelta(
                days=int(rng.integers(5, 180))
            )
            device_id = f"D{int(station.station_id):07d}-{sequence:02d}"
            rows.append(
                {
                    "device_id": device_id,
                    "station_id": int(station.station_id),
                    "device_code": f"PILE-{int(station.station_id):07d}-{sequence:02d}",
                    "charger_type": charger_type,
                    "rated_power_kw": rated_power,
                    "connector_count": int(rng.choice([1, 2], p=[0.72, 0.28])),
                    "manufacturer": str(rng.choice(manufacturers)),
                    "commissioned_at": commissioned_at,
                    "last_maintenance_at": last_maintenance,
                    "current_status": "online",
                }
            )
    devices = pd.DataFrame(rows)
    devices["commissioned_at"] = _date_text(devices["commissioned_at"])
    devices["last_maintenance_at"] = _date_text(devices["last_maintenance_at"])
    return devices


def build_users(
    source_orders: pd.DataFrame,
    user_count: int,
    start_date: pd.Timestamp,
    end_date: pd.Timestamp,
    rng: np.random.Generator,
) -> tuple[pd.DataFrame, np.ndarray]:
    existing_ids = [str(value) for value in source_orders["userId"].dropna().unique()]
    user_count = max(user_count, len(existing_ids))
    existing_set = set(existing_ids)
    generated_ids: list[str] = []
    candidate = 90000001
    while len(existing_ids) + len(generated_ids) < user_count:
        value = str(candidate)
        if value not in existing_set:
            generated_ids.append(value)
        candidate += 1
    user_ids = existing_ids + generated_ids

    period_days = max(1, (end_date - start_date).days)
    registration_offsets = np.where(
        rng.random(user_count) < 0.76,
        -rng.integers(30, 730, user_count),
        rng.integers(0, max(1, int(period_days * 0.82)), user_count),
    )
    registered_at = start_date + pd.to_timedelta(registration_offsets, unit="D")

    activity_weights = rng.lognormal(mean=0.0, sigma=0.9, size=user_count)
    activity_quantiles = pd.qcut(
        activity_weights,
        q=[0, 0.52, 0.78, 0.94, 1.0],
        labels=["bronze", "silver", "gold", "platinum"],
    ).astype(str)
    vehicle_types = rng.choice(
        ["sedan", "suv", "mpv", "commercial"],
        size=user_count,
        p=[0.50, 0.34, 0.09, 0.07],
    )
    capacity_ranges = {
        "sedan": (42, 76),
        "suv": (55, 100),
        "mpv": (50, 90),
        "commercial": (65, 130),
    }
    battery_capacity = np.array(
        [rng.uniform(*capacity_ranges[value]) for value in vehicle_types]
    )
    preferred_platform = rng.choice(
        ["android", "ios", "web"], size=user_count, p=[0.60, 0.34, 0.06]
    )
    users = pd.DataFrame(
        {
            "user_id": user_ids,
            "registered_at": registered_at,
            "city": "郑州市",
            "member_level": activity_quantiles,
            "vehicle_type": vehicle_types,
            "battery_capacity_kwh": np.round(battery_capacity, 1),
            "preferred_platform": preferred_platform,
            "registration_channel": rng.choice(
                ["app", "wechat", "station_qr", "partner"],
                size=user_count,
                p=[0.45, 0.30, 0.18, 0.07],
            ),
            "user_status": rng.choice(
                ["active", "inactive"], size=user_count, p=[0.97, 0.03]
            ),
        }
    )
    users["registered_at"] = _timestamp_text(users["registered_at"])
    return users, activity_weights


def build_weather(
    stations: pd.DataFrame,
    start_date: pd.Timestamp,
    end_date: pd.Timestamp,
    rng: np.random.Generator,
) -> pd.DataFrame:
    dates = pd.date_range(start_date, end_date, freq="D")
    rows: list[pd.DataFrame] = []
    for location_id in sorted(stations["location_id"].unique()):
        location_offset = rng.normal(0, 0.7)
        daily_conditions: list[str] = []
        for date_value in dates:
            choices, probabilities = _weather_probabilities(int(date_value.month))
            daily_conditions.append(str(rng.choice(choices, p=probabilities)))
        day_frame = pd.DataFrame(
            {
                "location_id": int(location_id),
                "date": dates,
                "weather_type": daily_conditions,
            }
        )
        expanded = day_frame.loc[day_frame.index.repeat(24)].reset_index(drop=True)
        expanded["hour"] = np.tile(np.arange(24), len(day_frame))
        expanded["weather_time"] = expanded["date"] + pd.to_timedelta(
            expanded["hour"], unit="h"
        )

        day_of_year = expanded["weather_time"].dt.dayofyear.to_numpy()
        hour = expanded["hour"].to_numpy()
        seasonal = 16.0 + 13.0 * np.sin(2 * np.pi * (day_of_year - 105) / 365.25)
        diurnal = 4.8 * np.sin(2 * np.pi * (hour - 9) / 24)
        condition_adjustment = expanded["weather_type"].map(
            {
                "sunny": 1.6,
                "cloudy": 0.2,
                "overcast": -0.8,
                "fog": -1.2,
                "light_rain": -1.5,
                "heavy_rain": -2.5,
                "snow": -4.0,
            }
        ).to_numpy()
        temperature = (
            seasonal
            + diurnal
            + condition_adjustment
            + location_offset
            + rng.normal(0, 0.9, len(expanded))
        )

        humidity_base = expanded["weather_type"].map(
            {
                "sunny": 45,
                "cloudy": 58,
                "overcast": 67,
                "fog": 91,
                "light_rain": 88,
                "heavy_rain": 94,
                "snow": 85,
            }
        ).to_numpy()
        humidity = np.clip(humidity_base + rng.normal(0, 5, len(expanded)), 22, 100)
        precipitation = np.zeros(len(expanded))
        light_mask = expanded["weather_type"].to_numpy() == "light_rain"
        heavy_mask = expanded["weather_type"].to_numpy() == "heavy_rain"
        snow_mask = expanded["weather_type"].to_numpy() == "snow"
        precipitation[light_mask] = rng.gamma(1.2, 0.45, light_mask.sum())
        precipitation[heavy_mask] = rng.gamma(2.2, 1.45, heavy_mask.sum())
        precipitation[snow_mask] = rng.gamma(1.0, 0.30, snow_mask.sum())

        wind = np.clip(
            rng.gamma(2.0, 1.1, len(expanded))
            + np.where(heavy_mask, 2.0, 0.0),
            0.1,
            13.0,
        )
        visibility = expanded["weather_type"].map(
            {
                "sunny": 18.0,
                "cloudy": 14.0,
                "overcast": 11.0,
                "fog": 2.2,
                "light_rain": 8.0,
                "heavy_rain": 4.5,
                "snow": 5.5,
            }
        ).to_numpy() + rng.normal(0, 0.8, len(expanded))

        expanded["weather_name"] = expanded["weather_type"].map(WEATHER_LABELS_ZH)
        expanded["temperature_c"] = np.round(temperature, 1)
        expanded["humidity_pct"] = np.round(humidity, 1)
        expanded["precipitation_mm"] = np.round(precipitation, 2)
        expanded["wind_speed_mps"] = np.round(wind, 1)
        expanded["visibility_km"] = np.round(np.clip(visibility, 0.3, 25), 1)
        expanded["is_rain"] = expanded["weather_type"].isin(
            ["light_rain", "heavy_rain"]
        ).astype("int8")
        expanded["weather_id"] = (
            "W"
            + expanded["location_id"].astype(str)
            + "-"
            + expanded["weather_time"].dt.strftime("%Y%m%d%H")
        )
        rows.append(
            expanded[
                [
                    "weather_id",
                    "location_id",
                    "weather_time",
                    "weather_type",
                    "weather_name",
                    "temperature_c",
                    "humidity_pct",
                    "precipitation_mm",
                    "wind_speed_mps",
                    "visibility_km",
                    "is_rain",
                ]
            ]
        )
    weather = pd.concat(rows, ignore_index=True)
    weather["weather_time"] = _timestamp_text(weather["weather_time"])
    return weather.sort_values(["location_id", "weather_time"], ignore_index=True)


def _calibrated_factors(
    source_orders: pd.DataFrame,
    stations: pd.DataFrame,
    start_date: pd.Timestamp,
    end_date: pd.Timestamp,
) -> tuple[dict[int, float], np.ndarray, np.ndarray]:
    source = source_orders.copy()
    source["created_at"] = pd.to_datetime(source["created"], dayfirst=True, errors="coerce")
    source = source.dropna(subset=["created_at"])

    station_counts = source.groupby("stationId").size()
    smoothed = stations["station_id"].map(station_counts).fillna(0).astype(float) + 4.0
    station_factor_values = np.clip(smoothed / smoothed.mean(), 0.25, 4.0)
    station_factors = dict(zip(stations["station_id"], station_factor_values))

    hour_counts = source["created_at"].dt.hour.value_counts().reindex(range(24), fill_value=0)
    hour_factors = np.clip((hour_counts + 3) / (hour_counts + 3).mean(), 0.22, 2.8)

    date_index = pd.date_range(start_date, end_date, freq="D")
    available_days = pd.Series(date_index.weekday).value_counts().reindex(range(7), fill_value=1)
    weekday_counts = source["created_at"].dt.weekday.value_counts().reindex(range(7), fill_value=0)
    per_day = (weekday_counts + 1) / available_days
    weekday_factors = np.clip(per_day / per_day.mean(), 0.72, 1.35)
    return station_factors, hour_factors.to_numpy(), weekday_factors.to_numpy()


def build_orders(
    source_orders: pd.DataFrame,
    stations: pd.DataFrame,
    devices: pd.DataFrame,
    users: pd.DataFrame,
    user_activity_weights: np.ndarray,
    weather: pd.DataFrame,
    target_orders: int,
    start_date: pd.Timestamp,
    end_date: pd.Timestamp,
    rng: np.random.Generator,
) -> tuple[pd.DataFrame, pd.DataFrame]:
    hours = pd.date_range(start_date, end_date + pd.Timedelta(hours=23), freq="h")
    grid = pd.MultiIndex.from_product(
        [stations["station_id"].to_numpy(), hours],
        names=["station_id", "weather_time"],
    ).to_frame(index=False)
    grid = grid.merge(
        stations[["station_id", "location_id", "device_count"]],
        on="station_id",
        how="left",
        validate="many_to_one",
    )
    calendar = _calendar_features(grid["weather_time"])
    for column in calendar.columns:
        grid[column] = calendar[column]
    weather_for_join = weather.copy()
    weather_for_join["weather_time"] = pd.to_datetime(weather_for_join["weather_time"])
    grid = grid.merge(
        weather_for_join[
            [
                "weather_id",
                "location_id",
                "weather_time",
                "weather_type",
                "temperature_c",
            ]
        ],
        on=["location_id", "weather_time"],
        how="left",
        validate="many_to_one",
    )

    station_factors, hour_factors, weekday_factors = _calibrated_factors(
        source_orders, stations, start_date, end_date
    )
    station_factor = grid["station_id"].map(station_factors).to_numpy()
    capacity_factor = np.sqrt(
        grid["device_count"].to_numpy() / stations["device_count"].mean()
    )
    hour_factor = hour_factors[grid["weather_time"].dt.hour.to_numpy()]
    weekday_factor = weekday_factors[grid["weather_time"].dt.weekday.to_numpy()]
    weather_factor = grid["weather_type"].map(WEATHER_DEMAND_FACTORS).to_numpy()
    temperature = grid["temperature_c"].to_numpy()
    comfort_factor = np.where(
        temperature < -5,
        0.72,
        np.where(temperature > 36, 0.78, np.where(temperature > 32, 0.90, 1.0)),
    )
    calendar_factor = np.where(
        grid["is_holiday"].to_numpy() == 1,
        0.72,
        np.where(
            (grid["is_weekend"].to_numpy() == 1)
            & (grid["is_workday"].to_numpy() == 1),
            1.03,
            1.0,
        ),
    )
    raw_score = (
        station_factor
        * np.clip(capacity_factor, 0.55, 2.2)
        * hour_factor
        * weekday_factor
        * weather_factor
        * comfort_factor
        * calendar_factor
    )
    base_lambda = float(target_orders) / raw_score.sum()
    grid["expected_orders"] = raw_score * base_lambda
    grid["order_count"] = rng.poisson(grid["expected_orders"].to_numpy())

    events = grid.loc[grid.index.repeat(grid["order_count"])].copy()
    events["created_at"] = events["weather_time"] + pd.to_timedelta(
        rng.integers(0, 60, len(events)), unit="m"
    )
    events = events.sort_values("created_at", ignore_index=True)

    device_rows = devices.set_index("device_id").to_dict("index")
    devices_by_station = {
        int(station_id): group["device_id"].tolist()
        for station_id, group in devices.groupby("station_id")
    }
    available_at = {
        device_id: start_date - pd.Timedelta(days=1)
        for device_id in devices["device_id"]
    }
    station_prices = stations.set_index("station_id")[
        ["electricity_price_cny_kwh", "service_fee_cny_kwh"]
    ].to_dict("index")

    active_users = users.loc[users["user_status"] == "active"].copy()
    active_users["registered_at_dt"] = pd.to_datetime(active_users["registered_at"])
    active_users["activity_weight"] = user_activity_weights[active_users.index]
    active_users = active_users.sort_values("registered_at_dt", ignore_index=True)
    cumulative_weights = active_users["activity_weight"].cumsum().to_numpy()

    order_rows: list[dict[str, object]] = []
    for event in events.itertuples(index=False):
        created_at = pd.Timestamp(event.created_at)
        station_id = int(event.station_id)
        free_devices = [
            device_id
            for device_id in devices_by_station[station_id]
            if available_at[device_id] <= created_at
        ]
        if not free_devices:
            continue
        preferred_type = "DC_FAST" if rng.random() < 0.58 else "AC_SLOW"
        preferred = [
            device_id
            for device_id in free_devices
            if device_rows[device_id]["charger_type"] == preferred_type
        ]
        device_id = str(rng.choice(preferred or free_devices))
        device = device_rows[device_id]

        eligible_count = int(
            np.searchsorted(
                active_users["registered_at_dt"].to_numpy(),
                np.datetime64(created_at),
                side="right",
            )
        )
        if eligible_count == 0:
            continue
        draw = rng.random() * cumulative_weights[eligible_count - 1]
        user_position = int(np.searchsorted(cumulative_weights, draw, side="right"))
        user = active_users.iloc[min(user_position, eligible_count - 1)]

        if device["charger_type"] == "DC_FAST":
            duration_hours = float(np.clip(rng.lognormal(-0.10, 0.42), 0.25, 2.6))
        else:
            duration_hours = float(np.clip(rng.lognormal(1.05, 0.48), 0.60, 8.5))
        ended_at = created_at + pd.to_timedelta(duration_hours, unit="h")
        available_at[device_id] = ended_at + pd.Timedelta(minutes=5)

        potential_energy = (
            float(device["rated_power_kw"])
            * duration_hours
            * rng.uniform(0.50, 0.82)
        )
        battery_limit = float(user["battery_capacity_kwh"]) * rng.uniform(0.20, 0.82)
        energy_kwh = float(np.clip(min(potential_energy, battery_limit), 1.0, 105.0))
        order_status = str(
            rng.choice(
                ["completed", "interrupted", "failed"],
                p=[0.965, 0.025, 0.010],
            )
        )
        if order_status == "failed":
            energy_kwh *= rng.uniform(0.03, 0.22)
        elif order_status == "interrupted":
            energy_kwh *= rng.uniform(0.45, 0.82)

        price = station_prices[station_id]
        fee_amount = energy_kwh * (
            float(price["electricity_price_cny_kwh"])
            + float(price["service_fee_cny_kwh"])
        )
        if order_status == "failed":
            payment_status = "waived"
            fee_amount = 0.0
        elif order_status == "interrupted" and rng.random() < 0.25:
            payment_status = "refunded"
            fee_amount = 0.0
        else:
            payment_status = "paid" if rng.random() < 0.985 else "pending"

        order_rows.append(
            {
                "session_id": f"SYN{len(order_rows) + 1:09d}",
                "user_id": str(user["user_id"]),
                "station_id": station_id,
                "device_id": device_id,
                "location_id": int(event.location_id),
                "weather_id": str(event.weather_id),
                "created_at": created_at,
                "ended_at": ended_at,
                "charge_time_hrs": round(duration_hours, 3),
                "start_hour": int(created_at.hour),
                "weekday": int(created_at.weekday()),
                "weekday_name": WEEKDAY_NAMES[created_at.weekday()],
                "is_weekend": int(event.is_weekend),
                "is_holiday": int(event.is_holiday),
                "holiday_name": str(event.holiday_name),
                "is_workday": int(event.is_workday),
                "energy_kwh": round(energy_kwh, 3),
                "fee_amount_cny": round(fee_amount, 2),
                "platform": str(user["preferred_platform"]),
                "order_status": order_status,
                "payment_status": payment_status,
            }
        )

    orders = pd.DataFrame(order_rows)
    orders["created_at"] = _timestamp_text(orders["created_at"])
    orders["ended_at"] = _timestamp_text(orders["ended_at"])
    return orders, grid


def build_device_status_log(
    devices: pd.DataFrame,
    stations: pd.DataFrame,
    orders: pd.DataFrame,
    weather: pd.DataFrame,
    start_date: pd.Timestamp,
    end_date: pd.Timestamp,
    rng: np.random.Generator,
) -> tuple[pd.DataFrame, pd.DataFrame]:
    dates = pd.date_range(start_date, end_date, freq="D")
    status = pd.MultiIndex.from_product(
        [devices["device_id"].to_numpy(), dates],
        names=["device_id", "stat_date"],
    ).to_frame(index=False)
    status = status.merge(
        devices[["device_id", "station_id", "rated_power_kw", "commissioned_at"]],
        on="device_id",
        validate="many_to_one",
    ).merge(
        stations[["station_id", "location_id"]],
        on="station_id",
        validate="many_to_one",
    )

    order_facts = orders.copy()
    order_facts["stat_date"] = pd.to_datetime(order_facts["created_at"]).dt.normalize()
    order_facts["successful"] = (order_facts["order_status"] == "completed").astype(int)
    order_facts["failed"] = (order_facts["order_status"] == "failed").astype(int)
    order_facts["output_power"] = (
        order_facts["energy_kwh"] / order_facts["charge_time_hrs"]
    )
    order_daily = order_facts.groupby(["device_id", "stat_date"], as_index=False).agg(
        successful_sessions=("successful", "sum"),
        failed_sessions=("failed", "sum"),
        total_sessions=("session_id", "count"),
        avg_output_power_kw=("output_power", "mean"),
    )
    status = status.merge(
        order_daily,
        on=["device_id", "stat_date"],
        how="left",
        validate="one_to_one",
    )
    for column in ("successful_sessions", "failed_sessions", "total_sessions"):
        status[column] = status[column].fillna(0).astype(int)
    status["avg_output_power_kw"] = status["avg_output_power_kw"].fillna(0.0)

    weather_daily = weather.copy()
    weather_daily["stat_date"] = pd.to_datetime(weather_daily["weather_time"]).dt.normalize()
    weather_daily = weather_daily.groupby(["location_id", "stat_date"], as_index=False).agg(
        daily_precipitation_mm=("precipitation_mm", "sum"),
        daily_avg_humidity_pct=("humidity_pct", "mean"),
        daily_avg_temperature_c=("temperature_c", "mean"),
    )
    status = status.merge(
        weather_daily,
        on=["location_id", "stat_date"],
        how="left",
        validate="many_to_one",
    )

    commissioned = pd.to_datetime(status["commissioned_at"])
    age_years = (status["stat_date"] - commissioned).dt.days.clip(lower=0) / 365.25
    precipitation_risk = (status["daily_precipitation_mm"] >= 5).astype(float)
    humidity_risk = (status["daily_avg_humidity_pct"] >= 85).astype(float)
    utilization_risk = np.clip(status["total_sessions"].to_numpy() / 7.0, 0, 1)
    fault_probability = np.clip(
        0.0025
        + age_years.to_numpy() * 0.0025
        + precipitation_risk.to_numpy() * 0.007
        + humidity_risk.to_numpy() * 0.004
        + utilization_risk * 0.006,
        0.002,
        0.12,
    )
    has_fault = rng.random(len(status)) < fault_probability
    fault_count = np.where(
        has_fault,
        1 + rng.poisson(0.22 + utilization_risk * 0.35),
        0,
    )
    maintenance_event = rng.random(len(status)) < (0.0015 + 0.006 * has_fault)
    fault_minutes = np.where(
        has_fault,
        np.clip(rng.lognormal(4.5, 0.62, len(status)), 12, 720),
        0,
    )
    maintenance_minutes = np.where(
        maintenance_event, rng.integers(60, 360, len(status)), 0
    )
    communication_outage = np.where(
        rng.random(len(status)) < 0.004, rng.integers(5, 90, len(status)), 0
    )
    offline_minutes = np.clip(
        fault_minutes + maintenance_minutes + communication_outage, 0, 1380
    ).astype(int)
    online_minutes = 1440 - offline_minutes
    availability_rate = online_minutes / 1440.0

    fault_codes = np.array(
        [
            "COMMUNICATION",
            "POWER_MODULE",
            "GROUND_FAULT",
            "OVER_CURRENT",
            "CONNECTOR_LOCK",
        ]
    )
    primary_fault_code = np.full(len(status), "NONE", dtype=object)
    primary_fault_code[has_fault] = rng.choice(fault_codes, has_fault.sum())
    health_score = np.clip(
        100
        - 1.4 * age_years.to_numpy()
        - 10.0 * fault_count
        - 18.0 * (offline_minutes / 1440.0)
        - 2.0 * utilization_risk
        + rng.normal(0, 1.4, len(status)),
        35,
        100,
    )

    status["status_id"] = [f"DSL{value:010d}" for value in range(1, len(status) + 1)]
    status["online_minutes"] = online_minutes
    status["offline_minutes"] = offline_minutes
    status["fault_minutes"] = fault_minutes.astype(int)
    status["fault_count"] = fault_count.astype(int)
    status["primary_fault_code"] = primary_fault_code
    status["availability_rate"] = np.round(availability_rate, 4)
    status["health_score"] = np.round(health_score, 1)
    status["maintenance_flag"] = maintenance_event.astype("int8")
    status["avg_output_power_kw"] = status["avg_output_power_kw"].round(2)
    status["record_time"] = status["stat_date"] + pd.Timedelta(hours=23, minutes=59)

    output = status[
        [
            "status_id",
            "device_id",
            "station_id",
            "record_time",
            "online_minutes",
            "offline_minutes",
            "fault_minutes",
            "fault_count",
            "primary_fault_code",
            "successful_sessions",
            "failed_sessions",
            "avg_output_power_kw",
            "availability_rate",
            "health_score",
            "maintenance_flag",
        ]
    ].copy()
    output["record_time"] = _timestamp_text(output["record_time"])

    latest = output.sort_values("record_time").groupby("device_id").tail(1)
    latest_status = np.where(
        latest["fault_count"] > 0,
        "fault",
        np.where(latest["availability_rate"] < 0.95, "offline", "online"),
    )
    updated_devices = devices.merge(
        pd.DataFrame(
            {"device_id": latest["device_id"].to_numpy(), "latest_status": latest_status}
        ),
        on="device_id",
        how="left",
        validate="one_to_one",
    )
    updated_devices["current_status"] = updated_devices["latest_status"].fillna("online")
    updated_devices = updated_devices.drop(columns="latest_status")
    return output, updated_devices


def validate_tables(
    users: pd.DataFrame,
    stations: pd.DataFrame,
    devices: pd.DataFrame,
    weather: pd.DataFrame,
    orders: pd.DataFrame,
    status: pd.DataFrame,
    station_hour_grid: pd.DataFrame,
) -> dict[str, object]:
    checks: dict[str, bool] = {}
    checks["unique_user_id"] = users["user_id"].is_unique
    checks["unique_station_id"] = stations["station_id"].is_unique
    checks["unique_device_id"] = devices["device_id"].is_unique
    checks["unique_weather_id"] = weather["weather_id"].is_unique
    checks["unique_session_id"] = orders["session_id"].is_unique
    checks["unique_status_id"] = status["status_id"].is_unique
    checks["order_user_fk"] = orders["user_id"].isin(users["user_id"]).all()
    checks["order_station_fk"] = orders["station_id"].isin(stations["station_id"]).all()
    checks["order_device_fk"] = orders["device_id"].isin(devices["device_id"]).all()
    checks["order_weather_fk"] = orders["weather_id"].isin(weather["weather_id"]).all()
    checks["status_device_fk"] = status["device_id"].isin(devices["device_id"]).all()

    device_station = devices.set_index("device_id")["station_id"]
    checks["order_device_belongs_to_station"] = (
        orders["device_id"].map(device_station).to_numpy()
        == orders["station_id"].to_numpy()
    ).all()
    device_counts = devices.groupby("station_id").size()
    checks["station_device_count_matches"] = (
        stations.set_index("station_id")["device_count"].sort_index().to_numpy()
        == device_counts.sort_index().to_numpy()
    ).all()

    registration = pd.to_datetime(users.set_index("user_id")["registered_at"])
    checks["users_registered_before_orders"] = (
        orders["user_id"].map(registration).to_numpy()
        <= pd.to_datetime(orders["created_at"]).to_numpy()
    ).all()
    checks["orders_end_after_start"] = (
        pd.to_datetime(orders["ended_at"]) > pd.to_datetime(orders["created_at"])
    ).all()
    order_calendar = _calendar_features(
        pd.Series(pd.to_datetime(orders["created_at"]), index=orders.index)
    )
    checks["order_holiday_fields_match_calendar"] = (
        orders["is_weekend"].to_numpy() == order_calendar["is_weekend"].to_numpy()
    ).all() and (
        orders["is_holiday"].to_numpy() == order_calendar["is_holiday"].to_numpy()
    ).all() and (
        orders["is_workday"].to_numpy() == order_calendar["is_workday"].to_numpy()
    ).all()

    weather_rates = (
        station_hour_grid.groupby("weather_type", as_index=False)
        .agg(station_hours=("station_id", "size"), orders=("order_count", "sum"))
        .assign(orders_per_1000_station_hours=lambda value: np.round(
            value["orders"] / value["station_hours"] * 1000, 3
        ))
        .sort_values("orders_per_1000_station_hours", ascending=False)
    )
    weather_rate_map = weather_rates.set_index("weather_type")[
        "orders_per_1000_station_hours"
    ]
    checks["rain_reduces_demand"] = bool(
        weather_rate_map.get("light_rain", np.inf)
        < weather_rate_map.get("sunny", -np.inf)
        and weather_rate_map.get("heavy_rain", np.inf)
        < weather_rate_map.get("light_rain", -np.inf)
    )

    checks = {name: bool(passed) for name, passed in checks.items()}
    failed_checks = [name for name, passed in checks.items() if not passed]
    if failed_checks:
        raise ValueError("Generated-table validation failed: " + ", ".join(failed_checks))

    return {
        "row_counts": {
            "users": int(len(users)),
            "stations": int(len(stations)),
            "devices": int(len(devices)),
            "weather_hourly": int(len(weather)),
            "charging_orders": int(len(orders)),
            "device_status_log": int(len(status)),
        },
        "checks": checks,
        "weather_demand_rates": weather_rates.to_dict("records"),
        "notes": [
            "All generated rows are synthetic and reproducible from the recorded seed.",
            "Weather affects order arrival intensity; do not use weather_id as a model feature.",
            "Join weather attributes through weather_id before training a demand model.",
        ],
    }


def write_data_dictionary(output_dir: Path) -> None:
    text = """# 六表训练数据说明

这些CSV是合成训练数据，不代表真实用户、设备状态或真实历史天气。

## 关联关系

- `charging_orders.user_id -> users.user_id`
- `charging_orders.station_id -> stations.station_id`
- `charging_orders.device_id -> devices.device_id`
- `charging_orders.weather_id -> weather_hourly.weather_id`
- `devices.station_id -> stations.station_id`
- `device_status_log.device_id -> devices.device_id`

## 表粒度

- `users.csv`：每位用户一行。
- `stations.csv`：每个充电站一行。
- `devices.csv`：每台充电桩一行。
- `weather_hourly.csv`：每个位置、每小时一行。
- `charging_orders.csv`：每笔充电订单一行。
- `device_status_log.csv`：每台设备、每天一行，记录当日在线和故障分钟数。

## 1. users.csv

| 字段 | 含义 |
|---|---|
| user_id | 用户主键 |
| registered_at | 注册时间，可用于新增用户和用户生命周期分析 |
| city | 用户所在城市 |
| member_level | bronze、silver、gold、platinum会员等级 |
| vehicle_type | sedan、suv、mpv、commercial车辆类型 |
| battery_capacity_kwh | 车辆电池容量，仅作为用户画像，不包含SOC遥测 |
| preferred_platform | android、ios或web |
| registration_channel | app、微信、站点扫码或合作渠道 |
| user_status | active或inactive |

## 2. stations.csv

| 字段 | 含义 |
|---|---|
| station_id | 充电站主键 |
| location_id | 天气位置关联键 |
| station_name、address、city | 站点名称和地址信息 |
| latitude、longitude | 站点坐标 |
| facility_type | 设施类型 |
| device_count | 站点设备数量，已与devices.csv核对一致 |
| open_time | 营业时间 |
| electricity_price_cny_kwh | 每千瓦时电价 |
| service_fee_cny_kwh | 每千瓦时服务费 |
| station_status | 站点当前状态 |
| updated_on | 站点信息更新时间 |

## 3. devices.csv

| 字段 | 含义 |
|---|---|
| device_id | 充电桩主键 |
| station_id | 所属站点外键 |
| device_code | 可展示的设备编码 |
| charger_type | DC_FAST或AC_SLOW |
| rated_power_kw | 额定功率 |
| connector_count | 充电枪数量 |
| manufacturer | 制造商 |
| commissioned_at | 投运日期 |
| last_maintenance_at | 最近维护日期 |
| current_status | online、offline或fault |

## 4. weather_hourly.csv

| 字段 | 含义 |
|---|---|
| weather_id | 天气记录主键 |
| location_id | 与站点关联的位置键 |
| weather_time | 整点时间 |
| weather_type、weather_name | 天气英文类别和中文名称 |
| temperature_c | 摄氏温度 |
| humidity_pct | 相对湿度百分比 |
| precipitation_mm | 小时降水量 |
| wind_speed_mps | 风速 |
| visibility_km | 能见度 |
| is_rain | 是否为小雨或大雨 |

## 5. charging_orders.csv

| 字段 | 含义 |
|---|---|
| session_id | 订单主键 |
| user_id、station_id、device_id | 用户、站点和设备外键 |
| location_id、weather_id | 位置和订单开始时刻天气外键 |
| created_at、ended_at | 充电开始和结束时间 |
| charge_time_hrs | 充电时长 |
| start_hour | 开始小时0至23 |
| weekday、weekday_name | 星期数字0至6和英文名称 |
| is_weekend | 是否自然周末 |
| is_holiday、holiday_name | 是否处于节假日区间以及节日名称 |
| is_workday | 是否按工作日处理，包含调休工作日 |
| energy_kwh | 订单充电量 |
| fee_amount_cny | 订单费用 |
| platform | 下单平台 |
| order_status | completed、interrupted或failed |
| payment_status | paid、pending、refunded或waived |

## 6. device_status_log.csv

| 字段 | 含义 |
|---|---|
| status_id | 日状态记录主键 |
| device_id、station_id | 设备和站点关联键 |
| record_time | 每日状态统计时点 |
| online_minutes、offline_minutes | 当日在线和离线分钟数 |
| fault_minutes、fault_count | 当日故障分钟数和次数 |
| primary_fault_code | 主要故障类型，NONE表示无故障 |
| successful_sessions、failed_sessions | 当日成功和失败会话数 |
| avg_output_power_kw | 当日订单平均输出功率 |
| availability_rate | 在线分钟数除以1440 |
| health_score | 结合设备年龄、故障、离线和负载生成的健康分 |
| maintenance_flag | 当日是否发生维护 |

## 机器学习注意事项

- 需求预测应先按站点和小时聚合订单，并保留没有订单的站点小时作为0样本。
- 星期直接由 `created_at` 计算，`weekday` 仅为方便核对。
- 订单包含 `is_weekend`、`is_holiday`、`holiday_name` 和 `is_workday`；调休周末按工作日标记。
- 天气通过 `weather_id` 关联。训练时使用天气类型、温度、湿度、降水等字段，不使用ID。
- 合成逻辑令小雨和大雨降低订单到达率，并让高湿/强降水轻微提高设备故障概率。
- 不要把 `fee_amount_cny` 用作订单量预测的未来输入，也不要使用预测时尚未产生的结束时间。

2015年节假日范围依据国务院办公厅安排；9月3日至5日纪念日假期依据国务院专项通知：

- https://www.nea.gov.cn/2015-01/08/c_133904166.htm
- https://www.ndrc.gov.cn/xxgk/zcfb/qt/201505/t20150513_967883.html
"""
    (output_dir / "DATA_DICTIONARY.md").write_text(text, encoding="utf-8")


def main() -> None:
    args = _parse_args()
    if args.target_orders < 1000:
        raise ValueError("--target-orders must be at least 1000")
    if args.users < 50:
        raise ValueError("--users must be at least 50")

    rng = np.random.default_rng(args.seed)
    source_orders = pd.read_csv(args.orders)
    source_stations = pd.read_csv(args.stations)
    source_created = pd.to_datetime(
        source_orders["created"], dayfirst=True, errors="coerce"
    )
    start_date = source_created.min().normalize()
    end_date = source_created.max().normalize()
    if pd.isna(start_date) or pd.isna(end_date):
        raise ValueError("The source orders do not contain parseable created timestamps")

    stations = build_stations(source_stations, rng)
    devices = build_devices(stations, start_date, end_date, rng)
    users, user_activity_weights = build_users(
        source_orders, args.users, start_date, end_date, rng
    )
    weather = build_weather(stations, start_date, end_date, rng)
    orders, station_hour_grid = build_orders(
        source_orders,
        stations,
        devices,
        users,
        user_activity_weights,
        weather,
        args.target_orders,
        start_date,
        end_date,
        rng,
    )
    device_status, devices = build_device_status_log(
        devices, stations, orders, weather, start_date, end_date, rng
    )

    report = validate_tables(
        users,
        stations,
        devices,
        weather,
        orders,
        device_status,
        station_hour_grid,
    )
    report["seed"] = args.seed
    report["source_date_range"] = {
        "start": start_date.strftime("%Y-%m-%d"),
        "end": end_date.strftime("%Y-%m-%d"),
    }

    output_dir = Path(args.output_dir).resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    tables = {
        "users.csv": users,
        "stations.csv": stations,
        "devices.csv": devices,
        "weather_hourly.csv": weather,
        "charging_orders.csv": orders,
        "device_status_log.csv": device_status,
    }
    # UTF-8 with BOM lets Excel/WPS detect Chinese correctly. Spark's CSV
    # reader can continue to use encoding=UTF-8 and will ignore the BOM.
    for filename, frame in tables.items():
        frame.to_csv(output_dir / filename, index=False, encoding="utf-8-sig")
    (output_dir / "quality_report.json").write_text(
        json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8"
    )
    write_data_dictionary(output_dir)
    print(json.dumps(report, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
