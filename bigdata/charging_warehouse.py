"""Build ODS, DWD, DWS and ADS tables from the six current CSV inputs."""

import argparse
import os
import sys

if __package__ in (None, ""):
    sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from pyspark.sql import DataFrame, SparkSession, functions as F

from bigdata.analytics.charging_stats import (
    holiday_patterns,
    hourly_distribution,
    overall_charging_kpis,
    rank_stations,
    session_count_trend,
    station_distribution,
    station_kpis,
    user_kpis,
    user_summary_kpis,
    weekday_hour_heatmap,
    weekday_patterns,
)
from bigdata.analytics.device_stats import device_kpis, device_operation_kpis
from bigdata.analytics.weather_stats import weather_impact_kpis
from bigdata.etl.clean_charging import (
    clean_charging_data,
    detect_charging_quality_issues,
    detect_charging_reference_issues,
    quality_issue_summary,
    rejected_charging_data,
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


def clean_station_dimension(df: DataFrame) -> DataFrame:
    """Compatibility wrapper for the station cleaner used by older callers."""
    return clean_stations(df)


def _write(df: DataFrame, path: str, mode: str) -> None:
    df.write.mode(mode).parquet(path)


def _valid_devices(devices: DataFrame, stations: DataFrame) -> DataFrame:
    station_keys = stations.select("station_id").dropDuplicates()
    return devices.join(F.broadcast(station_keys), "station_id", "left_semi")


def _valid_weather(weather: DataFrame, stations: DataFrame) -> DataFrame:
    location_keys = stations.select("location_id").dropDuplicates()
    return weather.join(F.broadcast(location_keys), "location_id", "left_semi")


def _valid_device_status(status: DataFrame, devices: DataFrame) -> DataFrame:
    references = devices.select(
        "device_id", F.col("station_id").alias("_expected_station_id")
    ).dropDuplicates(["device_id"])
    return (
        status.join(F.broadcast(references), "device_id", "inner")
        .where(F.col("station_id") == F.col("_expected_station_id"))
        .drop("_expected_station_id")
    )


def build_warehouse(
    spark: SparkSession,
    *,
    charging_input: str,
    users_input: str,
    station_input: str,
    devices_input: str,
    weather_input: str,
    device_status_input: str,
    warehouse_root: str,
    mode: str = "overwrite",
) -> None:
    root = warehouse_root.rstrip("/")
    readers = {
        "charging_orders": (read_charging_orders_ods, charging_input),
        "users": (read_users_ods, users_input),
        "stations": (read_stations_ods, station_input),
        "devices": (read_devices_ods, devices_input),
        "weather_hourly": (read_weather_ods, weather_input),
        "device_status_log": (read_device_status_ods, device_status_input),
    }

    for name, (reader, source) in readers.items():
        _write(reader(spark, source), f"{root}/ods/{name}", mode)

    ods_orders = spark.read.parquet(f"{root}/ods/charging_orders")
    ods_users = spark.read.parquet(f"{root}/ods/users")
    ods_stations = spark.read.parquet(f"{root}/ods/stations")
    ods_devices = spark.read.parquet(f"{root}/ods/devices")
    ods_weather = spark.read.parquet(f"{root}/ods/weather_hourly")
    ods_status = spark.read.parquet(f"{root}/ods/device_status_log")

    dwd_users = clean_users(ods_users)
    dwd_stations = clean_stations(ods_stations)
    dwd_devices = _valid_devices(clean_devices(ods_devices), dwd_stations)
    dwd_weather = _valid_weather(clean_weather(ods_weather), dwd_stations)
    dwd_status = _valid_device_status(clean_device_status(ods_status), dwd_devices)

    checked = detect_charging_quality_issues(ods_orders)
    checked = detect_charging_reference_issues(
        checked,
        users=dwd_users,
        stations=dwd_stations,
        devices=dwd_devices,
        weather=dwd_weather,
    ).cache()
    dwd_orders = clean_charging_data(checked)
    rejected = rejected_charging_data(checked)
    quality_summary = quality_issue_summary(checked)

    dwd_tables = {
        "charging_orders": dwd_orders,
        "charging_rejects": rejected,
        "users": dwd_users,
        "stations": dwd_stations,
        "devices": dwd_devices,
        "weather_hourly": dwd_weather,
        "device_status_log": dwd_status,
    }
    for name, frame in dwd_tables.items():
        _write(frame, f"{root}/dwd/{name}", mode)
    _write(quality_summary, f"{root}/ads/data_quality", mode)
    checked.unpersist()

    # Read the facts/dimension back so aggregate lineage is ODS -> DWD -> DWS/ADS.
    dwd_orders = spark.read.parquet(f"{root}/dwd/charging_orders")
    dwd_stations = spark.read.parquet(f"{root}/dwd/stations")
    dwd_devices = spark.read.parquet(f"{root}/dwd/devices")
    dwd_weather = spark.read.parquet(f"{root}/dwd/weather_hourly")
    dwd_status = spark.read.parquet(f"{root}/dwd/device_status_log")

    outputs = {
        "dws/station_kpis": station_kpis(dwd_orders, dwd_stations),
        "dws/user_kpis": user_kpis(dwd_orders),
        "dws/device_kpis": device_kpis(dwd_orders, dwd_devices),
        "dws/weekday_patterns": weekday_patterns(dwd_orders),
        "dws/holiday_patterns": holiday_patterns(dwd_orders),
        "dws/weather_impact": weather_impact_kpis(
            dwd_orders, dwd_weather, dwd_stations
        ),
        "dws/device_operation_kpis": device_operation_kpis(
            dwd_status, dwd_devices
        ),
        "ads/overall_charging_kpis": overall_charging_kpis(dwd_orders),
        "ads/user_summary_kpis": user_summary_kpis(dwd_orders),
        "dws/hourly_distribution": hourly_distribution(dwd_orders),
        "dws/weekday_heatmap": weekday_hour_heatmap(dwd_orders),
        "dws/session_count_trend": session_count_trend(dwd_orders),
        "dws/station_ranking": rank_stations(dwd_orders, dwd_stations),
        "dws/station_distribution": station_distribution(dwd_stations),
    }
    for relative_path, frame in outputs.items():
        _write(frame, f"{root}/{relative_path}", mode)


def _join(root: str, filename: str) -> str:
    return f"{root.rstrip('/')}/{filename}"


def _parse_args(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input-root", help="六张 CSV 所在目录（本地路径或 HDFS URI）")
    parser.add_argument("--charging-input", help="charging_orders.csv 路径")
    parser.add_argument("--users-input", help="users.csv 路径")
    parser.add_argument("--station-input", help="stations.csv 路径")
    parser.add_argument("--devices-input", help="devices.csv 路径")
    parser.add_argument("--weather-input", help="weather_hourly.csv 路径")
    parser.add_argument("--device-status-input", help="device_status_log.csv 路径")
    parser.add_argument("--warehouse-root", required=True, help="数仓输出根目录")
    parser.add_argument("--mode", choices=("overwrite", "error"), default="overwrite")
    args = parser.parse_args(argv)

    names = {
        "charging_input": "charging_orders.csv",
        "users_input": "users.csv",
        "station_input": "stations.csv",
        "devices_input": "devices.csv",
        "weather_input": "weather_hourly.csv",
        "device_status_input": "device_status_log.csv",
    }
    for attribute, filename in names.items():
        if getattr(args, attribute) is None and args.input_root:
            setattr(args, attribute, _join(args.input_root, filename))
    missing = [option.replace("_", "-") for option in names if not getattr(args, option)]
    if missing:
        parser.error("请提供 --input-root，或分别提供：" + ", ".join(f"--{name}" for name in missing))
    return args


def main() -> None:
    args = _parse_args()
    spark = (
        SparkSession.builder.appName("ChargingWarehouse")
        .config("spark.sql.session.timeZone", "Asia/Shanghai")
        .config("spark.sql.ansi.enabled", "false")
        .config("spark.sql.parquet.int96RebaseModeInWrite", "CORRECTED")
        .config("spark.sql.parquet.datetimeRebaseModeInWrite", "CORRECTED")
        .getOrCreate()
    )
    try:
        build_warehouse(
            spark,
            charging_input=args.charging_input,
            users_input=args.users_input,
            station_input=args.station_input,
            devices_input=args.devices_input,
            weather_input=args.weather_input,
            device_status_input=args.device_status_input,
            warehouse_root=args.warehouse_root,
            mode=args.mode,
        )
    finally:
        spark.stop()


if __name__ == "__main__":
    main()
