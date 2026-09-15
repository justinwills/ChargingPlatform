"""PySpark feature engineering for the six charging-platform source tables.

The pipeline keeps the source data at station-hour grain for demand modeling:

    CSV/HDFS -> ODS readers and DWD validation -> integrated orders
    -> station-hour targets and features -> Spark CSV output

The CSV writer creates a directory named ``ml_training_dataset.csv``. This is
the normal distributed Spark CSV layout and can be read by Spark, pandas, or
the Hadoop command line tools.
"""

from __future__ import annotations

import argparse
import csv
import json
import os
import shutil
import sys
from pathlib import Path
from typing import Any, Iterable, Mapping, Optional


PROJECT_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_INPUT_ROOT = str(PROJECT_ROOT / "data" / "raw")
DEFAULT_OUTPUT_PATH = str(PROJECT_ROOT / "data" / "processed" / "ml_training_dataset.csv")

if __package__ in (None, ""):
    sys.path.insert(0, str(PROJECT_ROOT))

TABLE_FILES = {
    "charging_orders": "charging_orders.csv",
    "stations": "stations.csv",
    "devices": "devices.csv",
    "users": "users.csv",
    "weather_hourly": "weather_hourly.csv",
    "device_status_log": "device_status_log.csv",
}

EXPECTED_COLUMNS = {
    "charging_orders": (
        "session_id", "user_id", "station_id", "device_id", "location_id",
        "weather_id", "created_at", "ended_at", "charge_time_hrs",
        "start_hour", "weekday", "is_weekend", "is_holiday", "is_workday",
        "energy_kwh", "fee_amount_cny", "order_status",
    ),
    "stations": (
        "station_id", "location_id", "facility_type", "device_count",
        "electricity_price_cny_kwh", "service_fee_cny_kwh", "station_status",
    ),
    "devices": (
        "device_id", "station_id", "charger_type", "rated_power_kw",
        "connector_count", "current_status",
    ),
    "users": (
        "user_id", "vehicle_type", "battery_capacity_kwh", "member_level",
        "user_status",
    ),
    "weather_hourly": (
        "weather_id", "location_id", "weather_time", "weather_type",
        "temperature_c", "humidity_pct", "precipitation_mm", "wind_speed_mps",
        "visibility_km", "is_rain",
    ),
    "device_status_log": (
        "status_id", "device_id", "station_id", "online_minutes",
        "offline_minutes", "fault_minutes", "fault_count",
        "avg_output_power_kw", "availability_rate", "health_score",
    ),
}

IDENTIFIER_COLUMNS = ("station_id", "location_id", "datetime")

TARGET_COLUMNS = (
    "energy_kwh",
    "session_count",
    "charge_time_hrs",
    "completed_order_count",
    "interrupted_order_count",
    "failed_order_count",
)

FEATURE_COLUMNS = (
    "hour",
    "day",
    "month",
    "weekday_number",
    "is_weekend",
    "is_holiday",
    "is_workday",
    "weather_type_code",
    "temperature_c",
    "humidity_pct",
    "precipitation_mm",
    "wind_speed_mps",
    "visibility_km",
    "is_rain",
    "device_count",
    "facility_type_code",
    "electricity_price_cny_kwh",
    "service_fee_cny_kwh",
    "station_status_code",
    "dc_fast_device_count",
    "ac_slow_device_count",
    "avg_rated_power_kw",
    "avg_connector_count",
    "avg_availability_rate",
    "total_fault_count",
    "total_fault_minutes",
    "avg_health_score",
    "avg_output_power_kw",
    "avg_battery_capacity_kwh",
    "avg_vehicle_type_code",
    "avg_member_level_code",
    "sedan_share",
    "suv_share",
    "mpv_share",
    "commercial_share",
    "bronze_share",
    "silver_share",
    "gold_share",
    "platinum_share",
    # Historical station load used by the time-series models.  The first
    # rows of each station naturally have null values (there is no earlier
    # observation); callers may drop those rows when fitting a model.
    "load_lag_1h",
    "load_lag_2h",
    "load_lag_3h",
    "load_lag_24h",
)


def _spark_modules():
    """Import Spark lazily so non-Spark tooling can import this module."""
    try:
        from pyspark.sql import SparkSession, functions as F
        from pyspark.sql.types import StringType
    except ImportError as error:
        raise RuntimeError(
            "PySpark is required for the ML feature pipeline. "
            "Install requirements.txt or run with spark-submit."
        ) from error
    return SparkSession, F, StringType


