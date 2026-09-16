import unittest

from run_api import app


class BigDataApiRouteTests(unittest.TestCase):
    def setUp(self):
        self.client = app.test_client()

    def test_stats_routes_return_payloads(self):
        endpoints = [
            "/api/stats/charging-volume-trend?days=7",
            "/api/stats/revenue-trend?days=7",
            "/api/stats/session-count-trend?days=7",
            "/api/stats/station-distribution",
            "/api/stats/device-status-chart",
            "/api/stats/filtered?date=2024-11-18&weekday=1&station_id=371335",
        ]

        for endpoint in endpoints:
            with self.subTest(endpoint=endpoint):
                response = self.client.get(endpoint)
                self.assertEqual(200, response.status_code, response.get_data(as_text=True))
                payload = response.get_json()
                self.assertEqual(0, payload.get("code"), payload)
                self.assertIn("data", payload)

    def test_filtered_stats_contains_expected_schema(self):
        response = self.client.get("/api/stats/filtered?date=2024-11-18&weekday=1&station_id=371335")
        self.assertEqual(200, response.status_code)
        payload = response.get_json()
        self.assertEqual(0, payload["code"])
        self.assertIsInstance(payload["data"], dict)
        self.assertIn("total_energy_kwh", payload["data"])
        self.assertIn("total_sessions", payload["data"])
        self.assertIn("points", payload["data"])


if __name__ == "__main__":
    unittest.main()
