"""使用 PySpark 加载教师提供的 CSV 数据，并将其写入 HDFS。

Spark 的 CSV 写入器输出的是一个包含 ``part-*.csv`` 文件的目录，而不是像
``pandas.DataFrame.to_csv`` 那样输出单个文件。这是 Hadoop 的标准存储布局，
允许执行器并行写入各个分区。
"""

from __future__ import annotations

import argparse
import os
import sys
from pathlib import Path
from typing import Iterable, Optional

from pyspark.sql import DataFrame, SparkSession, functions as F
from pyspark.sql.types import StringType

if __package__:
    from .ingest import read_battery_ods, read_charging_ods, read_station_ods
else:
    # ``spark-submit bigdata/etl/load_data.py`` 会将此文件作为普通脚本运行。
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
    from bigdata.etl.ingest import (  # type: ignore[no-redef]
        read_battery_ods,
        read_charging_ods,
        read_station_ods,
    )


DATA_ROOT = os.environ.get(
    "CHARGING_DATA_ROOT", "hdfs:///charging-platform/data"
).rstrip("/")
RAW_DIR = f"{DATA_ROOT}/raw"
PROCESSED_DIR = f"{DATA_ROOT}/processed"


def _uri_join(root: str, *parts: str) -> str:
    """不使用 pathlib，拼接 HDFS、S3 或本地 URI 的各个部分。"""
    return "/".join([root.rstrip("/"), *(part.strip("/") for part in parts)])


def _trim_strings(df: DataFrame) -> DataFrame:
    for field in df.schema.fields:
        if isinstance(field.dataType, StringType):
            df = df.withColumn(field.name, F.trim(F.col(field.name)))
    return df


def _try_cast(column: str, spark_type: str):
    """使用 Spark 3.x 的宽松转换方式转换源数据值。"""
    return F.col(column).cast(spark_type)


def _business_columns(df: DataFrame) -> list[str]:
    return [name for name in df.columns if not name.startswith("_")]


def _save_processed(
    df: DataFrame,
    output_path: str,
    *,
    output_format: str = "csv",
    mode: str = "overwrite",
    partitions: Optional[int] = None,
) -> str:
    """保存分布式 DataFrame，并返回其 HDFS 或本地 URI。"""
    if partitions is not None:
        if partitions < 1:
            raise ValueError("partitions 至少必须为 1")
        df = df.coalesce(partitions)

    writer = df.write.mode(mode)
    if output_format == "csv":
        writer.option("header", True).option("encoding", "UTF-8").csv(output_path)
    elif output_format == "parquet":
        writer.option("compression", "snappy").parquet(output_path)
    else:
        raise ValueError("output_format must be 'csv' or 'parquet'")
    return output_path


def _output_path(name: str, output_format: str, output_root: Optional[str]) -> str:
    root = (output_root or PROCESSED_DIR).rstrip("/")
    return _uri_join(root, f"{name}.{output_format}")


def load_charging_orders(
    spark: SparkSession,
    path: Optional[str] = None,
    *,
    output_path: Optional[str] = None,
    output_format: str = "csv",
    mode: str = "overwrite",
    partitions: Optional[int] = None,
) -> DataFrame:
    """任务 #61：清洗充电订单，并将结果保存到 HDFS。"""
    source = str(path or _uri_join(RAW_DIR, "nvv2t.csv"))
    df = _trim_strings(read_charging_ods(spark, source))

    casts = {
        "kwhTotal": "double",
        "charging_fees": "double",
        "created": "timestamp",
        "ended": "timestamp",
        "startTime": "int",
        "endTime": "int",
        "chargeTimeHrs": "double",
        "stationId": "long",
        "locationId": "long",
        "Mon": "int",
        "Tues": "int",
        "Wed": "int",
        "Thurs": "int",
        "Fri": "int",
        "Sat": "int",
        "Sun": "int",
    }
    for column, spark_type in casts.items():
        df = df.withColumn(column, _try_cast(column, spark_type))

    df = (
        df.filter(F.col("sessionId").isNotNull() & (F.col("sessionId") != ""))
        .filter(F.col("userId").isNotNull() & (F.col("userId") != ""))
        .filter(F.col("stationId").isNotNull())
        .filter(F.col("kwhTotal").isNotNull() & (F.col("kwhTotal") >= 0))
        .filter(F.col("charging_fees").isNotNull() & (F.col("charging_fees") >= 0))
        .filter(
            F.col("chargeTimeHrs").isNotNull()
            & F.col("chargeTimeHrs").between(0, 24)
        )
        .dropDuplicates(["sessionId"])
        .orderBy("sessionId")
    )

    target = output_path or _output_path("charging_orders", output_format, None)
    _save_processed(
        df,
        target,
        output_format=output_format,
        mode=mode,
        partitions=partitions,
    )
    return df


