"""Run on the Linux VM with: python3 -m unittest bigdata.tests.test_charging_pipeline"""

import unittest
import os
import sys

if __package__ in (None, ""):
    sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from pyspark.sql import SparkSession, functions as F

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
from bigdata.etl.load_data import (
    _valid_battery_measurements,
    _valid_charging_measurements,
)
from bigdata.etl.schemas import CHARGING_ODS_SCHEMA, SOURCE_TIMESTAMP_FORMAT


def _raw_row(session_id="1", kwh="7.50", weekday="Tue", start_hour="15"):
    values = {
        "sessionId": session_id,
        "kwhTotal": kwh,
        "charging_fees": "1.25",
        "created": "18/11/2014 15:40",
        "ended": "18/11/2014 17:11",
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

    def test_current_teacher_timestamp_format(self):
        source = self.spark.createDataFrame(
            [("18/11/2014 17:11",), ("3/12/2014 21:02",)],
            ["record_time"],
        )
        parsed = source.select(
            F.to_timestamp(F.trim("record_time"), SOURCE_TIMESTAMP_FORMAT).alias(
                "record_time"
            )
        ).collect()

        self.assertEqual("2014-11-18 17:11:00", str(parsed[0].record_time))
        self.assertEqual("2014-12-03 21:02:00", str(parsed[1].record_time))

    def test_basic_loader_rejects_all_injected_charging_anomaly_types(self):
        valid = {
            "case": "valid",
            "kwhTotal": 7.5,
            "charging_fees": 1.25,
            "chargeTimeHrs": 1.5,
            "startTime": 15,
            "endTime": 17,
        }
        rows = [valid]
        for case, field, value in (
            ("negative_kwh", "kwhTotal", -1.0),
            ("negative_fee", "charging_fees", -0.5),
            ("long_duration", "chargeTimeHrs", 25.0),
            ("invalid_start_hour", "startTime", 24),
            ("invalid_end_hour", "endTime", -1),
        ):
            row = dict(valid)
            row.update(case=case, **{field: value})
            rows.append(row)

        source = self.spark.createDataFrame(rows)
        kept = {
            row.case
            for row in source.where(_valid_charging_measurements())
            .select("case")
            .collect()
        }
        self.assertEqual({"valid"}, kept)

    def test_basic_loader_rejects_all_injected_battery_anomaly_types(self):
        valid = {
            "case": "valid",
            "soc": 50.0,
            "pack_voltage": 340.0,
            "charge_current": -22.5,
            "max_cell_voltage": 3.8,
            "min_cell_voltage": 3.7,
            "max_temperature": 36.0,
            "min_temperature": 34.0,
            "available_energy": 20.0,
            "available_capacity": 60.0,
        }
        rows = [valid]
        for case, field, value in (
            ("soc_over_100", "soc", 101.0),
            ("negative_pack_voltage", "pack_voltage", -100.0),
            ("max_cell_below_min", "max_cell_voltage", 0.5),
            ("extreme_temperature", "max_temperature", 120.0),
            ("negative_capacity", "available_capacity", -1.0),
        ):
            row = dict(valid)
            row.update(case=case, **{field: value})
            rows.append(row)

        source = self.spark.createDataFrame(rows)
        kept = {
            row.case
            for row in source.where(_valid_battery_measurements())
            .select("case")
            .collect()
        }
        self.assertEqual({"valid"}, kept)

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