def _etl_modules():
    from bigdata.etl.clean_charging import (
        clean_charging_orders,
        detect_charging_quality_issues,
        detect_charging_reference_issues,
    )
    from bigdata.etl.clean_tables import (
        clean_device_status,
        clean_devices,
        clean_stations,
        clean_users,
        clean_weather,
    )
    from bigdata.etl.ingest import (
        read_charging_orders_ods,
        read_device_status_ods,
        read_devices_ods,
        read_stations_ods,
        read_users_ods,
        read_weather_ods,
    )
    return {
        "clean_charging_orders": clean_charging_orders,
        "detect_charging_quality_issues": detect_charging_quality_issues,
        "detect_charging_reference_issues": detect_charging_reference_issues,
        "clean_device_status": clean_device_status,
        "clean_devices": clean_devices,
        "clean_stations": clean_stations,
        "clean_users": clean_users,
        "clean_weather": clean_weather,
        "read_charging_orders_ods": read_charging_orders_ods,
        "read_device_status_ods": read_device_status_ods,
        "read_devices_ods": read_devices_ods,
        "read_stations_ods": read_stations_ods,
        "read_users_ods": read_users_ods,
        "read_weather_ods": read_weather_ods,
    }


def _join_path(root: str, filename: str) -> str:
    return f"{root.rstrip('/')}/{filename.lstrip('/')}"


def _parse_timestamp(column, F):
    value = F.trim(column.cast("string"))
    return F.coalesce(
        F.to_timestamp(value, "yyyy-MM-dd HH:mm:ss"),
        F.to_timestamp(value, "d/M/yyyy H:mm"),
    )


def _load_and_clean_data(spark, input_root: str):
    """Read ODS tables, apply existing DWD cleaners, and validate references."""
    modules = _etl_modules()
    raw = {
        "charging_orders": modules["read_charging_orders_ods"](
            spark, _join_path(input_root, TABLE_FILES["charging_orders"])
        ),
        "stations": modules["read_stations_ods"](
            spark, _join_path(input_root, TABLE_FILES["stations"])
        ),
        "devices": modules["read_devices_ods"](
            spark, _join_path(input_root, TABLE_FILES["devices"])
        ),
        "users": modules["read_users_ods"](
            spark, _join_path(input_root, TABLE_FILES["users"])
        ),
        "weather_hourly": modules["read_weather_ods"](
            spark, _join_path(input_root, TABLE_FILES["weather_hourly"])
        ),
        "device_status_log": modules["read_device_status_ods"](
            spark, _join_path(input_root, TABLE_FILES["device_status_log"])
        ),
    }

    stations = modules["clean_stations"](raw["stations"])
    devices = modules["clean_devices"](raw["devices"])
    users = modules["clean_users"](raw["users"])
    weather = modules["clean_weather"](raw["weather_hourly"])
    status = modules["clean_device_status"](raw["device_status_log"])

    checked_orders = modules["detect_charging_quality_issues"](raw["charging_orders"])
    checked_orders = modules["detect_charging_reference_issues"](
        checked_orders,
        users=users,
        stations=stations,
        devices=devices,
        weather=weather,
    )
    orders = modules["clean_charging_orders"](checked_orders)

    return {
        "charging_orders": orders,
        "stations": stations,
        "devices": devices,
        "users": users,
        "weather_hourly": weather,
        "device_status_log": status,
    }, checked_orders


def load_all_data(spark, input_root: str = DEFAULT_INPUT_ROOT) -> dict[str, Any]:
    """Load and clean all six tables from a local path or HDFS URI."""
    data, _ = _load_and_clean_data(spark, input_root)
    return data


def _null_counts(df, F, StringType) -> dict[str, int]:
    expressions = []
    for field in df.schema.fields:
        condition = F.col(field.name).isNull()
        if isinstance(field.dataType, StringType):
            condition = condition | (F.trim(F.col(field.name)) == "")
        expressions.append(
            F.sum(F.when(condition, F.lit(1)).otherwise(F.lit(0))).alias(field.name)
        )
    row = df.agg(*expressions).first()
    return {name: int(value or 0) for name, value in row.asDict().items()}


def _unmatched_count(child, parent, child_key: str, parent_key: str, F) -> int:
    child_keys = child.select(F.col(child_key).alias("_key")).where(
        F.col(child_key).isNotNull()
    )
    parent_keys = parent.select(F.col(parent_key).alias("_key")).dropDuplicates()
    return int(child_keys.join(parent_keys, "_key", "left_anti").count())


