"""Typed DWD cleaners for users, stations, devices, weather and status logs."""

from typing import Iterable

from pyspark.sql import DataFrame, functions as F
from pyspark.sql.types import DecimalType

from .schemas import ORDER_TIMESTAMP_FORMAT, STANDARD_DATE_FORMAT, STANDARD_TIMESTAMP_FORMAT


def _require_columns(df: DataFrame, required: Iterable[str], table_name: str) -> None:
    missing = sorted(set(required) - set(df.columns))
    if missing:
        raise ValueError(f"{table_name} 缺少必需列：" + ", ".join(missing))


def _text(name: str):
    return F.trim(F.col(name))


def _present(name: str):
    return F.col(name).isNotNull() & (_text(name) != "")


def _not_corrupt(df: DataFrame):
    return F.col("_corrupt_record").isNull() if "_corrupt_record" in df.columns else F.lit(True)


def _timestamp(name: str):
    value = _text(name)
    return F.coalesce(
        F.to_timestamp(value, STANDARD_TIMESTAMP_FORMAT),
        F.to_timestamp(value, ORDER_TIMESTAMP_FORMAT),
    )


def _date(name: str):
    return F.to_date(_text(name), STANDARD_DATE_FORMAT)


def clean_users(df: DataFrame) -> DataFrame:
    required = (
        "user_id", "registered_at", "city", "member_level", "vehicle_type",
        "battery_capacity_kwh", "preferred_platform", "registration_channel",
        "user_status",
    )
    _require_columns(df, required, "users.csv")
    typed = (
        df.withColumn("_registered_at", _timestamp("registered_at"))
        .withColumn("_battery_capacity_kwh", _text("battery_capacity_kwh").cast(DecimalType(8, 1)))
    )
    valid = (
        _not_corrupt(typed)
        & _present("user_id")
        & F.col("_registered_at").isNotNull()
        & _present("city")
        & F.lower(_text("member_level")).isin("bronze", "silver", "gold", "platinum")
        & F.lower(_text("vehicle_type")).isin("sedan", "suv", "mpv", "commercial")
        & (F.col("_battery_capacity_kwh") > 0)
        & F.lower(_text("preferred_platform")).isin("android", "ios", "web")
        & F.lower(_text("registration_channel")).isin("app", "wechat", "station_qr", "partner")
        & F.lower(_text("user_status")).isin("active", "inactive")
    )
    return (
        typed.where(valid)
        .select(
            _text("user_id").alias("user_id"),
            F.col("_registered_at").alias("registered_at"),
            _text("city").alias("city"),
            F.lower(_text("member_level")).alias("member_level"),
            F.lower(_text("vehicle_type")).alias("vehicle_type"),
            F.col("_battery_capacity_kwh").alias("battery_capacity_kwh"),
            F.lower(_text("preferred_platform")).alias("preferred_platform"),
            F.lower(_text("registration_channel")).alias("registration_channel"),
            F.lower(_text("user_status")).alias("user_status"),
        )
        .dropDuplicates(["user_id"])
    )


