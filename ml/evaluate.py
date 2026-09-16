"""Evaluation metrics for charging-load forecasts."""

from __future__ import annotations

from collections.abc import Mapping
from typing import Any, Iterable, Optional

import numpy as np
import pandas as pd


def _values(actual: Any, predicted: Any) -> tuple[np.ndarray, np.ndarray]:
    """Convert inputs to aligned finite numeric arrays."""
    if isinstance(actual, pd.DataFrame):
        actual = actual["actual" if "actual" in actual.columns else "energy_kwh"]
    if isinstance(predicted, pd.DataFrame):
        predicted = predicted["prediction" if "prediction" in predicted.columns else "predicted_load_kwh"]
    y = np.asarray(actual, dtype=float).reshape(-1)
    p = np.asarray(predicted, dtype=float).reshape(-1)
    if y.size != p.size:
        raise ValueError(f"actual and predicted lengths differ: {y.size} != {p.size}")
    valid = np.isfinite(y) & np.isfinite(p)
    if not valid.all():
        y, p = y[valid], p[valid]
    return y, p


def mae(actual: Any, predicted: Any) -> float:
    """Mean absolute error between actual and predicted load."""
    y, p = _values(actual, predicted)
    return float(np.mean(np.abs(p - y))) if y.size else 0.0


def rmse(actual: Any, predicted: Any) -> float:
    """Root mean squared error, emphasizing large errors."""
    y, p = _values(actual, predicted)
    return float(np.sqrt(np.mean((p - y) ** 2))) if y.size else 0.0


def mape(actual: Any, predicted: Any, zero_policy: str = "ignore") -> float:
    """Mean absolute percentage error in percent.

    Charging datasets contain legitimate zero-load hours.  By default those
    rows are excluded from the denominator; ``zero_policy='include'`` uses a
    small epsilon denominator instead.
    """
    y, p = _values(actual, predicted)
    if zero_policy not in {"ignore", "include"}:
        raise ValueError("zero_policy must be 'ignore' or 'include'")
    if not y.size:
        return 0.0
    if zero_policy == "ignore":
        mask = np.abs(y) > 1e-12
        if not mask.any():
            return 0.0
        return float(np.mean(np.abs((p[mask] - y[mask]) / y[mask])) * 100)
    denominator = np.maximum(np.abs(y), 1e-12)
    return float(np.mean(np.abs((p - y) / denominator)) * 100)


def r_squared(actual: Any, predicted: Any) -> float:
    """Coefficient of determination (R²)."""
    y, p = _values(actual, predicted)
    if not y.size:
        return 0.0
    total = float(np.sum((y - np.mean(y)) ** 2))
    residual = float(np.sum((p - y) ** 2))
    if total <= 1e-12:
        return 1.0 if residual <= 1e-12 else 0.0
    return float(1.0 - residual / total)


def _metric_record(name: str, value: Any, actual: Any = None, predicted: Any = None) -> dict[str, Any]:
    if isinstance(value, Mapping):
        metrics = value.get("test_metrics") or value.get("metrics")
        if isinstance(metrics, Mapping):
            return {"model": name, "mae": float(metrics.get("mae", 0.0)), "rmse": float(metrics.get("rmse", 0.0)), "mape": float(metrics.get("mape", 0.0)), "r2": float(metrics.get("r2", metrics.get("r_squared", 0.0)))}
        actual = value.get("actual", value.get("y_true", actual))
        predicted = value.get("prediction", value.get("predicted", value.get("y_pred", predicted)))
    elif isinstance(value, (tuple, list)) and len(value) == 2 and actual is None:
        actual, predicted = value
    if actual is None or predicted is None:
        raise ValueError(f"No actual/predicted values or metrics supplied for model {name!r}")
    return {"model": name, "mae": mae(actual, predicted), "rmse": rmse(actual, predicted), "mape": mape(actual, predicted), "r2": r_squared(actual, predicted)}


def compare_models(models: Mapping[str, Any], actual: Any = None, predicted: Any = None, sort_by: str = "rmse") -> pd.DataFrame:
    """Compare model metrics and return rows sorted by the selected metric.

    ``models`` may map names to ``(actual, predicted)`` pairs, prediction
    dictionaries, or the result dictionaries returned by the training helpers.
    When all models share one target array, pass it as ``actual`` and provide a
    mapping of names to prediction arrays as ``predicted``.
    """
    if not isinstance(models, Mapping) or not models:
        raise ValueError("models must be a non-empty mapping")
    # ``train_rf_xgboost`` returns a wrapper containing a ``models`` mapping;
    # accept that object directly in addition to the inner mapping.
    if isinstance(models.get("models"), Mapping):
        models = models["models"]
    if sort_by not in {"mae", "rmse", "mape", "r2"}:
        raise ValueError("sort_by must be one of mae, rmse, mape, or r2")
    rows = []
    if actual is not None and isinstance(predicted, Mapping):
        rows = [_metric_record(name, prediction, actual=actual, predicted=prediction) for name, prediction in predicted.items()]
    else:
        rows = [
            _metric_record(name, value, actual=actual, predicted=(value if actual is not None and predicted is None else predicted))
            for name, value in models.items()
        ]
    result = pd.DataFrame(rows)
    result = result.sort_values(sort_by, ascending=(sort_by != "r2"), kind="mergesort").reset_index(drop=True)
    result["rank"] = result.index + 1
    result["is_best"] = result["rank"] == 1
    return result[["rank", "model", "mae", "rmse", "mape", "r2", "is_best"]]


def select_best_model(models: Mapping[str, Any], criterion: str = "rmse") -> tuple[str, Any]:
    """Select the final model using test/validation metrics.

    Error metrics are minimized; R² is maximized.  The returned tuple is
    ``(model_name, original_result)`` so callers can immediately persist the
    selected estimator.
    """
    criterion = {"test_rmse": "rmse", "test_mae": "mae", "test_mape": "mape", "r_squared": "r2"}.get(criterion, criterion)
    table = compare_models(models, sort_by=criterion)
    name = str(table.iloc[0]["model"])
    source = models.get("models", models) if isinstance(models, Mapping) else models
    return name, source[name]
