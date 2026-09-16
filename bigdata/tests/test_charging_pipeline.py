"""Run on the Linux VM with: python3 -m unittest bigdata.tests.test_charging_pipeline"""

import importlib.util
import os
import sys
import unittest

if __package__ in (None, ""):
    sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

PYSPARK_AVAILABLE = importlib.util.find_spec("pyspark") is not None

if PYSPARK_AVAILABLE:
    from pyspark.sql import SparkSession, functions as F

    from bigdata.analytics.charging_stats import (
        holiday_patterns, overall_charging_kpis, station_kpis, user_kpis,
        user_summary_kpis, weekday_patterns,
    )
    from bigdata.analytics.device_stats import device_kpis, device_operation_kpis
    from bigdata.analytics.weather_stats import weather_impact_kpis
    from bigdata.charging_warehouse import _parse_args as parse_warehouse_args
    from bigdata.etl.clean_charging import (
        clean_charging_data, clean_charging_orders, detect_charging_quality_issues,
        detect_charging_reference_issues,
    )
    from bigdata.etl.clean_tables import (
        clean_device_status, clean_devices, clean_stations, clean_users, clean_weather,
    )
    from bigdata.etl.load_data import _parse_args as parse_load_args, _valid_charging_measurements
    from bigdata.etl.schemas import CHARGING_ORDER_ODS_SCHEMA
else:
    SparkSession = None
    F = None
    CHARGING_ORDER_ODS_SCHEMA = None


def _raw_order(session_id="S1", energy="7.500", device_id="D1"):
    values = {
        "session_id": session_id,
        "user_id": "00042",
        "station_id": "00582",
        "device_id": device_id,
        "location_id": "00461",
        "weather_id": "W1",
        "created_at": "18/11/2014 15:40",
        "ended_at": "18/11/2014 17:10",
        "charge_time_hrs": "1.500",
        "start_hour": "15",
        "weekday": "1",
        "weekday_name": "Tue",
        "is_weekend": "0",
        "is_holiday": "0",
        "holiday_name": "非节假日",
        "is_workday": "1",
        "energy_kwh": energy,
        "fee_amount_cny": "1.25",
        "platform": "android",
        "order_status": "completed",
        "payment_status": "paid",
        "_corrupt_record": None,
    }
    return tuple(values[field.name] for field in CHARGING_ORDER_ODS_SCHEMA.fields)


