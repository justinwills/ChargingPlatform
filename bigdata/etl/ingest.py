"""CSV 到 ODS 的读取器，支持本地路径或 HDFS URI。"""

from pyspark.sql import DataFrame, SparkSession, functions as F

from .schemas import BATTERY_ODS_SCHEMA, CHARGING_ODS_SCHEMA, STATION_ODS_SCHEMA


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


def read_charging_ods(spark: SparkSession, path: str) -> DataFrame:
    """读取 nvv2t.csv，同时保留格式错误的源数据值。"""
    return _read_csv(spark, path, CHARGING_ODS_SCHEMA)


def read_station_ods(spark: SparkSession, path: str) -> DataFrame:
    """将 nvv2t_md_end.csv 作为充电站维度数据源读取。"""
    return _read_csv(spark, path, STATION_ODS_SCHEMA)


def read_battery_ods(spark: SparkSession, path: str) -> DataFrame:
    """将 dsv13r2.csv 作为原始电池遥测数据读取。"""
    return _read_csv(spark, path, BATTERY_ODS_SCHEMA)
