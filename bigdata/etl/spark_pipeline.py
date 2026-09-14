"""Spark/SparkSQL ETL for the charging-platform source datasets."""

from pathlib import Path


def _create_spark_session(app_name, master=None):
    try:
        from pyspark.sql import SparkSession
    except ImportError as error:
        raise RuntimeError(
            "PySpark is required for the Spark ETL. Install requirements.txt first."
        ) from error

    builder = SparkSession.builder.appName(app_name)
    if master:
        builder = builder.master(master)
    return builder.getOrCreate()


def _read_csv(spark, input_path):
    dataframe = (
        spark.read.option("header", True)
        .option("inferSchema", True)
        .option("encoding", "UTF-8")
        .csv(str(input_path))
    )
    return dataframe.toDF(*(column.strip() for column in dataframe.columns)).dropna(
        how="all"
    )


def _load_dataset(input_path, output_path, app_name, master=None):
    spark = _create_spark_session(app_name, master)
    try:
        dataframe = _read_csv(spark, input_path)
        (
            dataframe.write.mode("overwrite")
            .option("header", True)
            .option("encoding", "UTF-8")
            .csv(str(output_path))
        )
        return dataframe
    finally:
        spark.stop()


def load_charging_orders_spark(
    input_path="hdfs:///charging/raw/nvv2t.csv",
    output_path="hdfs:///charging/processed/charging_orders",
    master=None,
):
    """Load Task #61 orders through a Spark DataFrame and write distributed CSV."""
    return _load_dataset(input_path, output_path, "ChargingOrdersImport", master)


def load_stations_spark(
    input_path="hdfs:///charging/raw/nvv2t_md_end.csv",
    output_path="hdfs:///charging/processed/charging_stations",
    master=None,
):
    """Load Task #62 station metadata through Spark and write distributed CSV."""
    return _load_dataset(input_path, output_path, "ChargingStationsImport", master)


def run_local_spark_import(project_root=None, master="local[*]"):
    """Run both imports with Spark against this repository's local raw files."""
    root = Path(project_root) if project_root else Path(__file__).resolve().parents[2]
    processed = root / "data" / "processed" / "spark"
    orders = load_charging_orders_spark(
        root / "data" / "raw" / "nvv2t.csv",
        processed / "charging_orders",
        master,
    )
    stations = load_stations_spark(
        root / "data" / "raw" / "nvv2t_md_end.csv",
        processed / "charging_stations",
        master,
    )
    return orders, stations