@unittest.skipUnless(PYSPARK_AVAILABLE, "PySpark is required for charging pipeline tests")
class ChargingPipelineTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.spark = (
            SparkSession.builder.master("local[2]")
            .appName("ChargingPipelineTests")
            .config("spark.ui.enabled", "false")
            .config("spark.sql.session.timeZone", "Asia/Shanghai")
            .config("spark.driver.bindAddress", "127.0.0.1")
            .config("spark.driver.host", "127.0.0.1")
            .getOrCreate()
        )

    @classmethod
    def tearDownClass(cls):
        cls.spark.stop()

    def test_quality_rules_keep_valid_and_reject_invalid(self):
        source = self.spark.createDataFrame(
            [_raw_order(), _raw_order(session_id="S2", energy="-1")],
            CHARGING_ORDER_ODS_SCHEMA,
        )
        checked = detect_charging_quality_issues(source)
        issues = {row.session_id: row.quality_issues for row in checked.select("session_id", "quality_issues").collect()}
        self.assertEqual([], issues["S1"])
        self.assertIn("negative_energy_kwh", issues["S2"])
        self.assertEqual(1, clean_charging_orders(checked).count())

    def test_order_timestamp_accepts_current_and_generated_formats(self):
        first = _raw_order()
        values = dict(zip([field.name for field in CHARGING_ORDER_ODS_SCHEMA.fields], first))
        values.update(session_id="S2", created_at="2014-11-18 15:40:00", ended_at="2014-11-18 17:10:00")
        second = tuple(values[field.name] for field in CHARGING_ORDER_ODS_SCHEMA.fields)
        parsed = clean_charging_orders(self.spark.createDataFrame([first, second], CHARGING_ORDER_ODS_SCHEMA))
        self.assertEqual(2, parsed.count())

    def test_basic_measurement_predicate_uses_new_columns(self):
        source = self.spark.createDataFrame(
            [
                ("valid", 7.5, 1.25, 1.5, 15, 1),
                ("negative_energy", -1.0, 1.25, 1.5, 15, 1),
                ("invalid_hour", 7.5, 1.25, 1.5, 24, 1),
            ],
            ["case", "energy_kwh", "fee_amount_cny", "charge_time_hrs", "start_hour", "weekday"],
        )
        kept = {row.case for row in source.where(_valid_charging_measurements()).select("case").collect()}
        self.assertEqual({"valid"}, kept)

    def _dimensions(self):
        users = clean_users(self.spark.createDataFrame(
            [("00042", "2014-01-01 00:00:00", "郑州市", "gold", "sedan", "60.0", "android", "app", "active")],
            ["user_id", "registered_at", "city", "member_level", "vehicle_type", "battery_capacity_kwh", "preferred_platform", "registration_channel", "user_status"],
        ))
        stations = clean_stations(self.spark.createDataFrame(
            [("00582", "00461", "Station A", "Address A", "郑州市", "34.7", "113.6", "3", "1", "00:00-24:00", "0.80", "0.20", "active", "2015-01-01")],
            ["station_id", "location_id", "station_name", "address", "city", "latitude", "longitude", "facility_type", "device_count", "open_time", "electricity_price_cny_kwh", "service_fee_cny_kwh", "station_status", "updated_on"],
        ))
        devices = clean_devices(self.spark.createDataFrame(
            [("D1", "00582", "PILE-1", "DC_FAST", "80", "2", "Maker", "2014-01-01", "2015-01-01", "online")],
            ["device_id", "station_id", "device_code", "charger_type", "rated_power_kw", "connector_count", "manufacturer", "commissioned_at", "last_maintenance_at", "current_status"],
        ))
        weather = clean_weather(self.spark.createDataFrame(
            [("W1", "00461", "2014-11-18 15:00:00", "sunny", "晴", "16", "50", "0", "2", "18", "0")],
            ["weather_id", "location_id", "weather_time", "weather_type", "weather_name", "temperature_c", "humidity_pct", "precipitation_mm", "wind_speed_mps", "visibility_km", "is_rain"],
        ))
        return users, stations, devices, weather

    def test_dimension_and_status_cleaners(self):
        users, stations, devices, weather = self._dimensions()
        self.assertEqual((1, 1, 1, 1), (users.count(), stations.count(), devices.count(), weather.count()))
        self.assertEqual(1, self._status().count())

    def _status(self):
        source = self.spark.createDataFrame(
            [("L1", "D1", "00582", "18/11/2014 23:59", "1440", "0", "0", "0", "NONE", "1", "0", "4.3", "1", "98", "0")],
            ["status_id", "device_id", "station_id", "record_time", "online_minutes", "offline_minutes", "fault_minutes", "fault_count", "primary_fault_code", "successful_sessions", "failed_sessions", "avg_output_power_kw", "availability_rate", "health_score", "maintenance_flag"],
        )
        return clean_device_status(source)

    def test_reference_checks_reject_unknown_device(self):
        users, stations, devices, weather = self._dimensions()
        source = self.spark.createDataFrame([_raw_order(device_id="UNKNOWN")], CHARGING_ORDER_ODS_SCHEMA)
        checked = detect_charging_reference_issues(
            detect_charging_quality_issues(source),
            users=users, stations=stations, devices=devices, weather=weather,
        )
        self.assertIn("unknown_device_id", checked.first().quality_issues)
        self.assertEqual(0, clean_charging_data(checked).count())

    def test_business_kpis_keep_existing_output_contract(self):
        source = self.spark.createDataFrame([_raw_order()], CHARGING_ORDER_ODS_SCHEMA)
        dwd = clean_charging_data(source)
        self.assertEqual("00042", dwd.first().user_id)

        overall = overall_charging_kpis(dwd).first()
        self.assertEqual(1, overall.total_charging_sessions)
        self.assertEqual("7.500", str(overall.total_kwh))
        self.assertEqual(1, user_kpis(dwd).first().charging_sessions)
        self.assertEqual(1, user_summary_kpis(dwd).first().total_users)

        station_dimension = self.spark.createDataFrame(
            [("00582", "Station A", 1), ("00999", "Station B", 3)],
            ["station_id", "station_name", "device_count"],
        )
        stations = {row.station_id: row for row in station_kpis(dwd, station_dimension).collect()}
        self.assertEqual(1, stations["00582"].charging_sessions)
        self.assertEqual(1.0, stations["00582"].sessions_per_device)
        self.assertEqual(0, stations["00999"].charging_sessions)
        self.assertEqual(7, len(weekday_patterns(dwd).collect()))

    def test_new_device_holiday_weather_and_operation_metrics(self):
        _, stations, devices, weather = self._dimensions()
        dwd = clean_charging_data(
            self.spark.createDataFrame([_raw_order()], CHARGING_ORDER_ODS_SCHEMA)
        )

        device = device_kpis(dwd, devices).first()
        self.assertEqual("D1", device.device_id)
        self.assertEqual(1, device.charging_sessions)
        self.assertGreater(device.charging_time_utilization_rate, 0)

        holiday = holiday_patterns(dwd).first()
        self.assertEqual("workday", holiday.day_type)
        self.assertEqual(1, holiday.charging_sessions)

        weather_row = weather_impact_kpis(dwd, weather, stations).first()
        self.assertEqual("sunny", weather_row.weather_type)
        self.assertEqual(1, weather_row.charging_sessions)
        self.assertEqual(1000.0, weather_row.sessions_per_1000_station_hours)

        operation = device_operation_kpis(self._status(), devices).first()
        self.assertEqual("D1", operation.device_id)
        self.assertEqual(1.0, operation.availability_rate)
        self.assertEqual(98.0, operation.avg_health_score)

    def test_cli_input_root_resolves_all_six_files(self):
        args = parse_warehouse_args(["--input-root", "hdfs:///data/raw", "--warehouse-root", "hdfs:///warehouse"])
        self.assertEqual("hdfs:///data/raw/users.csv", args.users_input)
        self.assertEqual("hdfs:///data/raw/device_status_log.csv", args.device_status_input)

        load_args = parse_load_args(["--input-root", "file:///data", "--output-root", "hdfs:///processed"])
        self.assertEqual("file:///data", load_args.input_root)
        self.assertEqual("hdfs:///processed", load_args.output_root)


if __name__ == "__main__":
    unittest.main()
