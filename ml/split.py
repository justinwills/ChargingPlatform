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
    ordered = ordered.sort_values("__split_time", kind="mergesort").drop(columns="__split_time")
    total = len(ordered)
    train_end = int(total * train_ratio)
    validation_end = train_end + int(total * validation_ratio)
    return (
        ordered.iloc[:train_end].reset_index(drop=True),
        ordered.iloc[train_end:validation_end].reset_index(drop=True),
        ordered.iloc[validation_end:].reset_index(drop=True),
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
        total = ordered.count()
        train_end = int(total * train_ratio)
        validation_end = train_end + int(total * validation_ratio)
        numbered = ordered.withColumn(
            "__split_index",
            F.row_number().over(Window.orderBy(F.col("__split_time"))) - 1,
        )
        train = numbered.where(F.col("__split_index") < train_end)
        validation = numbered.where(
            (F.col("__split_index") >= train_end)
            & (F.col("__split_index") < validation_end)
        )
        test = numbered.where(F.col("__split_index") >= validation_end)
        return tuple(
            part.orderBy(F.col("__split_time"))
            .drop("__split_time", "__split_index")
            for part in (train, validation, test)
        )

    raise TypeError("df must be a pandas or PySpark DataFrame")
