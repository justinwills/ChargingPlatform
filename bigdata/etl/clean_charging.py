"""Quality checks and DWD cleaning for the current charging_orders.csv."""

from typing import Iterable, Optional

from pyspark.sql import DataFrame, Window, functions as F
from pyspark.sql.types import DecimalType

from .schemas import ORDER_TIMESTAMP_FORMAT, STANDARD_TIMESTAMP_FORMAT


WEEKDAY_TO_NUM = {"Mon": 0, "Tue": 1, "Wed": 2, "Thu": 3, "Fri": 4, "Sat": 5, "Sun": 6}

RAW_REQUIRED_COLUMNS = (
    "session_id", "user_id", "station_id", "device_id", "location_id",
    "weather_id", "created_at", "ended_at", "charge_time_hrs",
    "start_hour", "weekday", "weekday_name", "is_weekend", "is_holiday",
    "holiday_name", "is_workday", "energy_kwh", "fee_amount_cny",
    "platform", "order_status", "payment_status",
)

DWD_COLUMNS = (
    "session_id", "kwh_total", "charging_fees", "created_at", "ended_at",
    "start_hour", "end_hour", "charge_time_hrs", "start_weekday",
    "weekday_name", "start_date", "platform", "user_id", "station_id",
    "device_id", "location_id", "weather_id", "is_weekend", "is_holiday",
    "holiday_name", "is_workday", "order_status", "payment_status",
)


def _require_columns(df: DataFrame, required: Iterable[str]) -> None:
    missing = sorted(set(required) - set(df.columns))
    if missing:
        raise ValueError("缺少必需的充电订单列：" + ", ".join(missing))


def _blank(column_name: str):
    value = F.col(column_name)
    return value.isNull() | (F.trim(value) == "")


def _weekday_number(column_name: str = "weekday_name"):
    entries = []
    for name, number in WEEKDAY_TO_NUM.items():
        entries.extend((F.lit(name), F.lit(number)))
    return F.create_map(*entries).getItem(F.trim(F.col(column_name)))


def _parse_timestamp(column_name: str):
    value = F.trim(F.col(column_name))
    if hasattr(F, "try_to_timestamp"):
        return F.coalesce(
            F.try_to_timestamp(value, F.lit(STANDARD_TIMESTAMP_FORMAT)),
            F.try_to_timestamp(value, F.lit(ORDER_TIMESTAMP_FORMAT)),
        )
    return F.coalesce(
        F.to_timestamp(value, STANDARD_TIMESTAMP_FORMAT),
        F.to_timestamp(value, ORDER_TIMESTAMP_FORMAT),
    )


def process_charging_time(df: DataFrame) -> DataFrame:
    """Parse and normalize raw order values without dropping source rows."""
    _require_columns(df, RAW_REQUIRED_COLUMNS)
    return (
        df.withColumn(
            "_created_at",
            _parse_timestamp("created_at"),
        )
        .withColumn(
            "_ended_at",
            _parse_timestamp("ended_at"),
        )
        .withColumn("_energy_kwh", F.trim("energy_kwh").cast(DecimalType(14, 3)))
        .withColumn("_fee_amount_cny", F.trim("fee_amount_cny").cast(DecimalType(14, 2)))
        .withColumn("_charge_time_hrs", F.trim("charge_time_hrs").cast("double"))
        .withColumn("_start_hour", F.trim("start_hour").cast("int"))
        .withColumn("_weekday", F.trim("weekday").cast("int"))
        .withColumn("_weekday_from_name", _weekday_number())
        .withColumn("_is_weekend", F.trim("is_weekend").cast("int"))
        .withColumn("_is_holiday", F.trim("is_holiday").cast("int"))
        .withColumn("_is_workday", F.trim("is_workday").cast("int"))
        .withColumn(
            "_actual_duration_hrs",
            (F.col("_ended_at").cast("long") - F.col("_created_at").cast("long")) / 3600.0,
        )
        .withColumn(
            "_weekday_from_timestamp",
            (F.dayofweek("_created_at") + F.lit(5)) % F.lit(7),
        )
    )