def validate_data(data: Mapping[str, Any]) -> dict[str, Any]:
    """Return schema, missing-value, duplicate, and foreign-key validation results."""
    _, F, StringType = _spark_modules()
    missing_tables = sorted(set(EXPECTED_COLUMNS) - set(data))
    if missing_tables:
        raise ValueError(f"Missing tables: {', '.join(missing_tables)}")

    schema_check = {}
    row_counts = {}
    missing_values = {}
    duplicate_counts = {}
    key_columns = {
        "charging_orders": "session_id",
        "stations": "station_id",
        "devices": "device_id",
        "users": "user_id",
        "weather_hourly": "weather_id",
        "device_status_log": "status_id",
    }
    for table_name, expected in EXPECTED_COLUMNS.items():
        frame = data[table_name]
        missing_columns = sorted(set(expected) - set(frame.columns))
        schema_check[table_name] = {
            "missing_columns": missing_columns,
            "extra_columns": sorted(set(frame.columns) - set(expected)),
            "valid": not missing_columns,
        }
        row_counts[table_name] = int(frame.count())
        missing_values[table_name] = _null_counts(frame, F, StringType)
        key = key_columns[table_name]
        duplicate_counts[table_name] = row_counts[table_name] - int(
            frame.select(key).dropDuplicates().count()
        )

    foreign_keys = {
        "order_user_fk_unmatched": _unmatched_count(
            data["charging_orders"], data["users"], "user_id", "user_id", F
        ),
        "order_station_fk_unmatched": _unmatched_count(
            data["charging_orders"], data["stations"], "station_id", "station_id", F
        ),
        "order_device_fk_unmatched": _unmatched_count(
            data["charging_orders"], data["devices"], "device_id", "device_id", F
        ),
        "order_weather_fk_unmatched": _unmatched_count(
            data["charging_orders"], data["weather_hourly"], "weather_id", "weather_id", F
        ),
        "device_station_fk_unmatched": _unmatched_count(
            data["devices"], data["stations"], "station_id", "station_id", F
        ),
        "status_device_fk_unmatched": _unmatched_count(
            data["device_status_log"], data["devices"], "device_id", "device_id", F
        ),
    }
    return {
        "schema_check": schema_check,
        "row_counts": row_counts,
        "missing_values": missing_values,
        "duplicate_counts": duplicate_counts,
        "foreign_keys": foreign_keys,
    }


def create_time_features(df, timestamp_column: str = "created_at"):
    """Extract calendar features from a timestamp using Monday=0 weekday numbering."""
    _, F, _ = _spark_modules()
    if timestamp_column not in df.columns:
        raise ValueError(f"Missing timestamp column: {timestamp_column}")
    timestamp = _parse_timestamp(F.col(timestamp_column), F)
    return (
        df.withColumn("datetime", F.date_trunc("hour", timestamp))
        .withColumn("hour", F.hour(timestamp).cast("int"))
        .withColumn("day", F.dayofmonth(timestamp).cast("int"))
        .withColumn("month", F.month(timestamp).cast("int"))
        .withColumn(
            "weekday_number",
            F.pmod(F.dayofweek(timestamp) + F.lit(5), F.lit(7)).cast("int"),
        )
        .withColumn(
            "is_weekend",
            F.coalesce(F.col("is_weekend").cast("int"), F.when(F.dayofweek(timestamp).isin(1, 7), 1).otherwise(0))
            if "is_weekend" in df.columns
            else F.when(F.dayofweek(timestamp).isin(1, 7), 1).otherwise(0),
        )
        .withColumn(
            "is_holiday",
            F.coalesce(F.col("is_holiday").cast("int"), F.lit(0))
            if "is_holiday" in df.columns
            else F.lit(0),
        )
        .withColumn(
            "is_workday",
            F.coalesce(F.col("is_workday").cast("int"), F.lit(1))
            if "is_workday" in df.columns
            else F.lit(1),
        )
    )


def build_weather_features(weather):
    """Encode weather type and retain numeric weather measurements."""
    _, F, _ = _spark_modules()
    weather_type_code = (
        F.when(F.lower(F.col("weather_type")) == "sunny", 0)
        .when(F.lower(F.col("weather_type")) == "cloudy", 1)
        .when(F.lower(F.col("weather_type")) == "overcast", 2)
        .when(F.lower(F.col("weather_type")) == "fog", 3)
        .when(F.lower(F.col("weather_type")) == "light_rain", 4)
        .when(F.lower(F.col("weather_type")) == "heavy_rain", 5)
        .when(F.lower(F.col("weather_type")) == "snow", 6)
        .otherwise(-1)
    )
    return (
        weather.withColumn("weather_time", _parse_timestamp(F.col("weather_time"), F))
        .withColumn("weather_type_code", weather_type_code.cast("int"))
        .withColumn("temperature_c", F.col("temperature_c").cast("double"))
        .withColumn("humidity_pct", F.col("humidity_pct").cast("double"))
        .withColumn("precipitation_mm", F.col("precipitation_mm").cast("double"))
        .withColumn("wind_speed_mps", F.col("wind_speed_mps").cast("double"))
        .withColumn("visibility_km", F.col("visibility_km").cast("double"))
        .withColumn("is_rain", F.col("is_rain").cast("int"))
        .withColumn("datetime", F.date_trunc("hour", F.col("weather_time")))
    )


