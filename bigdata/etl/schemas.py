"""Explicit ODS schemas for the teacher-provided CSV files.

ODS intentionally keeps source values as strings. Type conversion belongs to
DWD so malformed values remain visible and can be counted instead of silently
turning into indistinguishable nulls during CSV parsing.
"""

from pyspark.sql.types import StringType, StructField, StructType


# Current teacher CSV files use values such as ``18/11/2014 17:11`` and
# ``3/12/2014 21:02`` for charging and battery timestamps.
SOURCE_TIMESTAMP_FORMAT = "d/M/yyyy H:mm"


def _raw_string_schema(field_names):
    fields = [StructField(name, StringType(), True) for name in field_names]
    fields.append(StructField("_corrupt_record", StringType(), True))
    return StructType(fields)


CHARGING_ODS_COLUMNS = (
    "sessionId",
    "kwhTotal",
    "charging_fees",
    "created",
    "ended",
    "startTime",
    "endTime",
    "chargeTimeHrs",
    "weekday",
    "platform",
    "userId",
    "stationId",
    "locationId",
    "managerVehicle",
    "facilityType",
    "Mon",
    "Tues",
    "Wed",
    "Thurs",
    "Fri",
    "Sat",
    "Sun",
)

STATION_ODS_COLUMNS = (
    "stationId",
    "locationId",
    "facilityType",
    "station_name",
    "address",
    "device_count",
    "open_time",
    "update_time",
)

BATTERY_ODS_COLUMNS = (
    "esd",
    "record_time",
    "soc",
    "pack_voltage (V)",
    "charge_current (A)",
    "max_cell_voltage (V)",
    "min_cell_voltage (V)",
    "max_temperature (℃)",
    "min_temperature (℃)",
    "available_energy (kw)",
    "available_capacity (Ah)",
)

CHARGING_ODS_SCHEMA = _raw_string_schema(CHARGING_ODS_COLUMNS)
STATION_ODS_SCHEMA = _raw_string_schema(STATION_ODS_COLUMNS)
BATTERY_ODS_SCHEMA = _raw_string_schema(BATTERY_ODS_COLUMNS)