def detect_charging_quality_issues(df: DataFrame) -> DataFrame:
    """保持每个ODS行并附加一个``quality_issues``字符串数组。"""
    typed = process_charging_time(df)
    duplicate_window = Window.partitionBy(F.trim(F.col("session_id"))).orderBy(
        F.col("_created_at").asc_nulls_last(),
        F.col("_ended_at").asc_nulls_last(),
        F.trim(F.col("station_id")).asc_nulls_last(),
    )
    typed = typed.withColumn("_session_row_number", F.row_number().over(duplicate_window))
    corrupt_record = F.col("_corrupt_record") if "_corrupt_record" in typed.columns else F.lit(None)

    issue_conditions = (
        (corrupt_record.isNotNull(), "corrupt_csv_row"),
        (_blank("session_id"), "missing_session_id"),
        (_blank("user_id"), "missing_user_id"),
        (_blank("station_id"), "missing_station_id"),
        (_blank("device_id"), "missing_device_id"),
        (_blank("location_id"), "missing_location_id"),
        (_blank("weather_id"), "missing_weather_id"),
        (_blank("energy_kwh"), "missing_energy_kwh"),
        (~_blank("energy_kwh") & F.col("_energy_kwh").isNull(), "invalid_energy_kwh_type"),
        (F.col("_energy_kwh") < 0, "negative_energy_kwh"),
        (_blank("fee_amount_cny"), "missing_fee_amount_cny"),
        (~_blank("fee_amount_cny") & F.col("_fee_amount_cny").isNull(), "invalid_fee_amount_cny_type"),
        (F.col("_fee_amount_cny") < 0, "negative_fee_amount_cny"),
        (_blank("created_at"), "missing_created_at"),
        (~_blank("created_at") & F.col("_created_at").isNull(), "invalid_created_at"),
        (_blank("ended_at"), "missing_ended_at"),
        (~_blank("ended_at") & F.col("_ended_at").isNull(), "invalid_ended_at"),
        (F.col("_ended_at") <= F.col("_created_at"), "ended_not_after_created"),
        (
            F.col("_charge_time_hrs").isNull()
            | (F.col("_charge_time_hrs") <= 0)
            | (F.col("_charge_time_hrs") > 24),
            "invalid_charge_duration",
        ),
        (
            F.col("_actual_duration_hrs").isNotNull()
            & F.col("_charge_time_hrs").isNotNull()
            & (F.abs(F.col("_actual_duration_hrs") - F.col("_charge_time_hrs")) > 0.02),
            "charge_duration_mismatch",
        ),
        (F.col("_start_hour").isNull() | ~F.col("_start_hour").between(0, 23), "invalid_start_hour"),
        (
            F.col("_created_at").isNotNull()
            & F.col("_start_hour").isNotNull()
            & (F.hour("_created_at") != F.col("_start_hour")),
            "start_hour_mismatch",
        ),
        (F.col("_weekday").isNull() | ~F.col("_weekday").between(0, 6), "invalid_weekday"),
        (F.col("_weekday_from_name").isNull(), "invalid_weekday_name"),
        (
            F.col("_weekday").isNotNull()
            & F.col("_weekday_from_name").isNotNull()
            & (F.col("_weekday") != F.col("_weekday_from_name")),
            "weekday_name_mismatch",
        ),
        (
            F.col("_created_at").isNotNull()
            & F.col("_weekday").isNotNull()
            & (F.col("_weekday") != F.col("_weekday_from_timestamp")),
            "weekday_timestamp_mismatch",
        ),
        (~F.coalesce(F.col("_is_weekend").isin(0, 1), F.lit(False)), "invalid_is_weekend"),
        (~F.coalesce(F.col("_is_holiday").isin(0, 1), F.lit(False)), "invalid_is_holiday"),
        (~F.coalesce(F.col("_is_workday").isin(0, 1), F.lit(False)), "invalid_is_workday"),
        (
            F.col("_weekday").isNotNull()
            & F.col("_is_weekend").isNotNull()
            & (F.col("_is_weekend") != F.when(F.col("_weekday") >= 5, 1).otherwise(0)),
            "weekend_flag_mismatch",
        ),
        (_blank("holiday_name"), "missing_holiday_name"),
        (_blank("platform") | ~F.lower(F.trim("platform")).isin("android", "ios", "web"), "invalid_platform"),
        (_blank("order_status") | ~F.lower(F.trim("order_status")).isin("completed", "interrupted", "failed"), "invalid_order_status"),
        (_blank("payment_status") | ~F.lower(F.trim("payment_status")).isin("paid", "pending", "refunded", "waived"), "invalid_payment_status"),
        (~_blank("session_id") & (F.col("_session_row_number") > 1), "duplicate_session_id"),
    )

    raw_issues = F.array(*[F.when(condition, F.lit(name)) for condition, name in issue_conditions])
    return (
        typed.withColumn("_raw_quality_issues", raw_issues)
        .withColumn("quality_issues", F.expr("filter(_raw_quality_issues, x -> x is not null)"))
        .drop("_raw_quality_issues")
    )

