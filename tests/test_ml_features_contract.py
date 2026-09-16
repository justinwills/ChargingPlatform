"""Focused tests for the Wang Qingxiang ML feature-preparation contract."""

import importlib.util
import unittest


PYSPARK_AVAILABLE = importlib.util.find_spec("pyspark") is not None


@unittest.skipUnless(PYSPARK_AVAILABLE, "PySpark is required for ML feature tests")
class MLFeatureContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        from pyspark.sql import SparkSession

        cls.spark = (
            SparkSession.builder.master("local[2]")
            .appName("ChargingPlatformMLFeatureTests")
            .config("spark.ui.enabled", "false")
            .config("spark.sql.session.timeZone", "Asia/Shanghai")
            .config("spark.driver.bindAddress", "127.0.0.1")
            .config("spark.driver.host", "127.0.0.1")
            .getOrCreate()
        )

    @classmethod
    def tearDownClass(cls):
        cls.spark.stop()

    def test_feature_contract_excludes_identifiers_and_future_only_fields(self):
        from ml.features import FEATURE_COLUMNS, TARGET_COLUMNS, select_features

        columns = ["station_id", "location_id", "datetime"]
        columns += list(TARGET_COLUMNS)
        columns += list(FEATURE_COLUMNS)
        frame = self.spark.createDataFrame([tuple(range(len(columns)))], columns)

        selected = select_features(frame)

        self.assertNotIn("weather_id", selected.columns)
        self.assertNotIn("user_id", selected.columns)
        self.assertNotIn("device_id", selected.columns)
        self.assertNotIn("fee_amount_cny", selected.columns)
        self.assertNotIn("ended_at", selected.columns)
        self.assertEqual(
            ["station_id", "location_id", "datetime"],
            selected.columns[:3],
        )

    def test_time_features_use_monday_zero_weekday_numbering(self):
        from ml.features import create_time_features

        source = self.spark.createDataFrame(
            [("2026-09-14 15:40:00", 0, 0, 1)],
            ["created_at", "is_weekend", "is_holiday", "is_workday"],
        )
        row = create_time_features(source).first()

        self.assertEqual(15, row.hour)
        self.assertEqual(14, row.day)
        self.assertEqual(9, row.month)
        self.assertEqual(0, row.weekday_number)
        self.assertEqual(0, row.is_weekend)
        self.assertEqual(1, row.is_workday)

    def test_device_features_aggregate_status_by_station(self):
        from ml.features import build_device_features

        devices = self.spark.createDataFrame(
            [
                ("D1", "S1", "DC_FAST", 80.0, 2),
                ("D2", "S1", "AC_SLOW", 22.0, 1),
            ],
            ["device_id", "station_id", "charger_type", "rated_power_kw", "connector_count"],
        )
        status = self.spark.createDataFrame(
            [
                ("L1", "D1", "S1", 2, 10.0, 0.90, 95.0, 40.0),
                ("L2", "D2", "S1", 1, 5.0, 1.00, 97.0, 30.0),
            ],
            [
                "status_id",
                "device_id",
                "station_id",
                "fault_count",
                "fault_minutes",
                "availability_rate",
                "health_score",
                "avg_output_power_kw",
            ],
        )

        row = build_device_features(devices, status).first()

        self.assertEqual("S1", row.station_id)
        self.assertEqual(1.0, row.dc_fast_device_count)
        self.assertEqual(1.0, row.ac_slow_device_count)
        self.assertEqual(3, row.total_fault_count)
        self.assertEqual(15.0, row.total_fault_minutes)
        self.assertAlmostEqual(0.95, row.avg_availability_rate)


if __name__ == "__main__":
    unittest.main()