def build_station_features(stations):
    """Create numeric station-level prediction features."""
    _, F, _ = _spark_modules()
    station_status_code = (
        F.when(F.lower(F.col("station_status")) == "active", 1.0)
        .when(F.lower(F.col("station_status")) == "maintenance", 0.5)
        .when(F.lower(F.col("station_status")) == "inactive", 0.0)
        .otherwise(-1.0)
    )
    return (
        stations.withColumn("facility_type_code", F.col("facility_type").cast("double"))
        .withColumn("device_count", F.col("device_count").cast("double"))
        .withColumn("electricity_price_cny_kwh", F.col("electricity_price_cny_kwh").cast("double"))
        .withColumn("service_fee_cny_kwh", F.col("service_fee_cny_kwh").cast("double"))
        .withColumn("station_status_code", station_status_code)
        .select(
            "station_id", "location_id", "device_count", "facility_type_code",
            "electricity_price_cny_kwh", "service_fee_cny_kwh", "station_status_code",
        )
    )


def build_device_features(devices, device_status_log):
    """Aggregate device and daily status information to one row per station."""
    _, F, _ = _spark_modules()
    device_features = devices.groupBy("station_id").agg(
        F.sum(F.when(F.upper(F.col("charger_type")) == "DC_FAST", 1).otherwise(0)).cast("double").alias("dc_fast_device_count"),
        F.sum(F.when(F.upper(F.col("charger_type")) == "AC_SLOW", 1).otherwise(0)).cast("double").alias("ac_slow_device_count"),
        F.avg(F.col("rated_power_kw").cast("double")).alias("avg_rated_power_kw"),
        F.avg(F.col("connector_count").cast("double")).alias("avg_connector_count"),
    )
    status_features = device_status_log.groupBy("station_id").agg(
        F.avg(F.col("availability_rate").cast("double")).alias("avg_availability_rate"),
        F.sum(F.col("fault_count").cast("long")).alias("total_fault_count"),
        F.sum(F.col("fault_minutes").cast("long")).alias("total_fault_minutes"),
        F.avg(F.col("health_score").cast("double")).alias("avg_health_score"),
        F.avg(F.col("avg_output_power_kw").cast("double")).alias("avg_output_power_kw"),
    )
    return device_features.join(status_features, "station_id", "left")


def build_user_vehicle_features(users):
    """Encode user vehicle and membership attributes without using user_id as a feature."""
    _, F, _ = _spark_modules()
    vehicle_code = (
        F.when(F.lower(F.col("vehicle_type")) == "sedan", 0)
        .when(F.lower(F.col("vehicle_type")) == "suv", 1)
        .when(F.lower(F.col("vehicle_type")) == "mpv", 2)
        .when(F.lower(F.col("vehicle_type")) == "commercial", 3)
        .otherwise(-1)
    )
    member_code = (
        F.when(F.lower(F.col("member_level")) == "bronze", 0)
        .when(F.lower(F.col("member_level")) == "silver", 1)
        .when(F.lower(F.col("member_level")) == "gold", 2)
        .when(F.lower(F.col("member_level")) == "platinum", 3)
        .otherwise(-1)
    )
    return (
        users.withColumn("vehicle_type_code", vehicle_code.cast("double"))
        .withColumn("member_level_code", member_code.cast("double"))
        .withColumn("battery_capacity_kwh", F.col("battery_capacity_kwh").cast("double"))
        .select(
            "user_id", "vehicle_type", "vehicle_type_code", "battery_capacity_kwh",
            "member_level", "member_level_code",
        )
    )


def _select_join_columns(df, alias: str, excluded: Iterable[str], F):
    excluded_set = set(excluded)
    return [
        F.col(f"{alias}.{column}").alias(column)
        for column in df.columns
        if column not in excluded_set
    ]


def merge_tables(data: Mapping[str, Any]):
    """Build an order-level integrated DataFrame from all six cleaned tables."""
    _, F, _ = _spark_modules()
    orders = create_time_features(data["charging_orders"])
    stations = build_station_features(data["stations"])
    devices = build_device_features(data["devices"], data["device_status_log"])
    users = build_user_vehicle_features(data["users"])
    weather = build_weather_features(data["weather_hourly"])

    joined = (
        orders.alias("o")
        .join(stations.alias("s"), F.col("o.station_id") == F.col("s.station_id"), "left")
        .join(devices.alias("d"), F.col("o.station_id") == F.col("d.station_id"), "left")
        .join(users.alias("u"), F.col("o.user_id") == F.col("u.user_id"), "left")
        .join(weather.alias("w"), F.col("o.weather_id") == F.col("w.weather_id"), "left")
    )

    order_columns = [F.col(f"o.{column}").alias(column) for column in orders.columns]
    station_columns = _select_join_columns(stations, "s", ("station_id", "location_id"), F)
    device_columns = _select_join_columns(devices, "d", ("station_id",), F)
    user_columns = _select_join_columns(users, "u", ("user_id",), F)
    weather_columns = _select_join_columns(weather, "w", ("weather_id", "location_id", "datetime"), F)
    return joined.select(
        *(order_columns + station_columns + device_columns + user_columns + weather_columns)
    )