# 添加质量问题的辅助函数
def _append_issue(df: DataFrame, condition, issue_name: str) -> DataFrame:
    return (
        df.withColumn("_reference_issue", F.when(condition, F.lit(issue_name)))
        .withColumn(
            "quality_issues",
            F.expr("filter(concat(quality_issues, array(_reference_issue)), x -> x is not null)"),
        )
        .drop("_reference_issue")
    )


def detect_charging_reference_issues(
    checked: DataFrame,
    *,
    users: Optional[DataFrame] = None,
    stations: Optional[DataFrame] = None,
    devices: Optional[DataFrame] = None,
    weather: Optional[DataFrame] = None,
) -> DataFrame:
    """增加外键和跨表一致性问题，而不收集键。"""
    result = checked if "quality_issues" in checked.columns else detect_charging_quality_issues(checked)

    if users is not None:
        reference = users.select(F.col("user_id").alias("_ref_user_id")).dropDuplicates()
        result = result.join(F.broadcast(reference), F.trim(result.user_id) == F.col("_ref_user_id"), "left")
        result = _append_issue(result, ~_blank("user_id") & F.col("_ref_user_id").isNull(), "unknown_user_id").drop("_ref_user_id")

    if stations is not None:
        reference = stations.select(
            F.col("station_id").alias("_ref_station_id"),
            F.col("location_id").alias("_station_location_id"),
        ).dropDuplicates(["_ref_station_id"])
        result = result.join(F.broadcast(reference), F.trim(result.station_id) == F.col("_ref_station_id"), "left")
        result = _append_issue(result, ~_blank("station_id") & F.col("_ref_station_id").isNull(), "unknown_station_id")
        result = _append_issue(
            result,
            F.col("_ref_station_id").isNotNull()
            & (F.trim(result.location_id) != F.col("_station_location_id")),
            "station_location_mismatch",
        )

    if devices is not None:
        reference = devices.select(
            F.col("device_id").alias("_ref_device_id"),
            F.col("station_id").alias("_device_station_id"),
        ).dropDuplicates(["_ref_device_id"])
        result = result.join(F.broadcast(reference), F.trim(result.device_id) == F.col("_ref_device_id"), "left")
        result = _append_issue(result, ~_blank("device_id") & F.col("_ref_device_id").isNull(), "unknown_device_id")
        result = _append_issue(
            result,
            F.col("_ref_device_id").isNotNull()
            & (F.trim(result.station_id) != F.col("_device_station_id")),
            "device_station_mismatch",
        )

    if weather is not None:
        reference = weather.select(
            F.col("weather_id").alias("_ref_weather_id"),
            F.col("location_id").alias("_weather_location_id"),
            F.col("weather_time").alias("_weather_time"),
        ).dropDuplicates(["_ref_weather_id"])
        result = result.join(reference, F.trim(result.weather_id) == F.col("_ref_weather_id"), "left")
        result = _append_issue(result, ~_blank("weather_id") & F.col("_ref_weather_id").isNull(), "unknown_weather_id")
        result = _append_issue(
            result,
            F.col("_ref_weather_id").isNotNull()
            & (F.trim(result.location_id) != F.col("_weather_location_id")),
            "weather_location_mismatch",
        )
        result = _append_issue(
            result,
            F.col("_ref_weather_id").isNotNull()
            & (F.date_trunc("hour", F.col("_created_at")) != F.col("_weather_time")),
            "weather_time_mismatch",
        )

    return result.drop(
        "_ref_station_id", "_station_location_id", "_ref_device_id",
        "_device_station_id", "_ref_weather_id", "_weather_location_id", "_weather_time",
    )


