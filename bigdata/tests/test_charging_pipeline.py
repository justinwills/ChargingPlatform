"""Run on the Linux VM with: python3 -m unittest bigdata.tests.test_charging_pipeline"""

import unittest
import os
import sys

if __package__ in (None, ""):
    sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from pyspark.sql import SparkSession

from bigdata.analytics.charging_stats import (
    overall_charging_kpis,
    station_kpis,
    user_kpis,
    user_summary_kpis,
    weekday_patterns,
)
from bigdata.etl.clean_charging import (
    clean_charging_data,
    detect_charging_quality_issues,
)
from bigdata.etl.schemas import CHARGING_ODS_SCHEMA


def _raw_row(session_id="1", kwh="7.50", weekday="Tue", start_hour="15"):
    values = {
        "sessionId": session_id,
        "kwhTotal": kwh,
        "charging_fees": "1.25",
        "created": "0014-11-18 15:40:26",
        "ended": "0014-11-18 17:11:04",
        "startTime": start_hour,
        "endTime": "17",
        "chargeTimeHrs": "1.510555556",
        "weekday": weekday,
        "platform": "android",
        "userId": "00042",
        "stationId": "00582",
        "locationId": "00461",
        "managerVehicle": "0",
        "facilityType": "3",
        "Mon": "0",
        "Tues": "1",
        "Wed": "0",
        "Thurs": "0",
        "Fri": "0",
        "Sat": "0",
        "Sun": "0",
        "_corrupt_record": None,
    }
    return tuple(values[field.name] for field in CHARGING_ODS_SCHEMA.fields)


class ChargingPipelineTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.spark = (
            SparkSession.builder.master("local[2]")
            .appName("ChargingPipelineTests")
            .config("spark.ui.enabled", "false")
            .getOrCreate()
        )

    @classmethod
    def tearDownClass(cls):
        cls.spark.stop()

    def test_quality_rules_keep_valid_and_reject_invalid(self):
        source = self.spark.createDataFrame(
            [_raw_row(), _raw_row(session_id="2", kwh="-1")],
            CHARGING_ODS_SCHEMA,
        )
        checked = detect_charging_quality_issues(source)
        issues = {
            row.sessionId: row.quality_issues
            for row in checked.select("sessionId", "quality_issues").collect()
        }
        self.assertEqual([], issues["1"])
        self.assertIn("negative_kwh_total", issues["2"])
        self.assertEqual(1, clean_charging_data(checked).count())

    def test_business_kpis_and_identifier_leading_zero(self):
        source = self.spark.createDataFrame([_raw_row()], CHARGING_ODS_SCHEMA)
        dwd = clean_charging_data(source)
        self.assertEqual("00042", dwd.first().user_id)

        overall = overall_charging_kpis(dwd).first()
        self.assertEqual(1, overall.total_charging_sessions)
        self.assertEqual("7.500", str(overall.total_kwh))

        user = user_kpis(dwd).first()
        self.assertEqual("00042", user.user_id)
        self.assertEqual(1, user.charging_sessions)

        user_summary = user_summary_kpis(dwd).first()
        self.assertEqual(1, user_summary.total_users)

        station_dimension = self.spark.createDataFrame(
            [("00582", "Station A", 2), ("00999", "Station B", 3)],
            ["station_id", "station_name", "device_count"],
        )
        stations = {row.station_id: row for row in station_kpis(dwd, station_dimension).collect()}
        self.assertEqual(1, stations["00582"].charging_sessions)
        self.assertEqual(0, stations["00999"].charging_sessions)

        weekdays = weekday_patterns(dwd).collect()
        self.assertEqual(7, len(weekdays))
        self.assertEqual(1, weekdays[1].charging_sessions)


if __name__ == "__main__":
    unittest.main()