def _aggregate_user_features(integrated):
    _, F, _ = _spark_modules()
    return integrated.groupBy("station_id", "datetime").agg(
        F.avg("battery_capacity_kwh").alias("avg_battery_capacity_kwh"),
        F.avg("vehicle_type_code").alias("avg_vehicle_type_code"),
        F.avg("member_level_code").alias("avg_member_level_code"),
        F.avg(F.when(F.col("vehicle_type") == "sedan", 1.0).otherwise(0.0)).alias("sedan_share"),
        F.avg(F.when(F.col("vehicle_type") == "suv", 1.0).otherwise(0.0)).alias("suv_share"),
        F.avg(F.when(F.col("vehicle_type") == "mpv", 1.0).otherwise(0.0)).alias("mpv_share"),
        F.avg(F.when(F.col("vehicle_type") == "commercial", 1.0).otherwise(0.0)).alias("commercial_share"),
        F.avg(F.when(F.col("member_level") == "bronze", 1.0).otherwise(0.0)).alias("bronze_share"),
        F.avg(F.when(F.col("member_level") == "silver", 1.0).otherwise(0.0)).alias("silver_share"),
        F.avg(F.when(F.col("member_level") == "gold", 1.0).otherwise(0.0)).alias("gold_share"),
        F.avg(F.when(F.col("member_level") == "platinum", 1.0).otherwise(0.0)).alias("platinum_share"),
    )


def _build_station_hour_dataset(data: Mapping[str, Any]):
    _, F, _ = _spark_modules()
    integrated = merge_tables(data)
    stations = build_station_features(data["stations"])
    weather = build_weather_features(data["weather_hourly"])
    devices = build_device_features(data["devices"], data["device_status_log"])

    bounds = integrated.agg(
        F.min("datetime").alias("start_datetime"),
        F.max("datetime").alias("end_datetime"),
    ).first()
    if bounds is None or bounds.start_datetime is None or bounds.end_datetime is None:
        raise ValueError("No valid charging orders were available for feature engineering")

    spark = integrated.sparkSession
    bounds_frame = spark.createDataFrame(
        [(bounds.start_datetime, bounds.end_datetime)],
        ["start_datetime", "end_datetime"],
    )
    hours = bounds_frame.select(
        F.explode(
            F.sequence("start_datetime", "end_datetime", F.expr("INTERVAL 1 HOUR"))
        ).alias("datetime")
    )
    grid = stations.crossJoin(hours).join(devices, "station_id", "left")
    calendar = (
        integrated.withColumn("date", F.to_date("datetime"))
        .groupBy("date")
        .agg(
            F.max("is_weekend").cast("int").alias("calendar_is_weekend"),
            F.max("is_holiday").cast("int").alias("calendar_is_holiday"),
            F.max("is_workday").cast("int").alias("calendar_is_workday"),
        )
    )

    order_hour = integrated.groupBy("station_id", "datetime").agg(
        F.sum(F.col("energy_kwh").cast("double")).alias("energy_kwh"),
        F.count("*").cast("long").alias("session_count"),
        F.sum(F.col("charge_time_hrs").cast("double")).alias("charge_time_hrs"),
        F.avg(F.col("charge_time_hrs").cast("double")).alias("avg_charge_time_hrs"),
        F.sum(F.when(F.col("order_status") == "completed", 1).otherwise(0)).cast("long").alias("completed_order_count"),
        F.sum(F.when(F.col("order_status") == "interrupted", 1).otherwise(0)).cast("long").alias("interrupted_order_count"),
        F.sum(F.when(F.col("order_status") == "failed", 1).otherwise(0)).cast("long").alias("failed_order_count"),
    )

    user_hour = _aggregate_user_features(integrated)
    weather_hour = weather.select(
        "location_id", "datetime", "weather_type_code", "temperature_c",
        "humidity_pct", "precipitation_mm", "wind_speed_mps", "visibility_km",
        "is_rain",
    ).dropDuplicates(["location_id", "datetime"])

    result = (
        grid.withColumn("date", F.to_date("datetime"))
        .join(calendar, "date", "left")
        .join(order_hour, ["station_id", "datetime"], "left")
        .join(user_hour, ["station_id", "datetime"], "left")
        .join(weather_hour, ["location_id", "datetime"], "left")
        .withColumn("hour", F.hour("datetime").cast("int"))
        .withColumn("day", F.dayofmonth("datetime").cast("int"))
        .withColumn("month", F.month("datetime").cast("int"))
        .withColumn("weekday_number", F.pmod(F.dayofweek("datetime") + F.lit(5), F.lit(7)).cast("int"))
        .withColumn(
            "is_weekend",
            F.coalesce(
                F.col("calendar_is_weekend"),
                F.when(F.col("weekday_number") >= 5, 1).otherwise(0),
            ),
        )
        .withColumn("is_holiday", F.coalesce(F.col("calendar_is_holiday"), F.lit(0)))
        .withColumn(
            "is_workday",
            F.coalesce(
                F.col("calendar_is_workday"),
                F.when(F.col("weekday_number") < 5, 1).otherwise(0),
            ),
        )
    )

    numeric_defaults = {
        "energy_kwh": 0.0,
        "session_count": 0,
        "charge_time_hrs": 0.0,
        "avg_charge_time_hrs": 0.0,
        "completed_order_count": 0,
        "interrupted_order_count": 0,
        "failed_order_count": 0,
        "weather_type_code": -1,
        "temperature_c": 0.0,
        "humidity_pct": 0.0,
        "precipitation_mm": 0.0,
        "wind_speed_mps": 0.0,
        "visibility_km": 0.0,
        "is_rain": 0,
        "avg_availability_rate": 0.0,
        "total_fault_count": 0,
        "total_fault_minutes": 0,
        "avg_health_score": 0.0,
        "avg_output_power_kw": 0.0,
        "avg_rated_power_kw": 0.0,
        "avg_connector_count": 0.0,
        "dc_fast_device_count": 0.0,
        "ac_slow_device_count": 0.0,
        "avg_battery_capacity_kwh": 0.0,
        "avg_vehicle_type_code": -1.0,
        "avg_member_level_code": -1.0,
        "sedan_share": 0.0,
        "suv_share": 0.0,
        "mpv_share": 0.0,
        "commercial_share": 0.0,
        "bronze_share": 0.0,
        "silver_share": 0.0,
        "gold_share": 0.0,
        "platinum_share": 0.0,
    }
    # Keep lag columns null when there is not enough history.  Filling those
    # values with zero would turn "unknown history" into a real zero-demand
    # observation and can bias a forecasting model.
    result = result.fillna(numeric_defaults)
    return build_lag_features(result)