def clean_stations(df: DataFrame) -> DataFrame:
    required = (
        "station_id", "location_id", "station_name", "address", "city",
        "latitude", "longitude", "facility_type", "device_count", "open_time",
        "electricity_price_cny_kwh", "service_fee_cny_kwh", "station_status",
        "updated_on",
    )
    _require_columns(df, required, "stations.csv")
    typed = (
        df.withColumn("_latitude", _text("latitude").cast("double"))
        .withColumn("_longitude", _text("longitude").cast("double"))
        .withColumn("_facility_type", _text("facility_type").cast("int"))
        .withColumn("_device_count", _text("device_count").cast("int"))
        .withColumn("_electricity_price", _text("electricity_price_cny_kwh").cast(DecimalType(10, 2)))
        .withColumn("_service_fee", _text("service_fee_cny_kwh").cast(DecimalType(10, 2)))
        .withColumn("_updated_on", _date("updated_on"))
    )
    valid = (
        _not_corrupt(typed)
        & _present("station_id") & _present("location_id") & _present("station_name")
        & _present("address") & _present("city") & _present("open_time")
        & F.col("_latitude").between(-90, 90)
        & F.col("_longitude").between(-180, 180)
        & (F.col("_facility_type") > 0)
        & (F.col("_device_count") >= 0)
        & (F.col("_electricity_price") >= 0)
        & (F.col("_service_fee") >= 0)
        & F.lower(_text("station_status")).isin("active", "inactive", "maintenance")
        & F.col("_updated_on").isNotNull()
    )
    return (
        typed.where(valid)
        .select(
            _text("station_id").alias("station_id"),
            _text("location_id").alias("location_id"),
            _text("station_name").alias("station_name"),
            _text("address").alias("address"),
            _text("city").alias("city"),
            F.col("_latitude").alias("latitude"),
            F.col("_longitude").alias("longitude"),
            F.col("_facility_type").alias("facility_type"),
            F.col("_device_count").alias("device_count"),
            _text("open_time").alias("open_time"),
            F.col("_electricity_price").alias("electricity_price_cny_kwh"),
            F.col("_service_fee").alias("service_fee_cny_kwh"),
            F.lower(_text("station_status")).alias("station_status"),
            F.col("_updated_on").alias("updated_on"),
        )
        .dropDuplicates(["station_id"])
    )


def clean_devices(df: DataFrame) -> DataFrame:
    required = (
        "device_id", "station_id", "device_code", "charger_type",
        "rated_power_kw", "connector_count", "manufacturer", "commissioned_at",
        "last_maintenance_at", "current_status",
    )
    _require_columns(df, required, "devices.csv")
    typed = (
        df.withColumn("_rated_power_kw", _text("rated_power_kw").cast(DecimalType(10, 2)))
        .withColumn("_connector_count", _text("connector_count").cast("int"))
        .withColumn("_commissioned_at", _date("commissioned_at"))
        .withColumn("_last_maintenance_at", _date("last_maintenance_at"))
    )
    valid = (
        _not_corrupt(typed)
        & _present("device_id") & _present("station_id") & _present("device_code")
        & F.upper(_text("charger_type")).isin("AC_SLOW", "DC_FAST")
        & (F.col("_rated_power_kw") > 0) & (F.col("_connector_count") > 0)
        & _present("manufacturer")
        & F.col("_commissioned_at").isNotNull()
        & F.col("_last_maintenance_at").isNotNull()
        & (F.col("_last_maintenance_at") >= F.col("_commissioned_at"))
        & F.lower(_text("current_status")).isin("online", "offline", "fault")
    )
    return (
        typed.where(valid)
        .select(
            _text("device_id").alias("device_id"), _text("station_id").alias("station_id"),
            _text("device_code").alias("device_code"), F.upper(_text("charger_type")).alias("charger_type"),
            F.col("_rated_power_kw").alias("rated_power_kw"), F.col("_connector_count").alias("connector_count"),
            _text("manufacturer").alias("manufacturer"), F.col("_commissioned_at").alias("commissioned_at"),
            F.col("_last_maintenance_at").alias("last_maintenance_at"),
            F.lower(_text("current_status")).alias("current_status"),
        )
        .dropDuplicates(["device_id"])
    )


