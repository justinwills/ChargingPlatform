"""Focused tests for recommendation scoring and peak alerts."""

import unittest

import pandas as pd

from ml.recommend.peak_alert import detect_peak_station
from ml.recommend.scoring import calculate_station_score, recommend_low_load_station


class RecommendationContractTests(unittest.TestCase):
    def setUp(self):
        self.predictions = pd.DataFrame(
            [
                {"station_id": "S1", "datetime": "2026-09-16 10:00:00", "predicted_load": 20.0},
                {"station_id": "S2", "datetime": "2026-09-16 10:00:00", "predicted_load": 95.0},
                {"station_id": "S3", "datetime": "2026-09-16 10:00:00", "predicted_load": 10.0},
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
                    "station_status": "offline",
                    "electricity_price_cny_kwh": 0.80,
                    "service_fee_cny_kwh": 0.15,
                },
            ]
        )
        self.history = pd.DataFrame(
            [
                {"station_id": "S1", "energy_kwh": 18.0},
                {"station_id": "S1", "energy_kwh": 22.0},
                {"station_id": "S2", "energy_kwh": 100.0},
            ]
        )

    def test_calculate_station_score_ranks_low_pressure_normal_station_first(self):
        result = calculate_station_score(self.predictions, self.stations, self.history)

        self.assertEqual("S1", result.iloc[0]["station_id"])
        self.assertIn("score", result.columns)
        self.assertGreaterEqual(result["score"].min(), 0.0)
        self.assertLessEqual(result["score"].max(), 100.0)

    def test_recommend_low_load_station_returns_clean_api_records(self):
        recommendations = recommend_low_load_station(
            self.predictions,
            self.stations,
            self.history,
            top_n=2,
        )

        self.assertEqual([{"station_id": "S1", "score": recommendations[0]["score"], "reason": "Low predicted congestion"}], recommendations[:1])
        self.assertNotIn("S3", [item["station_id"] for item in recommendations])

    def test_detect_peak_station_returns_threshold_alerts(self):
        alerts = detect_peak_station(self.predictions, threshold=80.0)

        self.assertEqual(1, len(alerts))
        self.assertEqual("S2", alerts.iloc[0]["station_id"])
        self.assertEqual("high", alerts.iloc[0]["alert_level"])
        self.assertEqual("2026-09-16 10:00:00", alerts.iloc[0]["peak_time"])


if __name__ == "__main__":
    unittest.main()