def select_features(df, include_targets: bool = True):
    """Select identifiers, targets, and numeric model features from a dataset."""
    required = list(IDENTIFIER_COLUMNS) + list(FEATURE_COLUMNS)
    if include_targets:
        required = list(IDENTIFIER_COLUMNS) + list(TARGET_COLUMNS) + list(FEATURE_COLUMNS)
    missing = sorted(set(required) - set(df.columns))
    if missing:
        raise ValueError("Feature dataset is missing columns: " + ", ".join(missing))
    return df.select(*required)


def _write_dataset(dataset, output_path: str, mode: str) -> None:
    """Write with Spark, with a Windows-local fallback for missing Hadoop native IO."""
    local_windows_path = os.name == "nt" and "://" not in output_path
    if not local_windows_path:
        (
            dataset.write.mode(mode)
            .option("header", True)
            .option("encoding", "UTF-8")
            .csv(output_path)
        )
        return

    output = Path(output_path).expanduser().resolve()
    if mode not in ("overwrite", "error", "ignore"):
        raise ValueError("append mode requires a distributed Spark output path")
    if output.exists():
        if mode == "error":
            raise FileExistsError(output)
        if mode == "ignore":
            return
        if output.is_dir():
            shutil.rmtree(output)
        else:
            output.unlink()
    output.parent.mkdir(parents=True, exist_ok=True)

    # Spark's Windows commit protocol needs native Hadoop libraries that are
    # unavailable in a plain Python installation. The transformation remains
    # Spark-based; only this local file materialization uses the Python runtime.
    temporary = output.with_suffix(output.suffix + ".tmp")
    with temporary.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.writer(handle)
        writer.writerow(dataset.columns)
        for row in dataset.repartition(64, "station_id").toLocalIterator():
            writer.writerow(row)
    temporary.replace(output)


def build_training_dataset(
    spark,
    input_root: str = DEFAULT_INPUT_ROOT,
    output_path: str = DEFAULT_OUTPUT_PATH,
    mode: str = "overwrite",
):
    """Build, validate, and write the station-hour ML training dataset."""
    _, F, _ = _spark_modules()
    data, checked_orders = _load_and_clean_data(spark, input_root)
    report = validate_data(data)
    report["rejected_order_rows"] = int(
        checked_orders.where(F.size("quality_issues") > 0).count()
    )
    dataset = select_features(_build_station_hour_dataset(data))
    _write_dataset(dataset, output_path, mode)
    print(json.dumps(report, ensure_ascii=False, indent=2))
    print(f"ML training dataset written to {output_path}")
    return dataset


def build_ml_dataset(
    spark,
    input_root: str = DEFAULT_INPUT_ROOT,
    output_path: str = DEFAULT_OUTPUT_PATH,
    mode: str = "overwrite",
):
    """Compatibility name matching the project task specification."""
    return build_training_dataset(spark, input_root, output_path, mode)


def _is_spark_dataframe(value: Any) -> bool:
    """Return whether *value* looks like a PySpark DataFrame.

    Importing pyspark just to perform this check makes the non-Spark helpers
    unusable on a developer laptop, so use the DataFrame's module name.
    """
    return value.__class__.__module__.startswith("pyspark.sql")


