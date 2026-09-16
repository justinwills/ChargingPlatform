"""Tests for the ML prediction API service (Task #122).

Runs without PySpark: the service falls back to pandas for both history and
inference, so the contract can be verified on a developer laptop.  When pandas
itself is unavailable the whole class is skipped.
"""

from __future__ import annotations

import importlib.util
import pickle
import tempfile
import unittest
from pathlib import Path

PANDAS_AVAILABLE = importlib.util.find_spec("pandas") is not None
if PANDAS_AVAILABLE:
    import pandas as pd


class FakeModel:
    """Minimal sklearn-style regressor honoring the feature contract."""

    feature_columns = ["hour"]

    def predict(self, values):
        return values["hour"].to_numpy().astype(float) * 0.0 + 1.0


def _history_frame() -> pd.DataFrame:
    rows = []
    for station in (129465, 131897):
        for hour in range(24, 48):
            rows.append(
                {
                    "station_id": station,
                    "datetime": f"2025-10-05 {hour % 24:02d}:00:00",
                    "energy_kwh": 0.5,
                    "hour": hour % 24,
                }
            )
    return pd.DataFrame(rows)


@unittest.skipUnless(PANDAS_AVAILABLE, "pandas is required for prediction API service tests")
class PredictionServiceTests(unittest.TestCase):
    def _service(self, tmp: Path, model=True):
        history = tmp / "history.csv"
        _history_frame().to_csv(history, index=False)
        kwargs = {"history_path": str(history)}
        if model:
            path = tmp / "model.pkl"
            with path.open("wb") as handle:
                pickle.dump(FakeModel(), handle)
            kwargs["model_path"] = str(path)
        else:
            kwargs["model_path"] = str(tmp / "missing_model.pkl")
        from ml.prediction_service import PredictionService

        return PredictionService(**kwargs)

    def test_forecast_1h_returns_per_station_records(self):
        with tempfile.TemporaryDirectory() as directory:
            service = self._service(Path(directory))
            records = service.forecast(1)
        self.assertEqual(2, len(records))
        self.assertEqual(1, records[0]["horizon_hour"])
        self.assertEqual(1.0, records[0]["predicted_load_kwh"])
        self.assertIn("datetime", records[0])
        self.assertEqual(129465, records[0]["station_id"])

    def test_forecast_24h_returns_two_stations_times_horizon(self):
        with tempfile.TemporaryDirectory() as directory:
            service = self._service(Path(directory))
            records = service.forecast(24)
        self.assertEqual(48, len(records))
        self.assertEqual({129465, 131897}, {row["station_id"] for row in records})
        self.assertEqual(set(range(1, 25)), set(row["horizon_hour"] for row in records))

    def test_forecast_filters_single_station(self):
        with tempfile.TemporaryDirectory() as directory:
            service = self._service(Path(directory))
            records = service.forecast(6, station_id="131897")
        self.assertEqual(6, len(records))
        self.assertTrue(all(row["station_id"] == 131897 for row in records))

    def test_unknown_station_raises_value_error(self):
        with tempfile.TemporaryDirectory() as directory:
            service = self._service(Path(directory))
            with self.assertRaises(ValueError):
                service.forecast(1, station_id="does-not-exist")

    def test_invalid_horizon_is_rejected_before_loading(self):
        from ml.prediction_service import PredictionService

        with self.assertRaises(ValueError):
            PredictionService().forecast(12)

    def test_missing_model_falls_back_to_persistence(self):
        with tempfile.TemporaryDirectory() as directory:
            service = self._service(Path(directory), model=False)
            records = service.forecast(1)
        self.assertTrue(service.is_fallback)
        self.assertIsNotNone(service.load_error)
        self.assertIn("missing_model.pkl", service.model_name)
        self.assertEqual([0.5, 0.5], [row["predicted_load_kwh"] for row in records])

    def test_build_payload_success_and_not_found(self):
        from ml.prediction_service import build_forecast_payload

        with tempfile.TemporaryDirectory() as directory:
            service = self._service(Path(directory))
            payload = build_forecast_payload(service, 1)
            error_payload, status = build_forecast_payload(service, 1, station_id="who")
        self.assertEqual(0, payload["code"])
        self.assertEqual("ok", payload["msg"])
        self.assertEqual(1, payload["horizonHours"])
        self.assertEqual("model.pkl", payload["model"])
        self.assertEqual(2, len(payload["data"]))
        self.assertFalse(payload["isFallback"])
        self.assertEqual(404, status)
        self.assertEqual(1, error_payload["code"])


if __name__ == "__main__":
    unittest.main()