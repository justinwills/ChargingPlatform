"""Import and clean the six current charging-platform CSV tables with PySpark."""

from __future__ import annotations

import argparse
import os
import sys
from pathlib import Path
from typing import Callable, Iterable, Optional

from pyspark.sql import DataFrame, SparkSession, functions as F
from pyspark.sql.types import StringType

if __package__:
    from .clean_charging import clean_charging_orders
    from .clean_tables import clean_device_status, clean_devices, clean_stations, clean_users, clean_weather
    from .ingest import (
        read_charging_orders_ods,
        read_device_status_ods,
        read_devices_ods,
        read_stations_ods,
        read_users_ods,
        read_weather_ods,
    )
else:
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
    from bigdata.etl.clean_charging import clean_charging_orders
    from bigdata.etl.clean_tables import (
        clean_device_status, clean_devices, clean_stations, clean_users, clean_weather,
    )
    from bigdata.etl.ingest import (
        read_charging_orders_ods, read_device_status_ods, read_devices_ods,
        read_stations_ods, read_users_ods, read_weather_ods,
    )


DATA_ROOT = os.environ.get("CHARGING_DATA_ROOT", "hdfs:///charging-platform/data").rstrip("/")
RAW_DIR = f"{DATA_ROOT}/raw"
PROCESSED_DIR = f"{DATA_ROOT}/processed"


def _uri_join(root: str, *parts: str) -> str:
    return "/".join([root.rstrip("/"), *(part.strip("/") for part in parts)])


def _trim_strings(df: DataFrame) -> DataFrame:
    for field in df.schema.fields:
        if isinstance(field.dataType, StringType):
            df = df.withColumn(field.name, F.trim(F.col(field.name)))
    return df


def _valid_charging_measurements():
    """Reusable predicate for already-cast current order measurement columns."""
    return (
        F.col("energy_kwh").isNotNull() & (F.col("energy_kwh") >= 0)
        & F.col("fee_amount_cny").isNotNull() & (F.col("fee_amount_cny") >= 0)
        & F.col("charge_time_hrs").isNotNull()
        & (F.col("charge_time_hrs") > 0) & (F.col("charge_time_hrs") <= 24)
        & F.col("start_hour").between(0, 23)
        & F.col("weekday").between(0, 6)
    )


def _save_processed(
    df: DataFrame,
    output_path: str,
    *,
    output_format: str = "csv",
    mode: str = "overwrite",
    partitions: Optional[int] = None,
) -> str:
    if partitions is not None:
        if partitions < 1:
            raise ValueError("partitions 至少必须为 1")
        df = df.coalesce(partitions)

    writer = df.write.mode(mode)
    if output_format == "csv":
        (
            writer.option("header", True)
            .option("encoding", "UTF-8")
            .option("timestampFormat", "yyyy-MM-dd HH:mm:ss")
            .option("dateFormat", "yyyy-MM-dd")
            .csv(output_path)
        )
    elif output_format == "parquet":
        writer.option("compression", "snappy").parquet(output_path)
    else:
        raise ValueError("output_format must be 'csv' or 'parquet'")
    return output_path


def _output_path(name: str, output_format: str) -> str:
    return _uri_join(PROCESSED_DIR, f"{name}.{output_format}")


def _load_table(
    spark: SparkSession,
    path: str,
    reader: Callable[[SparkSession, str], DataFrame],
    cleaner: Callable[[DataFrame], DataFrame],
    *,
    output_path: str,
    output_format: str,
    mode: str,
    partitions: Optional[int],
) -> DataFrame:
    cleaned = cleaner(_trim_strings(reader(spark, path)))
    _save_processed(
        cleaned,
        output_path,
        output_format=output_format,
        mode=mode,
        partitions=partitions,
    )
    return cleaned


def load_charging_orders(
    spark: SparkSession, path: Optional[str] = None, *, output_path: Optional[str] = None,
    output_format: str = "csv", mode: str = "overwrite", partitions: Optional[int] = None,
) -> DataFrame:
    return _load_table(
        spark, str(path or _uri_join(RAW_DIR, "charging_orders.csv")),
        read_charging_orders_ods, clean_charging_orders,
        output_path=output_path or _output_path("charging_orders", output_format),
        output_format=output_format, mode=mode, partitions=partitions,
    )


