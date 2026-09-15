"""Leakage-safe chronological train/validation/test splitting."""

from __future__ import annotations

from typing import Any

import pandas as pd


def _validate_ratios(train_ratio: float, validation_ratio: float, test_ratio: float) -> None:
    ratios = (train_ratio, validation_ratio, test_ratio)
    if any(isinstance(value, bool) or not isinstance(value, (int, float)) for value in ratios):
        raise TypeError("split ratios must be numbers")
    if any(value < 0 or value > 1 for value in ratios):
        raise ValueError("split ratios must be between 0 and 1")
    if abs(sum(ratios) - 1.0) > 1e-9:
        raise ValueError("train_ratio + validation_ratio + test_ratio must equal 1")


def _split_pandas(frame: pd.DataFrame, time_column: str, train_ratio: float, validation_ratio: float):
    if time_column not in frame.columns:
        raise ValueError(f"Missing time column: {time_column}")
    ordered = frame.copy()
    ordered["__split_time"] = pd.to_datetime(ordered[time_column], errors="coerce")
    if ordered["__split_time"].isna().any():
        raise ValueError(f"Column {time_column!r} contains invalid timestamps")
    ordered = ordered.sort_values("__split_time", kind="mergesort")
    # Split on complete time buckets so records sharing an hour can never be
    # distributed across train and validation/test partitions.
    unique_times = ordered["__split_time"].drop_duplicates().sort_values().reset_index(drop=True)
    train_end = int(len(unique_times) * train_ratio)
    validation_end = train_end + int(len(unique_times) * validation_ratio)
    train_cut = unique_times.iloc[train_end] if train_end < len(unique_times) else pd.Timestamp.max
    validation_cut = unique_times.iloc[validation_end] if validation_end < len(unique_times) else pd.Timestamp.max
    return (
        ordered[ordered["__split_time"] < train_cut].drop(columns="__split_time").reset_index(drop=True),
        ordered[(ordered["__split_time"] >= train_cut) & (ordered["__split_time"] < validation_cut)].drop(columns="__split_time").reset_index(drop=True),
        ordered[ordered["__split_time"] >= validation_cut].drop(columns="__split_time").reset_index(drop=True),
    )


def time_based_split(
    df: Any,
    train_ratio: float = 0.7,
    validation_ratio: float = 0.15,
    test_ratio: float = 0.15,
    time_column: str = "datetime",
    **kwargs,
):
    """Split pandas or Spark data into chronological train/val/test ranges.

    ``val_ratio`` and ``timestamp_column`` are accepted as aliases. The
    returned tuple is ``(train, validation, test)`` and contains every input
    row exactly once, with no random shuffling.
    """
    if "val_ratio" in kwargs:
        validation_ratio = kwargs.pop("val_ratio")
    if "valid_ratio" in kwargs:
        validation_ratio = kwargs.pop("valid_ratio")
    if "timestamp_column" in kwargs:
        time_column = kwargs.pop("timestamp_column")
    if "time_col" in kwargs:
        time_column = kwargs.pop("time_col")
    if kwargs:
        raise TypeError("Unexpected keyword argument(s): " + ", ".join(sorted(kwargs)))
    _validate_ratios(train_ratio, validation_ratio, test_ratio)

    if isinstance(df, pd.DataFrame):
        return _split_pandas(df, time_column, train_ratio, validation_ratio)

    if df.__class__.__module__.startswith("pyspark.sql"):
        from pyspark.sql import Window, functions as F

        if time_column not in df.columns:
            raise ValueError(f"Missing time column: {time_column}")
        ordered = df.withColumn("__split_time", F.to_timestamp(F.col(time_column)))
        if ordered.where(F.col("__split_time").isNull()).limit(1).count():
            raise ValueError(f"Column {time_column!r} contains invalid timestamps")
        unique_times = ordered.select("__split_time").distinct().withColumn(
            "__split_index",
            F.row_number().over(Window.orderBy(F.col("__split_time"))) - 1,
        )
        time_count = unique_times.count()
        train_end = int(time_count * train_ratio)
        validation_end = train_end + int(time_count * validation_ratio)
        cuts = unique_times.where(F.col("__split_index").isin(train_end, validation_end)).collect()
        cut_map = {int(row["__split_index"]): row["__split_time"] for row in cuts}
        train_cut = cut_map.get(train_end)
        validation_cut = cut_map.get(validation_end)
        train = ordered.where(F.lit(True) if train_cut is None else F.col("__split_time") < F.lit(train_cut))
        validation = ordered.where(
            (F.lit(True) if train_cut is None else F.col("__split_time") >= F.lit(train_cut))
            & (F.lit(False) if validation_cut is None else F.col("__split_time") < F.lit(validation_cut))
        )
        test = ordered.where(F.lit(False) if validation_cut is None else F.col("__split_time") >= F.lit(validation_cut))
        # Do not order each returned DataFrame again.  ``orderBy`` here would
        # trigger a separate full shuffle for every split when the caller
        # fits a model.  The split boundaries are already chronological; Spark
        # does not require physical row ordering for ML training.
        return tuple(part.drop("__split_time") for part in (train, validation, test))

    raise TypeError("df must be a pandas or PySpark DataFrame")
