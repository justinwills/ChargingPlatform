"""Train a leakage-safe linear-regression baseline for charging load."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any, Iterable, Optional, Sequence

import pandas as pd

PROJECT_ROOT = Path(__file__).resolve().parents[1]
if str(PROJECT_ROOT) not in sys.path:
    sys.path.insert(0, str(PROJECT_ROOT))
from ml.features import FEATURE_COLUMNS
from ml.split import time_based_split

DEFAULT_DATASET = str(Path(__file__).resolve().parents[1] / "data" / "processed" / "ml_training_dataset.csv")


def _is_spark_dataframe(value: Any) -> bool:
    return value.__class__.__module__.startswith("pyspark.sql")


def _resolve_features(frame, feature_columns: Optional[Sequence[str]] = None, label_column: str = "energy_kwh"):
    available = list(frame.columns)
    columns = list(feature_columns) if feature_columns is not None else [c for c in FEATURE_COLUMNS if c in available]
    if feature_columns is None and not columns:
        columns = [c for c in available if c not in {"station_id", "location_id", "datetime", label_column}]
    missing = [c for c in columns if c not in available]
    if missing:
        raise ValueError("Training data is missing feature columns: " + ", ".join(missing))
    if not columns:
        raise ValueError("No numeric feature columns were supplied")
    return columns


def _metrics_pandas(actual, predicted) -> dict[str, float]:
    import numpy as np

    y, p = np.asarray(actual, dtype=float), np.asarray(predicted, dtype=float)
    error = p - y
    nonzero = np.abs(y) > 1e-12
    variance = np.sum((y - np.mean(y)) ** 2) if len(y) else 0.0
    return {
        "mae": float(np.mean(np.abs(error))) if len(y) else 0.0,
        "rmse": float(np.sqrt(np.mean(error ** 2))) if len(y) else 0.0,
        "mape": float(np.mean(np.abs(error[nonzero] / y[nonzero])) * 100) if nonzero.any() else 0.0,
        "r2": float(1 - np.sum(error ** 2) / variance) if variance else 0.0,
    }


def _metrics_spark(predictions, label_column: str):
    from pyspark.sql import functions as F
    from pyspark.ml.evaluation import RegressionEvaluator

    errors = predictions.select(F.col(label_column).cast("double").alias("_actual"), F.col("prediction").cast("double").alias("_prediction")).dropna()
    row = errors.select(
        F.avg(F.abs(F.col("_prediction") - F.col("_actual"))).alias("mae"),
        F.sqrt(F.avg(F.pow(F.col("_prediction") - F.col("_actual"), 2))).alias("rmse"),
        F.avg(F.when(F.abs(F.col("_actual")) > 1e-12, F.abs((F.col("_prediction") - F.col("_actual")) / F.col("_actual")) * 100)).alias("mape"),
        F.count("*").alias("n"),
    ).first()
    result = {name: float(row[name] or 0.0) for name in ("mae", "rmse", "mape")}
    result["r2"] = float(RegressionEvaluator(labelCol=label_column, predictionCol="prediction", metricName="r2").evaluate(predictions)) if row.n else 0.0
    return result


def _prepare_pandas(data, feature_columns, label_column):
    frame = data.copy()
    if label_column not in frame.columns or "datetime" not in frame.columns:
        raise ValueError(f"Training data must contain {label_column!r} and 'datetime'")
    frame["datetime"] = pd.to_datetime(frame["datetime"], errors="coerce")
    for column in [label_column, *feature_columns]:
        frame[column] = pd.to_numeric(frame[column], errors="coerce")
    return frame.dropna(subset=["datetime", label_column, *feature_columns]).reset_index(drop=True)


def _train_pandas(data, feature_columns, label_column, train_ratio, validation_ratio, test_ratio, **model_kwargs):
    from sklearn.linear_model import LinearRegression

    frame = _prepare_pandas(data, feature_columns, label_column)
    train, validation, test = time_based_split(frame, train_ratio, validation_ratio, test_ratio)
    if train.empty:
        raise ValueError("Training split is empty; adjust split ratios or provide more rows")
    model = LinearRegression(**model_kwargs).fit(train[feature_columns], train[label_column])
    val_prediction = model.predict(validation[feature_columns]) if len(validation) else []
    test_prediction = model.predict(test[feature_columns]) if len(test) else []
    return {
        "model_name": "linear_regression", "model": model, "feature_columns": feature_columns,
        "label_column": label_column, "train_rows": len(train), "validation_rows": len(validation), "test_rows": len(test),
        "validation_metrics": _metrics_pandas(validation[label_column], val_prediction),
        "test_metrics": _metrics_pandas(test[label_column], test_prediction),
        "predictions": pd.DataFrame({"actual": test[label_column].to_numpy(), "prediction": test_prediction}),
    }


def _prepare_spark(data, feature_columns, label_column):
    from pyspark.sql import functions as F

    if label_column not in data.columns or "datetime" not in data.columns:
        raise ValueError(f"Training data must contain {label_column!r} and 'datetime'")
    frame = data.withColumn(label_column, F.col(label_column).cast("double"))
    for column in feature_columns:
        frame = frame.withColumn(column, F.col(column).cast("double"))
    return frame.dropna(subset=[label_column, *feature_columns])


def _train_spark(data, feature_columns, label_column, train_ratio, validation_ratio, test_ratio, **model_kwargs):
    from pyspark.ml import Pipeline
    from pyspark.ml.feature import VectorAssembler
    from pyspark.ml.regression import LinearRegression
    from pyspark import StorageLevel

    # The chronological split and metric calculation each trigger Spark
    # actions. Persist once so the large shared-folder CSV is not reparsed and
    # resorted for every action.
    frame = _prepare_spark(data, feature_columns, label_column).persist(StorageLevel.MEMORY_AND_DISK)
    train, validation, test = time_based_split(frame, train_ratio, validation_ratio, test_ratio)
    if train.limit(1).count() == 0:
        raise ValueError("Training split is empty; adjust split ratios or provide more rows")
    model = Pipeline(stages=[VectorAssembler(inputCols=list(feature_columns), outputCol="features", handleInvalid="skip"), LinearRegression(featuresCol="features", labelCol=label_column, **model_kwargs)]).fit(train)
    val_prediction, test_prediction = model.transform(validation), model.transform(test)
    identifiers = [c for c in ("station_id", "location_id", "datetime") if c in test_prediction.columns]
    result = {
        "model_name": "linear_regression", "model": model, "feature_columns": list(feature_columns), "label_column": label_column,
        "train_rows": train.count(), "validation_rows": validation.count(), "test_rows": test.count(),
        "validation_metrics": _metrics_spark(val_prediction, label_column), "test_metrics": _metrics_spark(test_prediction, label_column),
        "predictions": test_prediction.select(*(identifiers + [label_column, "prediction"])),
    }
    frame.unpersist()
    return result


def train_linear_baseline(data, feature_columns: Optional[Sequence[str]] = None, label_column: str = "energy_kwh", train_ratio: float = 0.7, validation_ratio: float = 0.15, test_ratio: float = 0.15, **model_kwargs):
    """Fit and evaluate a linear-regression load baseline chronologically."""
    columns = _resolve_features(data, feature_columns, label_column)
    if _is_spark_dataframe(data):
        return _train_spark(data, columns, label_column, train_ratio, validation_ratio, test_ratio, **model_kwargs)
    if isinstance(data, pd.DataFrame):
        return _train_pandas(data, columns, label_column, train_ratio, validation_ratio, test_ratio, **model_kwargs)
    raise TypeError("data must be a pandas or PySpark DataFrame")


def _parse_args(argv: Optional[Iterable[str]] = None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", default=DEFAULT_DATASET)
    parser.add_argument("--model-output", default=None)
    parser.add_argument("--master", default=None)
    parser.add_argument("--train-ratio", type=float, default=0.7)
    parser.add_argument("--validation-ratio", type=float, default=0.15)
    parser.add_argument("--test-ratio", type=float, default=0.15)
    return parser.parse_args(argv)


def main(argv: Optional[Iterable[str]] = None):
    args = _parse_args(argv)
    from pyspark.sql import SparkSession

    builder = SparkSession.builder.appName("ChargingPlatform-LinearBaseline").config("spark.ui.enabled", "false")
    if args.master:
        builder = builder.master(args.master)
    spark = builder.getOrCreate()
    try:
        frame = spark.read.option("header", True).option("inferSchema", True).csv(args.input)
        result = train_linear_baseline(frame, train_ratio=args.train_ratio, validation_ratio=args.validation_ratio, test_ratio=args.test_ratio)
        if args.model_output:
            result["model"].write().overwrite().save(args.model_output)
        print(json.dumps({k: v for k, v in result.items() if k not in {"model", "predictions"}}, ensure_ascii=False, indent=2))
    finally:
        spark.stop()


if __name__ == "__main__":
    main()