def clean_charging_data(df: DataFrame) -> DataFrame:
    """Return valid DWD orders while preserving the existing analytics contract."""
    checked = df if "quality_issues" in df.columns else detect_charging_quality_issues(df)
    return checked.where(F.size("quality_issues") == 0).select(
        F.trim("session_id").alias("session_id"),
        F.col("_energy_kwh").alias("kwh_total"),
        F.col("_fee_amount_cny").alias("charging_fees"),
        F.col("_created_at").alias("created_at"),
        F.col("_ended_at").alias("ended_at"),
        F.col("_start_hour").alias("start_hour"),
        F.hour("_ended_at").alias("end_hour"),
        F.col("_charge_time_hrs").alias("charge_time_hrs"),
        F.col("_weekday").alias("start_weekday"),
        F.trim("weekday_name").alias("weekday_name"),
        F.to_date("_created_at").alias("start_date"),
        F.lower(F.trim("platform")).alias("platform"),
        F.trim("user_id").alias("user_id"),
        F.trim("station_id").alias("station_id"),
        F.trim("device_id").alias("device_id"),
        F.trim("location_id").alias("location_id"),
        F.trim("weather_id").alias("weather_id"),
        F.col("_is_weekend").alias("is_weekend"),
        F.col("_is_holiday").alias("is_holiday"),
        F.trim("holiday_name").alias("holiday_name"),
        F.col("_is_workday").alias("is_workday"),
        F.lower(F.trim("order_status")).alias("order_status"),
        F.lower(F.trim("payment_status")).alias("payment_status"),
    )


def clean_charging_orders(df: DataFrame) -> DataFrame:
    """Return valid typed orders using the current CSV's public column names."""
    checked = df if "quality_issues" in df.columns else detect_charging_quality_issues(df)
    return checked.where(F.size("quality_issues") == 0).select(
        F.trim("session_id").alias("session_id"),
        F.trim("user_id").alias("user_id"),
        F.trim("station_id").alias("station_id"),
        F.trim("device_id").alias("device_id"),
        F.trim("location_id").alias("location_id"),
        F.trim("weather_id").alias("weather_id"),
        F.col("_created_at").alias("created_at"),
        F.col("_ended_at").alias("ended_at"),
        F.col("_charge_time_hrs").alias("charge_time_hrs"),
        F.col("_start_hour").alias("start_hour"),
        F.col("_weekday").alias("weekday"),
        F.trim("weekday_name").alias("weekday_name"),
        F.col("_is_weekend").alias("is_weekend"),
        F.col("_is_holiday").alias("is_holiday"),
        F.trim("holiday_name").alias("holiday_name"),
        F.col("_is_workday").alias("is_workday"),
        F.col("_energy_kwh").alias("energy_kwh"),
        F.col("_fee_amount_cny").alias("fee_amount_cny"),
        F.lower(F.trim("platform")).alias("platform"),
        F.lower(F.trim("order_status")).alias("order_status"),
        F.lower(F.trim("payment_status")).alias("payment_status"),
    )


def rejected_charging_data(df: DataFrame) -> DataFrame:
    checked = df if "quality_issues" in df.columns else detect_charging_quality_issues(df)
    visible = [name for name in RAW_REQUIRED_COLUMNS if name in checked.columns]
    visible.extend(name for name in ("_corrupt_record", "_source_file", "_ingested_at", "quality_issues") if name in checked.columns)
    return checked.where(F.size("quality_issues") > 0).select(*visible)


def quality_issue_summary(df: DataFrame) -> DataFrame:
    checked = df if "quality_issues" in df.columns else detect_charging_quality_issues(df)
    overview = checked.agg(
        F.count("*").alias("total_rows"),
        F.sum(F.when(F.size("quality_issues") == 0, 1).otherwise(0)).alias("valid_rows"),
        F.sum(F.when(F.size("quality_issues") > 0, 1).otherwise(0)).alias("rejected_rows"),
    ).selectExpr(
        "stack(3, 'total_rows', total_rows, 'valid_rows', valid_rows, "
        "'rejected_rows', rejected_rows) AS (metric, count)"
    )
    issues = (
        checked.select(F.explode("quality_issues").alias("issue"))
        .groupBy("issue").count()
        .select(F.concat(F.lit("issue:"), F.col("issue")).alias("metric"), "count")
    )
    return overview.unionByName(issues).orderBy("metric")