def load_stations(
    spark: SparkSession,
    path: Optional[str] = None,
    *,
    output_path: Optional[str] = None,
    output_format: str = "csv",
    mode: str = "overwrite",
    partitions: Optional[int] = None,
) -> DataFrame:
    """任务 #62：清洗充电站维度数据，并将结果保存到 HDFS。"""
    source = str(path or _uri_join(RAW_DIR, "nvv2t_md_end.csv"))
    df = _trim_strings(read_station_ods(spark, source))

    df = (
        df.withColumn("stationId", _try_cast("stationId", "long"))
        .withColumn("locationId", _try_cast("locationId", "long"))
        .withColumn("device_count", _try_cast("device_count", "int"))
        .filter(F.col("stationId").isNotNull())
        .filter(F.col("station_name").isNotNull() & (F.col("station_name") != ""))
        .dropDuplicates(["stationId"])
        .orderBy("stationId")
    )

    target = output_path or _output_path("stations", output_format, None)
    _save_processed(
        df,
        target,
        output_format=output_format,
        mode=mode,
        partitions=partitions,
    )
    return df


def load_battery_telemetry(
    spark: SparkSession,
    path: Optional[str] = None,
    *,
    output_path: Optional[str] = None,
    output_format: str = "csv",
    mode: str = "overwrite",
    partitions: Optional[int] = None,
) -> DataFrame:
    """任务 #63：清洗电池遥测数据，并将结果保存到 HDFS。"""
    source = str(path or _uri_join(RAW_DIR, "dsv13r2.csv"))
    df = _trim_strings(read_battery_ods(spark, source))
    rename_map = {
        "pack_voltage (V)": "pack_voltage",
        "charge_current (A)": "charge_current",
        "max_cell_voltage (V)": "max_cell_voltage",
        "min_cell_voltage (V)": "min_cell_voltage",
        "max_temperature (℃)": "max_temperature",
        "min_temperature (℃)": "min_temperature",
        "available_energy (kw)": "available_energy",
        "available_capacity (Ah)": "available_capacity",
    }
    for old_name, new_name in rename_map.items():
        df = df.withColumnRenamed(old_name, new_name)

    df = df.withColumn("esd", _try_cast("esd", "long")).withColumn(
        "record_time",
        (_try_cast("record_time", "double") / F.lit(1000.0)).cast("timestamp"),
    )
    numeric_columns = (
        "soc",
        "pack_voltage",
        "charge_current",
        "max_cell_voltage",
        "min_cell_voltage",
        "max_temperature",
        "min_temperature",
        "available_energy",
        "available_capacity",
    )
    for column in numeric_columns:
        df = df.withColumn(column, _try_cast(column, "double"))

    df = (
        df.filter(F.col("esd").isNotNull())
        .filter(F.col("record_time").isNotNull())
        .filter(F.col("soc").isNotNull() & F.col("soc").between(0, 100))
        .filter(F.col("pack_voltage").isNotNull())
        .filter(F.col("available_capacity").isNotNull())
        .dropDuplicates(_business_columns(df))
        .orderBy("esd", "record_time")
    )

    target = output_path or _output_path("battery_telemetry", output_format, None)
    _save_processed(
        df,
        target,
        output_format=output_format,
        mode=mode,
        partitions=partitions,
    )
    return df


def _parse_args(argv: Optional[Iterable[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="将充电数据导入 HDFS")
    parser.add_argument("--data-root", default=DATA_ROOT, help="HDFS 或本地数据 URI")
    parser.add_argument("--format", choices=("csv", "parquet"), default="csv")
    parser.add_argument(
        "--mode",
        choices=("overwrite", "append", "error", "ignore"),
        default="overwrite",
    )
    parser.add_argument(
        "--partitions",
        type=int,
        default=None,
        help="可选的输出文件数量；省略时保留分布式分区",
    )
    return parser.parse_args(argv)


def main(argv: Optional[Iterable[str]] = None) -> None:
    args = _parse_args(argv)
    raw_root = _uri_join(args.data_root, "raw")
    output_root = _uri_join(args.data_root, "processed")
    spark = SparkSession.builder.appName("ChargingPlatform-LoadData").getOrCreate()
    spark.conf.set("spark.sql.session.timeZone", "UTC")
    spark.conf.set("spark.sql.ansi.enabled", "false")
    # The supplied charging timestamps use anonymized years such as 0014.
    # Preserve Spark 3's proleptic Gregorian values when writing Parquet.
    spark.conf.set("spark.sql.parquet.int96RebaseModeInWrite", "CORRECTED")
    spark.conf.set("spark.sql.parquet.datetimeRebaseModeInWrite", "CORRECTED")

    jobs = (
        (load_charging_orders, "nvv2t.csv", "charging_orders"),
        (load_stations, "nvv2t_md_end.csv", "stations"),
        (load_battery_telemetry, "dsv13r2.csv", "battery_telemetry"),
    )
    try:
        for loader, source_name, output_name in jobs:
            loader(
                spark,
                _uri_join(raw_root, source_name),
                output_path=_uri_join(
                    output_root, f"{output_name}.{args.format}"
                ),
                output_format=args.format,
                mode=args.mode,
                partitions=args.partitions,
            )
    finally:
        spark.stop()


if __name__ == "__main__":
    main()
