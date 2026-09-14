"""Build charging ODS, DWD, DWS and ADS tables with PySpark 3.3.x."""

import argparse
import os
import sys

if __package__ in (None, ""):
    sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from pyspark.sql import DataFrame, SparkSession, Window, functions as F

from bigdata.analytics.charging_stats import (
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
from bigdata.etl.clean_charging import (
    clean_charging_data,
    detect_charging_quality_issues,
    quality_issue_summary,
    rejected_charging_data,
)
from bigdata.etl.ingest import read_charging_ods, read_station_ods

# 清洗充电站维度数据。
def clean_station_dimension(df: DataFrame) -> DataFrame:
    """Create the typed station dimension needed by station KPIs."""
    required = {
        "stationId",
        "locationId",
        "facilityType",
        "station_name",
        "address",
        "device_count",
        "open_time",
        "update_time",
    }
    missing = sorted(required - set(df.columns))
    if missing:
        raise ValueError("Missing station source columns: " + ", ".join(missing))

    first_station_row = Window.partitionBy(F.trim("stationId")).orderBy(
        F.trim("station_name"), F.trim("address")
    )
    return (
        df.withColumn("station_id", F.trim("stationId"))
        .withColumn("device_count", F.trim("device_count").cast("int"))
        .withColumn("_station_row_number", F.row_number().over(first_station_row))
        .where(
            F.col("station_id").isNotNull()
            & (F.col("station_id") != "")
            & F.col("device_count").isNotNull()
            & (F.col("device_count") >= 0)
            & (F.col("_station_row_number") == 1)
        )
        .select(
            "station_id",
            F.trim("station_name").alias("station_name"),
            F.trim("address").alias("address"),
            F.trim("locationId").alias("location_id"),
            F.trim("facilityType").cast("int").alias("facility_type"),
            "device_count",
            F.trim("open_time").alias("open_time"),
            F.to_date(F.trim("update_time"), "yyyy/M/d").alias("updated_on"),
        )
    )


def _write(df: DataFrame, path: str, mode: str) -> None:
    df.write.mode(mode).parquet(path)


def build_warehouse(
    spark: SparkSession,
    charging_input: str,
    station_input: str,
    warehouse_root: str,
    mode: str = "overwrite",
) -> None:
    root = warehouse_root.rstrip("/")

    ods_charging = read_charging_ods(spark, charging_input)
    ods_stations = read_station_ods(spark, station_input)
    _write(ods_charging, f"{root}/ods/charging_sessions", mode)
    _write(ods_stations, f"{root}/ods/stations", mode)

    # Read ODS back so the lineage genuinely follows ODS -> DWD.
    ods_charging = spark.read.parquet(f"{root}/ods/charging_sessions")
    ods_stations = spark.read.parquet(f"{root}/ods/stations")

    checked = detect_charging_quality_issues(ods_charging).cache()
    dwd_charging = clean_charging_data(checked)
    rejected = rejected_charging_data(checked)
    quality_summary = quality_issue_summary(checked)
    dwd_stations = clean_station_dimension(ods_stations)

    _write(dwd_charging, f"{root}/dwd/charging_sessions", mode)
    _write(rejected, f"{root}/dwd/charging_rejects", mode)
    _write(dwd_stations, f"{root}/dwd/stations", mode)
    _write(quality_summary, f"{root}/ads/data_quality", mode)
    checked.unpersist()

    # Read DWD back before building SparkSQL aggregate tables.
    dwd_charging = spark.read.parquet(f"{root}/dwd/charging_sessions")
    dwd_stations = spark.read.parquet(f"{root}/dwd/stations")

    station_metrics = station_kpis(dwd_charging, dwd_stations)
    user_metrics = user_kpis(dwd_charging)
    user_summary = user_summary_kpis(dwd_charging)
    weekday_metrics = weekday_patterns(dwd_charging)
    overall_metrics = overall_charging_kpis(dwd_charging)
    hourly_metrics = hourly_distribution(dwd_charging)
    heatmap_metrics = weekday_hour_heatmap(dwd_charging)
    trend_metrics = session_count_trend(dwd_charging)
    ranking_metrics = rank_stations(dwd_charging, dwd_stations)
    distribution_metrics = station_distribution(dwd_stations)

    _write(station_metrics, f"{root}/dws/station_kpis", mode)
    _write(user_metrics, f"{root}/dws/user_kpis", mode)
    _write(weekday_metrics, f"{root}/dws/weekday_patterns", mode)
    _write(overall_metrics, f"{root}/ads/overall_charging_kpis", mode)
    _write(user_summary, f"{root}/ads/user_summary_kpis", mode)
    _write(hourly_metrics, f"{root}/dws/hourly_distribution", mode)
    _write(heatmap_metrics, f"{root}/dws/weekday_heatmap", mode)
    _write(trend_metrics, f"{root}/dws/session_count_trend", mode)
    _write(ranking_metrics, f"{root}/dws/station_ranking", mode)
    _write(distribution_metrics, f"{root}/dws/station_distribution", mode)


def _parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--charging-input", required=True, help="nvv2t.csv local/HDFS path")
    parser.add_argument("--station-input", required=True, help="nvv2t_md_end.csv local/HDFS path")
    parser.add_argument(
        "--warehouse-root",
        required=True,
        help="warehouse output root, e.g. hdfs:///charging",
    )
    parser.add_argument("--mode", choices=("overwrite", "error"), default="overwrite")
    return parser.parse_args()


def main() -> None:
    args = _parse_args()
    spark = (
        SparkSession.builder.appName("ChargingWarehouse")
        .config("spark.sql.ansi.enabled", "false")
        # Keep Spark 3's corrected calendar behavior in Parquet outputs.
        .config("spark.sql.parquet.int96RebaseModeInWrite", "CORRECTED")
        .config("spark.sql.parquet.datetimeRebaseModeInWrite", "CORRECTED")
        .getOrCreate()
    )
    try:
        build_warehouse(
            spark,
            charging_input=args.charging_input,
            station_input=args.station_input,
            warehouse_root=args.warehouse_root,
            mode=args.mode,
        )
    finally:
        spark.stop()


if __name__ == "__main__":
    main()