def _pandas_historical_load(
    orders,
    timestamp_column: str = "datetime",
    value_column: str = "energy_kwh",
    granularity: str = "station_hour",
):
    import pandas as pd

    frame = orders.copy()
    if value_column not in frame.columns:
        raise ValueError(f"Missing load column: {value_column}")
    if timestamp_column not in frame.columns:
        # ``created_at`` is the source-table name; accepting it makes this
        # helper useful before the Spark feature pipeline has run.
        if timestamp_column == "datetime" and "created_at" in frame.columns:
            timestamp_column = "created_at"
        else:
            raise ValueError(f"Missing timestamp column: {timestamp_column}")
    frame["datetime"] = pd.to_datetime(frame[timestamp_column], errors="coerce").dt.floor("h")
    frame[value_column] = pd.to_numeric(frame[value_column], errors="coerce").fillna(0.0)
    frame = frame.dropna(subset=["datetime"])
    key = granularity.lower().replace("-", "_")
    if key in {"station_hour", "hour_station", "stationhour"}:
        groups = ["station_id", "datetime"]
    elif key in {"hour", "hourly", "global_hour"}:
        groups = ["datetime"]
    elif key in {"station", "station_total", "station_level"}:
        groups = ["station_id"]
    else:
        raise ValueError("granularity must be station_hour, hour, or station")
    if "station_id" in groups and "station_id" not in frame.columns:
        raise ValueError("Missing station column: station_id")
    result = frame.groupby(groups, as_index=False, sort=True)[value_column].sum()
    return result.sort_values(groups, kind="mergesort").reset_index(drop=True)


def build_historical_load(
    data: Mapping[str, Any] | Any,
    granularity: str = "station_hour",
    timestamp_column: str = "datetime",
    value_column: str = "energy_kwh",
):
    """Construct historical charging load from ``energy_kwh``.

    A mapping containing the six cleaned source tables uses the full Spark
    station-hour grid (including zero-demand hours) and returns the normal ML
    feature contract.  A pandas or Spark order DataFrame can be passed
    directly for a lightweight aggregate using ``station_hour`` (default),
    ``hour`` or ``station`` granularity.
    """
    if isinstance(data, Mapping):
        # A small mapping containing only orders is convenient for ad-hoc
        # aggregation and should not be mistaken for the six-table ETL input.
        if "charging_orders" in data and not set(EXPECTED_COLUMNS).issubset(data):
            return build_historical_load(
                data["charging_orders"], granularity, timestamp_column, value_column
            )
        dataset = _build_station_hour_dataset(data)
        key = granularity.lower().replace("-", "_")
        if key not in {"station_hour", "hour_station", "stationhour"}:
            _, F, _ = _spark_modules()
            if key in {"hour", "hourly", "global_hour"}:
                return dataset.groupBy("datetime").agg(F.sum("energy_kwh").alias("energy_kwh")).orderBy("datetime")
            if key in {"station", "station_total", "station_level"}:
                return dataset.groupBy("station_id").agg(F.sum("energy_kwh").alias("energy_kwh")).orderBy("station_id")
            raise ValueError("granularity must be station_hour, hour, or station")
        return select_features(dataset, include_targets=True)
    if _is_spark_dataframe(data):
        _, F, _ = _spark_modules()
        if value_column not in data.columns:
            raise ValueError(f"Missing load column: {value_column}")
        source_timestamp = timestamp_column
        if source_timestamp not in data.columns and source_timestamp == "datetime" and "created_at" in data.columns:
            source_timestamp = "created_at"
        if source_timestamp not in data.columns:
            raise ValueError(f"Missing timestamp column: {timestamp_column}")
        frame = data.withColumn("datetime", F.date_trunc("hour", _parse_timestamp(F.col(source_timestamp), F)))
        frame = frame.where(F.col("datetime").isNotNull())
        frame = frame.withColumn(value_column, F.coalesce(F.col(value_column).cast("double"), F.lit(0.0)))
        key = granularity.lower().replace("-", "_")
        if key in {"station_hour", "hour_station", "stationhour"}:
            groups = ["station_id", "datetime"]
        elif key in {"hour", "hourly", "global_hour"}:
            groups = ["datetime"]
        elif key in {"station", "station_total", "station_level"}:
            groups = ["station_id"]
        else:
            raise ValueError("granularity must be station_hour, hour, or station")
        if "station_id" in groups and "station_id" not in frame.columns:
            raise ValueError("Missing station column: station_id")
        return frame.groupBy(*groups).agg(F.sum(value_column).alias(value_column)).orderBy(*groups)
    # pandas is intentionally imported only for this local path.
    return _pandas_historical_load(data, timestamp_column, value_column, granularity)


