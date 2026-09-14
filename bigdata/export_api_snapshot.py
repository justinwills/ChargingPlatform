"""Export existing Spark warehouse metrics as one dashboard API snapshot."""

from __future__ import annotations

import argparse
import json
import os
import sys
import tempfile
from datetime import date, datetime, timezone
from decimal import Decimal
from pathlib import Path
from typing import Any, Iterable, Optional

if __package__ in (None, ""):
    sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from pyspark.sql import DataFrame, SparkSession


def _json_value(value: Any) -> Any:
    if isinstance(value, Decimal):
        return float(value)
    if isinstance(value, (date, datetime)):
        return value.isoformat()
    if isinstance(value, dict):
        return {key: _json_value(item) for key, item in value.items()}
    if isinstance(value, (list, tuple)):
        return [_json_value(item) for item in value]
    return value


def _rows(df: DataFrame) -> list[dict[str, Any]]:
    return [_json_value(row.asDict(recursive=True)) for row in df.collect()]


def _single_row(df: DataFrame) -> dict[str, Any]:
    row = df.first()
    return _json_value(row.asDict(recursive=True)) if row is not None else {}


def build_snapshot(spark: SparkSession, warehouse_root: str) -> dict[str, Any]:
    """Read the existing ADS/DWS tables and build one chart-friendly payload."""
    root = warehouse_root.rstrip("/")
    return {
        "code": 0,
        "msg": "ok",
        "generatedAt": datetime.now(timezone.utc).isoformat(),
        "data": {
            "overview": _single_row(
                spark.read.parquet(f"{root}/ads/overall_charging_kpis")
            ),
            "userSummary": _single_row(
                spark.read.parquet(f"{root}/ads/user_summary_kpis")
            ),
            "stations": _rows(spark.read.parquet(f"{root}/dws/station_kpis")),
            "users": _rows(spark.read.parquet(f"{root}/dws/user_kpis")),
            "devices": _rows(spark.read.parquet(f"{root}/dws/device_kpis")),
            "weekdays": _rows(
                spark.read.parquet(f"{root}/dws/weekday_patterns").orderBy(
                    "start_weekday"
                )
            ),
            "holidays": _rows(
                spark.read.parquet(f"{root}/dws/holiday_patterns")
            ),
            "weatherImpact": _rows(
                spark.read.parquet(f"{root}/dws/weather_impact")
            ),
            "deviceOperations": _rows(
                spark.read.parquet(f"{root}/dws/device_operation_kpis")
            ),
            "hourlyDistribution": _rows(
                spark.read.parquet(f"{root}/dws/hourly_distribution").orderBy("hour")
            ),
            "weekdayHeatmap": _rows(
                spark.read.parquet(f"{root}/dws/weekday_heatmap").orderBy(
                    "hour", "weekday"
                )
            ),
            "sessionCountTrend": _rows(
                spark.read.parquet(f"{root}/dws/session_count_trend").orderBy(
                    "start_date"
                )
            ),
            "stationRanking": _rows(
                spark.read.parquet(f"{root}/dws/station_ranking")
            ),
            "stationDistribution": _rows(
                spark.read.parquet(f"{root}/dws/station_distribution")
            ),
            "quality": _rows(spark.read.parquet(f"{root}/ads/data_quality")),
        },
    }


def write_snapshot(payload: dict[str, Any], output_path: str) -> Path:
    """Atomically replace the local JSON file served by ChargingServer."""
    output = Path(output_path).expanduser().resolve()
    output.parent.mkdir(parents=True, exist_ok=True)

    descriptor, temporary_name = tempfile.mkstemp(
        dir=output.parent, prefix=f".{output.name}.", suffix=".tmp"
    )
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8") as temporary_file:
            json.dump(payload, temporary_file, ensure_ascii=False, separators=(",", ":"))
            temporary_file.write("\n")
        os.replace(temporary_name, output)
    except BaseException:
        try:
            os.unlink(temporary_name)
        except FileNotFoundError:
            pass
        raise
    return output


def _parse_args(argv: Optional[Iterable[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--warehouse-root",
        required=True,
        help="HDFS warehouse root containing the existing ads/ and dws/ tables",
    )
    parser.add_argument(
        "--output",
        default="dashboard/data/bigdata.json",
        help="Local JSON path read by ChargingServer",
    )
    return parser.parse_args(argv)


def main(argv: Optional[Iterable[str]] = None) -> None:
    args = _parse_args(argv)
    spark = SparkSession.builder.appName("ChargingPlatform-ExportApi").getOrCreate()
    try:
        output = write_snapshot(
            build_snapshot(spark, args.warehouse_root), args.output
        )
        print(f"API snapshot written to {output}")
    finally:
        spark.stop()


if __name__ == "__main__":
    main()