def clean_weather(df: DataFrame) -> DataFrame:
    required = (
        "weather_id", "location_id", "weather_time", "weather_type", "weather_name",
        "temperature_c", "humidity_pct", "precipitation_mm", "wind_speed_mps",
        "visibility_km", "is_rain",
    )
    _require_columns(df, required, "weather_hourly.csv")
    typed = (
        df.withColumn("_weather_time", _timestamp("weather_time"))
        .withColumn("_temperature_c", _text("temperature_c").cast("double"))
        .withColumn("_humidity_pct", _text("humidity_pct").cast("double"))
        .withColumn("_precipitation_mm", _text("precipitation_mm").cast("double"))
        .withColumn("_wind_speed_mps", _text("wind_speed_mps").cast("double"))
        .withColumn("_visibility_km", _text("visibility_km").cast("double"))
        .withColumn("_is_rain", _text("is_rain").cast("int"))
    )
    valid = (
        _not_corrupt(typed) & _present("weather_id") & _present("location_id")
        & F.col("_weather_time").isNotNull() & _present("weather_type") & _present("weather_name")
        & F.col("_temperature_c").between(-80, 65)
        & F.col("_humidity_pct").between(0, 100)
        & (F.col("_precipitation_mm") >= 0) & (F.col("_wind_speed_mps") >= 0)
        & (F.col("_visibility_km") > 0)
        & F.col("_is_rain").isin(0, 1)
    )
    return (
        typed.where(valid)
        .select(
            _text("weather_id").alias("weather_id"), _text("location_id").alias("location_id"),
            F.col("_weather_time").alias("weather_time"), F.lower(_text("weather_type")).alias("weather_type"),
            _text("weather_name").alias("weather_name"), F.col("_temperature_c").alias("temperature_c"),
            F.col("_humidity_pct").alias("humidity_pct"), F.col("_precipitation_mm").alias("precipitation_mm"),
            F.col("_wind_speed_mps").alias("wind_speed_mps"), F.col("_visibility_km").alias("visibility_km"),
            F.col("_is_rain").alias("is_rain"),
        )
        .dropDuplicates(["weather_id"])
    )


def clean_device_status(df: DataFrame) -> DataFrame:
    required = (
        "status_id", "device_id", "station_id", "record_time", "online_minutes",
        "offline_minutes", "fault_minutes", "fault_count", "primary_fault_code",
        "successful_sessions", "failed_sessions", "avg_output_power_kw",
        "availability_rate", "health_score", "maintenance_flag",
    )
    _require_columns(df, required, "device_status_log.csv")
    typed = df.withColumn("_record_time", _timestamp("record_time"))
    integer_columns = (
        "online_minutes", "offline_minutes", "fault_minutes", "fault_count",
        "successful_sessions", "failed_sessions", "maintenance_flag",
    )
    for name in integer_columns:
        typed = typed.withColumn(f"_{name}", _text(name).cast("int"))
    for name in ("avg_output_power_kw", "availability_rate", "health_score"):
        typed = typed.withColumn(f"_{name}", _text(name).cast("double"))

    valid = (
        _not_corrupt(typed) & _present("status_id") & _present("device_id") & _present("station_id")
        & F.col("_record_time").isNotNull()
        & F.col("_online_minutes").between(0, 1440)
        & F.col("_offline_minutes").between(0, 1440)
        & ((F.col("_online_minutes") + F.col("_offline_minutes")) == 1440)
        & F.col("_fault_minutes").between(0, 1440)
        & (F.col("_fault_minutes") <= F.col("_offline_minutes"))
        & (F.col("_fault_count") >= 0) & _present("primary_fault_code")
        & (F.col("_successful_sessions") >= 0) & (F.col("_failed_sessions") >= 0)
        & (F.col("_avg_output_power_kw") >= 0)
        & F.col("_availability_rate").between(0, 1)
        & (F.abs(F.col("_availability_rate") - F.col("_online_minutes") / 1440.0) <= 0.0001)
        & F.col("_health_score").between(0, 100)
        & F.col("_maintenance_flag").isin(0, 1)
    )
    return (
        typed.where(valid)
        .select(
            _text("status_id").alias("status_id"), _text("device_id").alias("device_id"),
            _text("station_id").alias("station_id"), F.col("_record_time").alias("record_time"),
            *[F.col(f"_{name}").alias(name) for name in integer_columns[:6]],
            _text("primary_fault_code").alias("primary_fault_code"),
            F.col("_avg_output_power_kw").alias("avg_output_power_kw"),
            F.col("_availability_rate").alias("availability_rate"),
            F.col("_health_score").alias("health_score"),
            F.col("_maintenance_flag").alias("maintenance_flag"),
        )
        .dropDuplicates(["status_id"])
    )
