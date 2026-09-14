"""CSV-to-ODS readers for local paths and distributed filesystem URIs."""

from pyspark.sql import DataFrame, SparkSession, functions as F

from .schemas import (
    CHARGING_ORDER_ODS_SCHEMA,
    DEVICE_ODS_SCHEMA,
    DEVICE_STATUS_ODS_SCHEMA,
    STATION_ODS_SCHEMA,
    USER_ODS_SCHEMA,
    WEATHER_ODS_SCHEMA,
)


def _read_csv(spark: SparkSession, path: str, schema) -> DataFrame:
    if not path or not path.strip():
        raise ValueError("CSV input path must not be empty")

    return (
        spark.read.option("header", True)
        .option("encoding", "UTF-8")
        .option("mode", "PERMISSIVE")
        .option("columnNameOfCorruptRecord", "_corrupt_record")
        .schema(schema)
        .csv(path)
        .withColumn("_source_file", F.input_file_name())
        .withColumn("_ingested_at", F.current_timestamp())
    )


def read_charging_orders_ods(spark: SparkSession, path: str) -> DataFrame:
    return _read_csv(spark, path, CHARGING_ORDER_ODS_SCHEMA)


def read_users_ods(spark: SparkSession, path: str) -> DataFrame:
    return _read_csv(spark, path, USER_ODS_SCHEMA)


def read_stations_ods(spark: SparkSession, path: str) -> DataFrame:
    return _read_csv(spark, path, STATION_ODS_SCHEMA)


def read_devices_ods(spark: SparkSession, path: str) -> DataFrame:
    return _read_csv(spark, path, DEVICE_ODS_SCHEMA)


def read_weather_ods(spark: SparkSession, path: str) -> DataFrame:
    return _read_csv(spark, path, WEATHER_ODS_SCHEMA)


def read_device_status_ods(spark: SparkSession, path: str) -> DataFrame:
    return _read_csv(spark, path, DEVICE_STATUS_ODS_SCHEMA)


# Compatibility aliases for callers using the previous singular names.
read_charging_ods = read_charging_orders_ods
read_station_ods = read_stations_ods
