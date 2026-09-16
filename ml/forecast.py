"""Short-horizon charging-load forecasts.

The public helpers perform recursive hourly forecasting.  Each predicted hour
is fed back into the next hour's lag features, so the 6h and 24h functions do
not accidentally use future observations.  pandas is used for the small
forecast frame; Spark DataFrames are reduced to that frame before prediction.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any, Iterable, Mapping, Optional, Sequence

import pandas as pd


def _is_frame(value: Any) -> bool:
    return isinstance(value, pd.DataFrame) or value.__class__.__module__.startswith("pyspark.sql")


def _normalise_call(model: Any, data: Any, kwargs: dict[str, Any]):
    if "history" in kwargs and data is None:
        data = kwargs.pop("history")
    if "dataset" in kwargs and data is None:
        data = kwargs.pop("dataset")
    if "model" in kwargs and model is None:
        model = kwargs.pop("model")
    # Permit forecast_1h(history) and forecast_1h(history, model).
    if _is_frame(model):
        if data is not None and not _is_frame(data):
            model, data = data, model
        elif data is None:
            data, model = model, None
    if data is None:
        raise ValueError("A history DataFrame or CSV path is required")
    return model, data


def _to_pandas(data: Any) -> pd.DataFrame:
    if isinstance(data, (str, Path)):
        data = pd.read_csv(data)
    elif not isinstance(data, pd.DataFrame) and data.__class__.__module__.startswith("pyspark.sql"):
        # Forecasting needs only the latest row plus 24 historical values per
        # station.  Do not collect the entire training dataset to the driver.
        from pyspark.sql import Window, functions as F

        if "station_id" in data.columns:
            window = Window.partitionBy("station_id").orderBy(F.col("datetime").desc())
        else:
            window = Window.orderBy(F.col("datetime").desc())
        # Spark 3.4's toPandas conversion can fail with pandas 2.x on a
        # timezone-aware TimestampType ("unit-less dtype datetime64").  Cast
        # only the transfer column to text; _to_pandas parses it immediately
        # afterwards and preserves the original instant.
        # Keep a compact station/hour demand profile alongside the latest
        # rows.  The profile lets the demo produce a useful scenario forecast
        # when the final recorded hour is idle (which otherwise causes a
        # recursive model to stay at zero for every future hour).
        profile = None
        if "station_id" in data.columns and "hour" in data.columns:
            profile = (
                data.groupBy("station_id", "hour")
                .agg(F.avg(F.col("energy_kwh").cast("double")).alias("__profile_mean"))
                .toPandas()
            )
        station_profile = None
        if "station_id" in data.columns:
            station_profile = (
                data.groupBy("station_id")
                .agg(F.avg(F.col("energy_kwh").cast("double")).alias("__station_mean"))
                .toPandas()
            )
        data = (
            data.withColumn("__forecast_rank", F.row_number().over(window))
            .where(F.col("__forecast_rank") <= 24)
            .drop("__forecast_rank")
            .withColumn("datetime", F.col("datetime").cast("string"))
            .toPandas()
        )
        if profile is not None:
            data.attrs["historical_profile"] = profile.to_dict("records")
        if station_profile is not None:
            data.attrs["historical_station_mean"] = station_profile.to_dict("records")
    if not isinstance(data, pd.DataFrame):
        raise TypeError("history must be a pandas/Spark DataFrame or CSV path")
    required = {"datetime", "energy_kwh"}
    missing = sorted(required - set(data.columns))
    if missing:
        raise ValueError("History is missing columns: " + ", ".join(missing))
    frame = data.copy()
    frame["datetime"] = pd.to_datetime(frame["datetime"], errors="coerce")
    frame["energy_kwh"] = pd.to_numeric(frame["energy_kwh"], errors="coerce")
    frame = frame.dropna(subset=["datetime", "energy_kwh"])
    if "station_id" not in frame.columns:
        frame["station_id"] = "global"
    return frame.sort_values(["station_id", "datetime"], kind="mergesort").reset_index(drop=True)


def _unwrap_model(model: Any):
    if isinstance(model, Mapping):
        model = model.get("best_model") or model.get("model")
    return model


def _feature_columns(model: Any, frame: pd.DataFrame, explicit: Optional[Sequence[str]]) -> list[str]:
    source_model = _unwrap_model(model)
    if explicit is not None:
        columns = list(explicit)
    elif isinstance(model, Mapping) and model.get("feature_columns"):
        columns = list(model["feature_columns"])
    elif hasattr(model, "feature_columns"):
        columns = list(model.feature_columns)
    elif hasattr(source_model, "stages"):
        # A saved Spark PipelineModel contains the VectorAssembler used during
        # training.  Read its exact input column list instead of guessing from
        # column names; this is essential for fields such as device counts.
        columns = []
        for stage in source_model.stages:
            if hasattr(stage, "getInputCols"):
                columns = list(stage.getInputCols())
                break
        if not columns:
            columns = []
    else:
        excluded = {"station_id", "location_id", "datetime", "energy_kwh", "forecast_horizon", "predicted_load"}
        columns = [c for c in frame.columns if c not in excluded and (c.startswith("load_lag_") or c in {
            "hour", "day", "month", "weekday_number", "is_weekend", "is_holiday", "is_workday",
            "weather_type_code", "temperature_c", "humidity_pct", "precipitation_mm", "wind_speed_mps", "visibility_km", "is_rain",
            "device_count", "facility_type_code", "electricity_price_cny_kwh", "service_fee_cny_kwh", "station_status_code",
        })]
    missing = [c for c in columns if c not in frame.columns]
    if missing:
        raise ValueError("History is missing model feature columns: " + ", ".join(missing))
    return columns


def _update_calendar(row: dict[str, Any], timestamp: pd.Timestamp) -> None:
    row["datetime"] = timestamp
    row["hour"] = int(timestamp.hour)
    row["day"] = int(timestamp.day)
    row["month"] = int(timestamp.month)
    row["weekday_number"] = int(timestamp.weekday())
    row["is_weekend"] = int(timestamp.weekday() >= 5)
    row["is_workday"] = int(timestamp.weekday() < 5)


# Demand floor epsilon expressed in kWh.  Any per-hour station forecast below
# this level is operationally "no demand".  When a trained model collapses to
# near-zero for a station that recently charged -- which is exactly what a
# weak model does on a zero-inflated station-hour target, where roughly 97% of
# rows carry no demand -- the forecast is held at the station's most recent
# observed load (the same persistence baseline used when no model is
# available) instead of silently erasing real demand.
FORECAST_FLOOR_KWH = 0.05


def _floor_prediction(prediction: float, fallback: float) -> float:
    """Guard against model output collapse below the recent demand level."""
    fallback = max(0.0, float(fallback))
    if prediction < FORECAST_FLOOR_KWH and fallback > 0:
        return fallback
    return max(0.0, prediction)


def _predict(model: Any, row: dict[str, Any], features: list[str], fallback: float, spark: Any = None) -> float:
    model = _unwrap_model(model)
    if model is None:
        return max(0.0, float(fallback))
    values = pd.DataFrame([{column: pd.to_numeric(pd.Series([row.get(column)]), errors="coerce").iloc[0] for column in features}]).fillna(0.0)
    if hasattr(model, "predict"):
        prediction = model.predict(values)
        return _floor_prediction(float(prediction[0]), fallback)
    if hasattr(model, "transform") and model.__class__.__module__.startswith("pyspark"):
        # Spark ML/XGBoost models expose ``transform`` rather than sklearn's
        # ``predict``.  Only one row per station/hour is sent to Spark, so the
        # driver-side conversion remains small even for a large history table.
        from pyspark.sql import SparkSession, functions as F

        spark = spark or SparkSession.getActiveSession() or SparkSession.builder.getOrCreate()
        spark_row = spark.createDataFrame(values)
        for column in features:
            spark_row = spark_row.withColumn(column, F.col(column).cast("double"))
        # Keep the transformed DataFrame intact.  Embedding a Column obtained
        # from ``model.transform(spark_row)`` back into ``spark_row`` creates
        # mismatched Catalyst expression IDs and an AnalysisException.
        transformed = model.transform(spark_row)
        prediction = transformed.select("prediction").first()[0]
        return _floor_prediction(float(prediction), fallback)
    raise TypeError("model must provide a predict() method; Spark Pipeline models require pandas-compatible inference")


def _predict_batch(model: Any, rows: list[dict[str, Any]], features: list[str], fallbacks: list[float], spark: Any = None) -> list[float]:
    """Predict one future hour for all stations in one model invocation."""
    model = _unwrap_model(model)
    if model is None:
        return [max(0.0, float(value)) for value in fallbacks]
    values = pd.DataFrame([
        {column: pd.to_numeric(pd.Series([row.get(column)]), errors="coerce").iloc[0] for column in features}
        for row in rows
    ]).fillna(0.0)
    if hasattr(model, "predict"):
        return [_floor_prediction(float(value), fallback) for value, fallback in zip(model.predict(values), fallbacks)]
    if hasattr(model, "transform") and model.__class__.__module__.startswith("pyspark"):
        from pyspark.sql import SparkSession, functions as F

        spark = spark or SparkSession.getActiveSession() or SparkSession.builder.getOrCreate()
        spark_rows = spark.createDataFrame(values)
        for column in features:
            spark_rows = spark_rows.withColumn(column, F.col(column).cast("double"))
        predictions = model.transform(spark_rows).select("prediction").collect()
        return [_floor_prediction(float(row[0]), fallback) for row, fallback in zip(predictions, fallbacks)]
    raise TypeError("model must provide predict() or Spark transform()")


def forecast_load(
    model: Any,
    data: Any,
    horizon: int,
    station_id: Optional[str] = None,
    feature_columns: Optional[Sequence[str]] = None,
    as_records: bool = False,
    spark: Any = None,
):
    """Recursively forecast ``horizon`` future hours for each station."""
    if horizon <= 0:
        raise ValueError("horizon must be a positive integer")
    spark = spark or getattr(data, "sparkSession", None)
    frame = _to_pandas(data)
    if station_id is not None:
        frame = frame[frame["station_id"].astype(str) == str(station_id)]
    if frame.empty:
        raise ValueError("No history rows available for the requested station")
    features = _feature_columns(model, frame, feature_columns)
    if not features and _unwrap_model(model) is not None:
        raise ValueError("No model feature columns were found in history")
    # Build a compact historical fallback from the complete pandas history.
    # Spark inputs carry the same aggregates in DataFrame attrs (computed in
    # _to_pandas before it trims to the latest 24 rows per station).
    profile = {}
    station_means = {}
    for item in frame.attrs.get("historical_profile", []):
        profile[(str(item.get("station_id")), int(item.get("hour", 0)))] = float(item.get("__profile_mean") or 0.0)
    for item in frame.attrs.get("historical_station_mean", []):
        station_means[str(item.get("station_id"))] = float(item.get("__station_mean") or 0.0)
    if not profile and "hour" in frame.columns:
        profile_frame = frame[["station_id", "hour", "energy_kwh"]].copy()
        profile_frame["hour"] = pd.to_numeric(profile_frame["hour"], errors="coerce")
        profile_frame = profile_frame.dropna(subset=["hour"])
        profile_frame["hour"] = profile_frame["hour"].astype(int)
        profile_frame["energy_kwh"] = pd.to_numeric(profile_frame["energy_kwh"], errors="coerce").fillna(0.0)
        grouped = profile_frame.groupby(["station_id", "hour"], dropna=False)["energy_kwh"].mean()
        profile = {(str(station), int(hour)): float(value) for (station, hour), value in grouped.items()}
        means = profile_frame.groupby("station_id", dropna=False)["energy_kwh"].mean()
        station_means = {str(station): float(value) for station, value in means.items()}
    outputs = []
    states = []
    for key, group in frame.groupby("station_id", sort=True, dropna=False):
        group = group.sort_values("datetime", kind="mergesort")
        states.append({
            "key": key,
            "latest": group.iloc[-1].to_dict(),
            "observed": group["energy_kwh"].astype(float).tolist(),
            # If a station ends on an idle row, use its historical profile as
            # the fallback for each future hour instead of carrying zero
            # forward indefinitely.  Stations with a positive latest load
            # retain the existing persistence behavior.
            "profile_mode": float(group["energy_kwh"].iloc[-1]) <= 0.0,
        })
    for step in range(1, horizon + 1):
        candidates = []
        for state in states:
            latest = state["latest"]
            observed = state["observed"]
            timestamp = pd.Timestamp(latest["datetime"]) + pd.Timedelta(hours=1)
            row = dict(latest)
            _update_calendar(row, timestamp)
            for lag in (1, 2, 3, 24):
                value = observed[-lag] if len(observed) >= lag else None
                row[f"load_lag_{lag}h"] = value
                row[f"lag_{lag}h"] = value
                row[f"lag_{lag}"] = value
            candidates.append((state, row, timestamp))
        fallbacks = []
        for state, row, timestamp in candidates:
            if state["profile_mode"]:
                key = (str(state["key"]), int(timestamp.hour))
                fallback = profile.get(key, station_means.get(str(state["key"]), 0.0))
            else:
                fallback = state["observed"][-1]
            fallbacks.append(fallback)
        predictions = _predict_batch(model, [item[1] for item in candidates], features, fallbacks, spark=spark)
        for (state, row, timestamp), prediction in zip(candidates, predictions):
            outputs.append({"station_id": state["key"], "datetime": timestamp, "horizon_hour": step, "predicted_load_kwh": prediction})
            observed = state["observed"]
            observed.append(prediction)
            row["energy_kwh"] = prediction
            state["latest"] = row
    result = pd.DataFrame(outputs)
    return result.to_dict("records") if as_records else result


def forecast_1h(model: Any = None, data: Any = None, **kwargs):
    """Predict charging load for the next hour."""
    model, data = _normalise_call(model, data, kwargs)
    return forecast_load(model, data, 1, **kwargs)


def forecast_6h(model: Any = None, data: Any = None, **kwargs):
    """Recursively predict the next six hourly loads."""
    model, data = _normalise_call(model, data, kwargs)
    return forecast_load(model, data, 6, **kwargs)


def forecast_24h(model: Any = None, data: Any = None, **kwargs):
    """Recursively predict the next 24 hourly loads."""
    model, data = _normalise_call(model, data, kwargs)
    return forecast_load(model, data, 24, **kwargs)


def forecast_per_station(model: Any = None, data: Any = None, station_id: Optional[str] = None, **kwargs):
    """Return a 24-hour forecast for one station (or all stations)."""
    model, data = _normalise_call(model, data, kwargs)
    if station_id is None:
        raise ValueError("station_id is required for forecast_per_station")
    return forecast_load(model, data, 24, station_id=station_id, **kwargs)


def main(argv: Optional[Iterable[str]] = None) -> None:
    """Run a saved Spark PipelineModel against the latest history rows."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, help="Spark CSV directory or file")
    parser.add_argument("--model", required=True, help="Saved Spark PipelineModel directory")
    parser.add_argument("--horizon", type=int, choices=(1, 6, 24), default=24)
    parser.add_argument("--station-id", default=None)
    parser.add_argument("--output", default=None, help="Optional local CSV output")
    args = parser.parse_args(argv)
    from pyspark.sql import SparkSession
    spark = SparkSession.builder.appName("ChargingPlatform-Forecast").master("local[2]").config("spark.ui.enabled", "false").config("spark.driver.bindAddress", "127.0.0.1").config("spark.driver.host", "127.0.0.1").config("spark.sql.execution.arrow.pyspark.enabled", "false").getOrCreate()
    try:
        from pyspark.ml import PipelineModel
        history = spark.read.option("header", True).option("inferSchema", True).csv(args.input)
        model = PipelineModel.load(args.model)
        result = forecast_load(model, history, args.horizon, station_id=args.station_id, spark=spark)
        if args.output:
            result.to_csv(args.output, index=False)
            print(f"Forecast written to {args.output}")
        else:
            print(result.to_json(orient="records", date_format="iso"))
    finally:
        spark.stop()


if __name__ == "__main__":
    main()