def load_users(
    spark: SparkSession, path: Optional[str] = None, *, output_path: Optional[str] = None,
    output_format: str = "csv", mode: str = "overwrite", partitions: Optional[int] = None,
) -> DataFrame:
    return _load_table(
        spark, str(path or _uri_join(RAW_DIR, "users.csv")), read_users_ods, clean_users,
        output_path=output_path or _output_path("users", output_format),
        output_format=output_format, mode=mode, partitions=partitions,
    )


def load_stations(
    spark: SparkSession, path: Optional[str] = None, *, output_path: Optional[str] = None,
    output_format: str = "csv", mode: str = "overwrite", partitions: Optional[int] = None,
) -> DataFrame:
    return _load_table(
        spark, str(path or _uri_join(RAW_DIR, "stations.csv")), read_stations_ods, clean_stations,
        output_path=output_path or _output_path("stations", output_format),
        output_format=output_format, mode=mode, partitions=partitions,
    )


def load_devices(
    spark: SparkSession, path: Optional[str] = None, *, output_path: Optional[str] = None,
    output_format: str = "csv", mode: str = "overwrite", partitions: Optional[int] = None,
) -> DataFrame:
    return _load_table(
        spark, str(path or _uri_join(RAW_DIR, "devices.csv")), read_devices_ods, clean_devices,
        output_path=output_path or _output_path("devices", output_format),
        output_format=output_format, mode=mode, partitions=partitions,
    )


def load_weather_hourly(
    spark: SparkSession, path: Optional[str] = None, *, output_path: Optional[str] = None,
    output_format: str = "csv", mode: str = "overwrite", partitions: Optional[int] = None,
) -> DataFrame:
    return _load_table(
        spark, str(path or _uri_join(RAW_DIR, "weather_hourly.csv")), read_weather_ods, clean_weather,
        output_path=output_path or _output_path("weather_hourly", output_format),
        output_format=output_format, mode=mode, partitions=partitions,
    )


def load_device_status_log(
    spark: SparkSession, path: Optional[str] = None, *, output_path: Optional[str] = None,
    output_format: str = "csv", mode: str = "overwrite", partitions: Optional[int] = None,
) -> DataFrame:
    return _load_table(
        spark, str(path or _uri_join(RAW_DIR, "device_status_log.csv")),
        read_device_status_ods, clean_device_status,
        output_path=output_path or _output_path("device_status_log", output_format),
        output_format=output_format, mode=mode, partitions=partitions,
    )


def _parse_args(argv: Optional[Iterable[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="将六张充电平台 CSV 导入并清洗到 HDFS")
    parser.add_argument("--data-root", default=DATA_ROOT, help="包含 raw/ 和 processed/ 的 HDFS 或本地 URI")
    parser.add_argument("--input-root", help="可选：直接指定六张源 CSV 所在目录")
    parser.add_argument("--output-root", help="可选：直接指定清洗结果目录")
    parser.add_argument("--format", choices=("csv", "parquet"), default="csv")
    parser.add_argument("--mode", choices=("overwrite", "append", "error", "ignore"), default="overwrite")
    parser.add_argument("--partitions", type=int, default=None, help="可选的输出文件数量")
    return parser.parse_args(argv)


def main(argv: Optional[Iterable[str]] = None) -> None:
    args = _parse_args(argv)
    raw_root = args.input_root or _uri_join(args.data_root, "raw")
    output_root = args.output_root or _uri_join(args.data_root, "processed")
    spark = SparkSession.builder.appName("ChargingPlatform-LoadSixTables").getOrCreate()
    spark.conf.set("spark.sql.session.timeZone", "Asia/Shanghai")
    spark.conf.set("spark.sql.ansi.enabled", "false")
    spark.conf.set("spark.sql.parquet.int96RebaseModeInWrite", "CORRECTED")
    spark.conf.set("spark.sql.parquet.datetimeRebaseModeInWrite", "CORRECTED")

    jobs = (
        (load_users, "users.csv", "users"),
        (load_stations, "stations.csv", "stations"),
        (load_devices, "devices.csv", "devices"),
        (load_weather_hourly, "weather_hourly.csv", "weather_hourly"),
        (load_charging_orders, "charging_orders.csv", "charging_orders"),
        (load_device_status_log, "device_status_log.csv", "device_status_log"),
    )
    try:
        for loader, source_name, output_name in jobs:
            loader(
                spark,
                _uri_join(raw_root, source_name),
                output_path=_uri_join(output_root, f"{output_name}.{args.format}"),
                output_format=args.format,
                mode=args.mode,
                partitions=args.partitions,
            )
    finally:
        spark.stop()


if __name__ == "__main__":
    main()
