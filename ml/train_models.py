"""Train Random Forest and optional XGBoost charging-load models."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any, Iterable, Mapping, Optional, Sequence

import pandas as pd

PROJECT_ROOT = Path(__file__).resolve().parents[1]
if str(PROJECT_ROOT) not in sys.path:
    sys.path.insert(0, str(PROJECT_ROOT))
from ml.features import FEATURE_COLUMNS
from ml.split import time_based_split
from ml.train_baseline import (
    DEFAULT_DATASET,
    _is_spark_dataframe,
    _metrics_pandas,
    _metrics_spark,
    _prepare_pandas,
    _prepare_spark,
    _resolve_features,
)


def _train_pandas_model(data, model_name, feature_columns, label_column, train_ratio, validation_ratio, test_ratio, **kwargs):
    frame = _prepare_pandas(data, feature_columns, label_column)
    train, validation, test = time_based_split(frame, train_ratio, validation_ratio, test_ratio)
    if train.empty:
        raise ValueError("Training split is empty; adjust split ratios or provide more rows")
    if model_name == "random_forest":
        from sklearn.ensemble import RandomForestRegressor
        defaults = {"n_estimators": 30, "max_depth": 10, "random_state": 42, "n_jobs": -1}
        defaults.update(kwargs)
        model = RandomForestRegressor(**defaults)
    elif model_name == "xgboost":
        try:
            from xgboost import XGBRegressor
        except ImportError as error:
            raise RuntimeError("XGBoost is not installed; install requirements.txt or omit xgboost") from error
        defaults = {"n_estimators": 200, "max_depth": 8, "learning_rate": 0.05, "subsample": 0.8, "colsample_bytree": 0.8, "objective": "reg:squarederror", "random_state": 42, "n_jobs": -1}
        defaults.update(kwargs)
        model = XGBRegressor(**defaults)
    else:
        raise ValueError(f"Unsupported model: {model_name}")
    model.fit(train[feature_columns], train[label_column])
    val_prediction = model.predict(validation[feature_columns]) if len(validation) else []
    test_prediction = model.predict(test[feature_columns]) if len(test) else []
    return {
        "model_name": model_name, "model": model, "feature_columns": feature_columns, "label_column": label_column,
        "train_rows": len(train), "validation_rows": len(validation), "test_rows": len(test),
        "validation_metrics": _metrics_pandas(validation[label_column], val_prediction), "test_metrics": _metrics_pandas(test[label_column], test_prediction),
        "predictions": pd.DataFrame({"actual": test[label_column].to_numpy(), "prediction": test_prediction}),
    }


def _train_spark_model(data, model_name, feature_columns, label_column, train_ratio, validation_ratio, test_ratio, **kwargs):
    from pyspark.ml import Pipeline
    from pyspark.ml.feature import VectorAssembler
    from pyspark import StorageLevel

    frame = _prepare_spark(data, feature_columns, label_column).persist(StorageLevel.MEMORY_AND_DISK)
    train, validation, test = time_based_split(frame, train_ratio, validation_ratio, test_ratio)
    if train.limit(1).count() == 0:
        raise ValueError("Training split is empty; adjust split ratios or provide more rows")
    assembler = VectorAssembler(inputCols=list(feature_columns), outputCol="features", handleInvalid="skip")
    if model_name == "random_forest":
        from pyspark.ml.regression import RandomForestRegressor
        # Conservative defaults fit the teacher VM's limited memory.  Larger
        # forests can be requested from the CLI after a successful smoke run.
        defaults = {"numTrees": 30, "maxDepth": 10, "seed": 42, "subsamplingRate": 0.8}
        defaults.update(kwargs)
        estimator = RandomForestRegressor(featuresCol="features", labelCol=label_column, **defaults)
    elif model_name == "xgboost":
        try:
            from xgboost.spark import SparkXGBRegressor
        except ImportError as error:
            raise RuntimeError("XGBoost Spark support is unavailable; install xgboost>=2.0") from error
        defaults = {"n_estimators": 200, "max_depth": 8, "learning_rate": 0.05, "subsample": 0.8, "colsample_bytree": 0.8, "num_workers": 2, "objective": "reg:squarederror"}
        defaults.update(kwargs)
        estimator = SparkXGBRegressor(features_col="features", label_col=label_column, **defaults)
    else:
        raise ValueError(f"Unsupported model: {model_name}")
    model = Pipeline(stages=[assembler, estimator]).fit(train)
    val_prediction, test_prediction = model.transform(validation), model.transform(test)
    identifiers = [c for c in ("station_id", "location_id", "datetime") if c in test_prediction.columns]
    result = {
        "model_name": model_name, "model": model, "feature_columns": list(feature_columns), "label_column": label_column,
        "train_rows": train.count(), "validation_rows": validation.count(), "test_rows": test.count(),
        "validation_metrics": _metrics_spark(val_prediction, label_column), "test_metrics": _metrics_spark(test_prediction, label_column),
        "predictions": test_prediction.select(*(identifiers + [label_column, "prediction"])),
    }
    frame.unpersist()
    return result


def train_rf_xgboost(data, feature_columns: Optional[Sequence[str]] = None, label_column: str = "energy_kwh", models: Sequence[str] = ("random_forest", "xgboost"), train_ratio: float = 0.7, validation_ratio: float = 0.15, test_ratio: float = 0.15, **model_kwargs):
    """Train Random Forest and XGBoost models using chronological splits.

    XGBoost is optional: if its Spark integration is unavailable, the result
    records the error and still returns the Random Forest model.  The model
    with the lowest validation RMSE is exposed as ``best_model``.
    """
    columns = _resolve_features(data, feature_columns, label_column)
    results, failures = {}, {}
    for requested in models:
        name = requested.lower().replace("-", "_")
        try:
            options = model_kwargs.get(name, {}) if isinstance(model_kwargs.get(name, {}), dict) else {}
            # Also accept ordinary estimator kwargs (for example
            # ``n_estimators=200``) when callers train one model at a time.
            if not options:
                options = {key: value for key, value in model_kwargs.items() if key not in {"random_forest", "xgboost"}}
            if _is_spark_dataframe(data):
                result = _train_spark_model(data, name, columns, label_column, train_ratio, validation_ratio, test_ratio, **options)
            elif isinstance(data, pd.DataFrame):
                result = _train_pandas_model(data, name, columns, label_column, train_ratio, validation_ratio, test_ratio, **options)
            else:
                raise TypeError("data must be a pandas or PySpark DataFrame")
            results[name] = result
        except (ImportError, RuntimeError) as error:
            failures[name] = str(error)
    if not results:
        raise RuntimeError("No model could be trained: " + "; ".join(f"{k}: {v}" for k, v in failures.items()))
    best_name = min(results, key=lambda name: results[name]["validation_metrics"].get("rmse", float("inf")))
    return {"models": results, "failures": failures, "best_model_name": best_name, "best_model": results[best_name]["model"], "feature_columns": columns, "label_column": label_column, **results}


def save_best_model(result: Mapping[str, Any], path: str):
    """Persist the selected Spark or sklearn model to ``path``."""
    if not isinstance(result, Mapping):
        raise TypeError("result must be the mapping returned by train_rf_xgboost")
    model = result.get("best_model") or result.get("model")
    if model is None:
        raise ValueError("result does not contain a trained model")
    if hasattr(model, "write"):
        model.write().overwrite().save(path)
    else:
        import pickle
        Path(path).expanduser().parent.mkdir(parents=True, exist_ok=True)
        with open(path, "wb") as handle:
            pickle.dump(model, handle)
    return path


def _parse_args(argv: Optional[Iterable[str]] = None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", default=DEFAULT_DATASET)
    parser.add_argument("--model-output", default=None)
    parser.add_argument("--models", default="random_forest,xgboost")
    parser.add_argument("--master", default=None)
    parser.add_argument("--num-trees", type=int, default=30)
    parser.add_argument("--max-depth", type=int, default=10)
    return parser.parse_args(argv)


def main(argv: Optional[Iterable[str]] = None):
    args = _parse_args(argv)
    from pyspark.sql import SparkSession
    builder = SparkSession.builder.appName("ChargingPlatform-LoadModels").config("spark.ui.enabled", "false")
    # The teacher VM occasionally resolves its hostname to a stale VMware
    # adapter address.  Local mode only needs loopback, so make Driver
    # binding deterministic while still allowing --master to be overridden.
    if not args.master or args.master.startswith("local"):
        builder = builder.config("spark.driver.bindAddress", "127.0.0.1").config("spark.driver.host", "127.0.0.1")
    if args.master:
        builder = builder.master(args.master)
    spark = builder.getOrCreate()
    try:
        frame = spark.read.option("header", True).option("inferSchema", True).csv(args.input)
        model_names = tuple(x.strip() for x in args.models.split(",") if x.strip())
        result = train_rf_xgboost(
            frame,
            models=model_names,
            random_forest={"numTrees": args.num_trees, "maxDepth": args.max_depth},
            xgboost={"n_estimators": args.num_trees, "max_depth": args.max_depth},
        )
        if args.model_output:
            save_best_model(result, args.model_output)
        summary = {"best_model_name": result["best_model_name"], "failures": result["failures"], "models": {name: {k: value for k, value in item.items() if k not in {"model", "predictions"}} for name, item in result["models"].items()}}
        print(json.dumps(summary, ensure_ascii=False, indent=2))
    finally:
        spark.stop()


if __name__ == "__main__":
    main()
