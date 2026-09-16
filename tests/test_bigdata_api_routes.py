import unittest

from fastapi.testclient import TestClient

from run_api import app


class BigDataApiRouteTests(unittest.TestCase):
    def setUp(self):
        self.client = TestClient(app)

    def test_stats_routes_return_payloads(self):
        endpoints = [
            "/api/stats/charging-volume-trend?days=7",
            "/api/stats/revenue-trend?days=7",
            "/api/stats/session-count-trend?days=7",
            "/api/stats/station-distribution",
            "/api/stats/device-status-chart",
            "/api/stats/filtered?date=2014-11-18&weekday=2&station_id=00582",
        ]

        for endpoint in endpoints:
            with self.subTest(endpoint=endpoint):
                response = self.client.get(endpoint)
                self.assertEqual(200, response.status_code, response.text)
                payload = response.json()
                self.assertEqual(0, payload.get("code"), payload)
                self.assertIn("data", payload)

    def test_filtered_stats_contains_expected_schema(self):
        response = self.client.get("/api/stats/filtered?date=2014-11-18&weekday=2&station_id=00582")
        self.assertEqual(200, response.status_code)
        payload = response.json()
        self.assertEqual(0, payload["code"])
        self.assertIsInstance(payload["data"], list)
        self.assertIn("hour", payload["data"][0])
        self.assertIn("charging_sessions", payload["data"][0])


if __name__ == "__main__":
    unittest.main()
