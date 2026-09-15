"""Load a saved model and generate 1h, 6h, or 24h forecasts."""

from __future__ import annotations

import argparse
import pickle
import sys
from pathlib import Path
from typing import Any, Iterable, Optional

import pandas as pd

PROJECT_ROOT = Path(__file__).resolve().parents[1]
if str(PROJECT_ROOT) not in sys.path:
    sys.path.insert(0, str(PROJECT_ROOT))

from ml.forecast import forecast_1h, forecast_6h, forecast_24h


def _load_model(model: Any, spark: Any = None):
    if not isinstance(model, (str, Path)):
        return model
    model_path = str(model)
    if spark is not None:
        from pyspark.ml import PipelineModel
        return PipelineModel.load(model_path)
    path = Path(model_path)
    if path.is_dir():
        raise ValueError("A Spark session is required to load a Spark PipelineModel directory")
    with path.open("rb") as handle:
        return pickle.load(handle)


def run_inference(
    model: Any,
    history: Any,
    horizon: int = 24,
    station_id: Optional[str] = None,
    output: Optional[str] = None,
    feature_columns: Optional[list[str]] = None,
    spark: Any = None,
):
    """Generate future load predictions from a saved or in-memory model."""
    if horizon not in (1, 6, 24):
        raise ValueError("horizon must be 1, 6, or 24")
    model = _load_model(model, spark=spark)
    function = {1: forecast_1h, 6: forecast_6h, 24: forecast_24h}[horizon]
    result = function(model, history, station_id=station_id, feature_columns=feature_columns, spark=spark)
    if output:
        output_path = Path(output).expanduser()
        output_path.parent.mkdir(parents=True, exist_ok=True)
        result.to_csv(output_path, index=False)
    return result


def main(argv: Optional[Iterable[str]] = None) -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, help="Spark CSV directory or CSV file")
    parser.add_argument("--model", required=True, help="Saved Spark PipelineModel directory or pickle file")
    parser.add_argument("--horizon", type=int, choices=(1, 6, 24), default=24)
    parser.add_argument("--station-id", default=None)
    parser.add_argument("--output", required=True)
    args = parser.parse_args(argv)

    from pyspark.sql import SparkSession

    spark = (
        SparkSession.builder.appName("ChargingPlatform-Inference")
        .master("local[2]")
        .config("spark.ui.enabled", "false")
        .config("spark.driver.bindAddress", "127.0.0.1")
        .config("spark.driver.host", "127.0.0.1")
        .config("spark.sql.execution.arrow.pyspark.enabled", "false")
        .getOrCreate()
    )
    try:
        from pyspark.ml import PipelineModel
        history = spark.read.option("header", True).option("inferSchema", True).csv(args.input)
        model = PipelineModel.load(args.model)
        result = run_inference(model, history, horizon=args.horizon, station_id=args.station_id, output=args.output, spark=spark)
        print(f"Inference written to {args.output} ({len(result)} rows)")
    finally:
        spark.stop()


if __name__ == "__main__":
    main()
