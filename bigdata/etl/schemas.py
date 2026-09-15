"""Explicit raw-string ODS schemas for the six charging-platform CSV tables.

ODS deliberately preserves source values as strings. Casting and validation
belong to DWD so malformed values remain observable instead of being silently
converted to null by Spark's CSV reader.
"""

from pyspark.sql.types import StringType, StructField, StructType

STANDARD_TIMESTAMP_FORMAT = "yyyy-MM-dd HH:mm:ss"
STANDARD_DATE_FORMAT = "yyyy-MM-dd"

# Compatibility for code that imported the former constant.



def _raw_string_schema(field_names):
    fields = [StructField(name, StringType(), True) for name in field_names]
    fields.append(StructField("_corrupt_record", StringType(), True))
    return StructType(fields)


CHARGING_ORDER_ODS_COLUMNS = (
    "session_id", "user_id", "station_id", "device_id", "location_id",
    "weather_id", "created_at", "ended_at", "charge_time_hrs",
    "start_hour", "weekday", "weekday_name", "is_weekend", "is_holiday",
    "holiday_name", "is_workday", "energy_kwh", "fee_amount_cny",
    "platform", "order_status", "payment_status",
)

USER_ODS_COLUMNS = (
    "user_id", "registered_at", "city", "member_level", "vehicle_type",
    "battery_capacity_kwh", "preferred_platform", "registration_channel",
    "user_status",
)

STATION_ODS_COLUMNS = (
    "station_id", "location_id", "station_name", "address", "city",
    "latitude", "longitude", "facility_type", "device_count", "open_time",
    "electricity_price_cny_kwh", "service_fee_cny_kwh", "station_status",
    "updated_on",
)

DEVICE_ODS_COLUMNS = (
    "device_id", "station_id", "device_code", "charger_type",
    "rated_power_kw", "connector_count", "manufacturer", "commissioned_at",
    "last_maintenance_at", "current_status",
)

WEATHER_ODS_COLUMNS = (
    "weather_id", "location_id", "weather_time", "weather_type",
    "weather_name", "temperature_c", "humidity_pct", "precipitation_mm",
    "wind_speed_mps", "visibility_km", "is_rain",
)

DEVICE_STATUS_ODS_COLUMNS = (
    "status_id", "device_id", "station_id", "record_time", "online_minutes",
    "offline_minutes", "fault_minutes", "fault_count", "primary_fault_code",
    "successful_sessions", "failed_sessions", "avg_output_power_kw",
    "availability_rate", "health_score", "maintenance_flag",
)

CHARGING_ORDER_ODS_SCHEMA = _raw_string_schema(CHARGING_ORDER_ODS_COLUMNS)
USER_ODS_SCHEMA = _raw_string_schema(USER_ODS_COLUMNS)
STATION_ODS_SCHEMA = _raw_string_schema(STATION_ODS_COLUMNS)
DEVICE_ODS_SCHEMA = _raw_string_schema(DEVICE_ODS_COLUMNS)
WEATHER_ODS_SCHEMA = _raw_string_schema(WEATHER_ODS_COLUMNS)
DEVICE_STATUS_ODS_SCHEMA = _raw_string_schema(DEVICE_STATUS_ODS_COLUMNS)