def build_lag_features(
    df,
    value_column: str = "energy_kwh",
    lags: Iterable[int] = (1, 2, 3, 24),
    timestamp_column: str = "datetime",
    partition_columns: Iterable[str] = ("station_id",),
    **kwargs,
):
    """Add historical load lag columns without looking into the future.

    For Spark, window functions preserve distributed execution.  For pandas,
    ``groupby.shift`` provides the equivalent semantics.  Lag columns are
    named ``load_lag_<N>h`` and are ordered by ``timestamp_column`` within
    each station (or the supplied partition columns).
    """
    # Accept the names commonly used by pandas notebooks and earlier task
    # drafts while keeping one documented API.
    if "lag_hours" in kwargs:
        lags = kwargs.pop("lag_hours")
    if "lag_columns" in kwargs:
        lags = kwargs.pop("lag_columns")
    if "time_col" in kwargs:
        timestamp_column = kwargs.pop("time_col")
    if "load_column" in kwargs:
        value_column = kwargs.pop("load_column")
    if "group_columns" in kwargs:
        partition_columns = kwargs.pop("group_columns")
    if "group_by" in kwargs:
        partition_columns = kwargs.pop("group_by")
    if kwargs:
        raise TypeError("Unexpected keyword argument(s): " + ", ".join(sorted(kwargs)))
    lag_values = tuple(dict.fromkeys(int(lag) for lag in lags))
    if any(lag <= 0 for lag in lag_values):
        raise ValueError("lags must contain positive hour offsets")
    if not lag_values:
        return df
    partitions = tuple(partition_columns)
    if _is_spark_dataframe(df):
        from pyspark.sql import Window
        _, F, _ = _spark_modules()
        if value_column not in df.columns:
            raise ValueError(f"Missing load column: {value_column}")
        if timestamp_column not in df.columns:
            raise ValueError(f"Missing timestamp column: {timestamp_column}")
        # Global hourly aggregates do not carry station_id; in that case the
        # default partition naturally becomes one global time series.
        if partitions == ("station_id",) and "station_id" not in df.columns:
            partitions = ()
        missing = [column for column in partitions if column not in df.columns]
        if missing:
            raise ValueError("Missing partition columns: " + ", ".join(missing))
        window = Window.partitionBy(*(F.col(column) for column in partitions)).orderBy(F.col(timestamp_column))
        result = df
        for lag in lag_values:
            lagged = F.lag(F.col(value_column).cast("double"), lag).over(window)
            result = result.withColumn(f"load_lag_{lag}h", lagged)
            # ``lag_1h`` is retained as a concise compatibility alias for
            # notebooks and older task specifications.
            result = result.withColumn(f"lag_{lag}h", F.col(f"load_lag_{lag}h"))
            result = result.withColumn(f"lag_{lag}", F.col(f"load_lag_{lag}h"))
        return result

    import pandas as pd
    if not isinstance(df, pd.DataFrame):
        raise TypeError("df must be a pandas or PySpark DataFrame")
    if partitions == ("station_id",) and "station_id" not in df.columns:
        partitions = ()
    missing = [column for column in (value_column, timestamp_column, *partitions) if column not in df.columns]
    if missing:
        raise ValueError("Missing columns: " + ", ".join(dict.fromkeys(missing)))
    result = df.copy()
    result[timestamp_column] = pd.to_datetime(result[timestamp_column], errors="coerce")
    result[value_column] = pd.to_numeric(result[value_column], errors="coerce")
    result = result.sort_values([*partitions, timestamp_column], kind="mergesort").reset_index(drop=True)
    grouped = result.groupby(list(partitions), sort=False, dropna=False)[value_column] if partitions else None
    for lag in lag_values:
        result[f"load_lag_{lag}h"] = grouped.shift(lag) if grouped is not None else result[value_column].shift(lag)
        result[f"lag_{lag}h"] = result[f"load_lag_{lag}h"]
        result[f"lag_{lag}"] = result[f"load_lag_{lag}h"]
    return result


def _parse_args(argv: Optional[Iterable[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input-root", default=DEFAULT_INPUT_ROOT)
    parser.add_argument("--output", default=DEFAULT_OUTPUT_PATH)
    parser.add_argument("--master", default=None, help="Spark master, e.g. local[*]")
    parser.add_argument(
        "--mode", choices=("overwrite", "error", "ignore", "append"), default="overwrite"
    )
    return parser.parse_args(argv)


def main(argv: Optional[Iterable[str]] = None) -> None:
    args = _parse_args(argv)
    SparkSession, _, _ = _spark_modules()
    builder = (
        SparkSession.builder.appName("ChargingPlatform-MLFeatures")
        .config("spark.sql.session.timeZone", "Asia/Shanghai")
        .config("spark.sql.ansi.enabled", "false")
        .config("spark.ui.enabled", "false")
        .config("spark.driver.memory", "4g")
        .config("spark.sql.shuffle.partitions", "64")
        .config("spark.sql.adaptive.coalescePartitions.enabled", "false")
    )
    if args.master:
        builder = builder.master(args.master)
    spark = builder.getOrCreate()
    try:
        build_training_dataset(
            spark,
            input_root=args.input_root,
            output_path=args.output,
            mode=args.mode,
        )
    finally:
        spark.stop()


if __name__ == "__main__":
    main()
