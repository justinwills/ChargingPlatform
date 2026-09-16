"""End-to-end integration contract for the Phase 2 recommendation pipeline."""

import unittest

import pandas as pd

from ml.recommend.ops_advice import generate_ops_advice
from ml.recommend.peak_alert import detect_peak_station
from ml.recommend.scoring import calculate_station_score, recommend_low_load_station


class Phase2IntegrationTests(unittest.TestCase):
    """Task #126: functional integration for the recommendation stack."""

    def setUp(self):
        self.predictions = pd.DataFrame(
            [
                {"station_id": "S1", "datetime": "2026-09-16 10:00:00", "predicted_load": 18.0},
                {"station_id": "S2", "datetime": "2026-09-16 10:00:00", "predicted_load": 95.0},
                {"station_id": "S3", "datetime": "2026-09-16 10:00:00", "predicted_load": 42.0},
            ]
        )
        self.stations = pd.DataFrame(
            [
                {
                    "station_id": "S1",
                    "device_count": 8,
                    "available_chargers": 6,
                    "station_status": "normal",
                    "electricity_price_cny_kwh": 0.82,
                    "service_fee_cny_kwh": 0.20,
                },
                {
                    "station_id": "S2",
                    "device_count": 10,
                    "available_chargers": 1,
                    "station_status": "normal",
                    "electricity_price_cny_kwh": 1.20,
                    "service_fee_cny_kwh": 0.40,
                },
                {
                    "station_id": "S3",
                    "device_count": 4,
                    "available_chargers": 4,
                    "station_status": "online",
                    "electricity_price_cny_kwh": 0.80,
                    "service_fee_cny_kwh": 0.18,
                },
            ]
        )
        self.history = pd.DataFrame(
            [
                {"station_id": "S1", "energy_kwh": 18.0},
                {"station_id": "S1", "energy_kwh": 22.0},
                {"station_id": "S2", "energy_kwh": 100.0},
                {"station_id": "S3", "energy_kwh": 45.0},
            ]
        )

    def test_full_pipeline_recommends_low_load_station_and_alerts_peak(self):
        scored = calculate_station_score(self.predictions, self.stations, self.history)
        self.assertEqual("S1", scored.iloc[0]["station_id"])
        self.assertIn("score", scored.columns)

        alerts = detect_peak_station(self.predictions, threshold=80.0)
        self.assertEqual(1, len(alerts))
        self.assertEqual("S2", alerts.iloc[0]["station_id"])
        self.assertEqual("high", alerts.iloc[0]["alert_level"])

        recommendations = recommend_low_load_station(
            self.predictions,
            self.stations,
            self.history,
            top_n=2,
        )
        self.assertEqual("S1", recommendations[0]["station_id"])
        self.assertNotIn("S2", [item["station_id"] for item in recommendations])

    def test_ops_advice_returns_actionable_station_guidance(self):
        advice = generate_ops_advice(
            station_id="S2",
            predictions=self.predictions,
            stations=self.stations,
            history=self.history,
            threshold=80.0,
        )

        self.assertEqual("S2", advice["station_id"])
        self.assertEqual("high", advice["priority"])
        self.assertTrue(advice["advice"])
        self.assertTrue(any("峰值" in item or "peak" in item.lower() for item in advice["advice"]))


if __name__ == "__main__":
    unittest.main()