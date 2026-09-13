"""使用 PySpark 进行充电会话质量检查和 DWD 数据清洗。

兼容 Spark 3.3.x。所有函数都返回延迟计算的 Spark DataFrame；ETL 层内部不会
隐藏调用 ``collect``。
"""

from typing import Iterable

from pyspark.sql import DataFrame, Window, functions as F
from pyspark.sql.types import DecimalType


WEEKDAY_TO_NUM = {
    "Mon": 0,
    "Tue": 1,
    "Wed": 2,
    "Thu": 3,
    "Fri": 4,
    "Sat": 5,
    "Sun": 6,
}

RAW_REQUIRED_COLUMNS = (
    "sessionId",
    "kwhTotal",
    "charging_fees",
    "created",
    "ended",
    "startTime",
    "endTime",
    "chargeTimeHrs",
    "weekday",
    "platform",
    "userId",
    "stationId",
    "locationId",
    "managerVehicle",
    "facilityType",
    "Mon",
    "Tues",
    "Wed",
    "Thurs",
    "Fri",
    "Sat",
    "Sun",
)

DWD_COLUMNS = (
    "session_id",
    "kwh_total",
    "charging_fees",
    "created_at",
    "ended_at",
    "start_hour",
    "end_hour",
    "charge_time_hrs",
    "start_weekday",
    "weekday_name",
    "start_date",
    "platform",
    "user_id",
    "station_id",
    "location_id",
    "manager_vehicle",
    "facility_type",
)


def _require_columns(df: DataFrame, required: Iterable[str]) -> None:
    missing = sorted(set(required) - set(df.columns))
    if missing:
        raise ValueError("缺少必需的充电数据列：" + ", ".join(missing))


def _blank(column_name: str):
    value = F.col(column_name)
    return value.isNull() | (F.trim(value) == "")


def _weekday_number():
    entries = []
    for name, number in WEEKDAY_TO_NUM.items():
        entries.extend((F.lit(name), F.lit(number)))
    return F.create_map(*entries).getItem(F.trim(F.col("weekday")))


def process_charging_time(df: DataFrame) -> DataFrame:
    """解析原始时间字段，并添加标准化的 DWD 候选列。

    在提供的数据集中，``startTime`` 和 ``endTime`` 是整数小时。
    ``created`` 和 ``ended`` 是完整时间戳，但年份已经匿名化处理。
    """
    _require_columns(df, RAW_REQUIRED_COLUMNS)

    return (
        df.withColumn("_created_at", F.to_timestamp(F.trim("created"), "dd/MM/yyyy HH:mm:ss"))
        .withColumn("_ended_at", F.to_timestamp(F.trim("ended"), "dd/MM/yyyy HH:mm:ss"))
        .withColumn("_kwh_total", F.trim("kwhTotal").cast(DecimalType(14, 3)))
        .withColumn("_charging_fees", F.trim("charging_fees").cast(DecimalType(14, 2)))
        .withColumn("_start_hour", F.trim("startTime").cast("int"))
        .withColumn("_end_hour", F.trim("endTime").cast("int"))
        .withColumn("_charge_time_hrs", F.trim("chargeTimeHrs").cast("double"))
        .withColumn("_start_weekday", _weekday_number())
        .withColumn("_manager_vehicle", F.trim("managerVehicle").cast("int"))
        .withColumn("_facility_type", F.trim("facilityType").cast("int"))
        .withColumn("_mon", F.trim("Mon").cast("int"))
        .withColumn("_tues", F.trim("Tues").cast("int"))
        .withColumn("_wed", F.trim("Wed").cast("int"))
        .withColumn("_thurs", F.trim("Thurs").cast("int"))
        .withColumn("_fri", F.trim("Fri").cast("int"))
        .withColumn("_sat", F.trim("Sat").cast("int"))
        .withColumn("_sun", F.trim("Sun").cast("int"))
    )


def detect_charging_quality_issues(df: DataFrame) -> DataFrame:
    """在保留每一条 ODS 数据的同时，添加 ``quality_issues`` 数组。"""
    typed = process_charging_time(df)

    duplicate_window = Window.partitionBy(F.trim(F.col("sessionId"))).orderBy(
        F.col("_created_at").asc_nulls_last(),
        F.col("_ended_at").asc_nulls_last(),
        F.trim(F.col("stationId")).asc_nulls_last(),
    )
    typed = typed.withColumn("_session_row_number", F.row_number().over(duplicate_window))

    one_hot_columns = ["_mon", "_tues", "_wed", "_thurs", "_fri", "_sat", "_sun"]
    one_hot_sum = sum((F.col(name) for name in one_hot_columns), F.lit(0))
    one_hot_values_valid = F.lit(True)
    for name in one_hot_columns:
        one_hot_values_valid = one_hot_values_valid & F.coalesce(
            F.col(name).isin(0, 1), F.lit(False)
        )

    expected_one_hot = (
        F.when(F.col("_start_weekday") == 0, F.col("_mon"))
        .when(F.col("_start_weekday") == 1, F.col("_tues"))
        .when(F.col("_start_weekday") == 2, F.col("_wed"))
        .when(F.col("_start_weekday") == 3, F.col("_thurs"))
        .when(F.col("_start_weekday") == 4, F.col("_fri"))
        .when(F.col("_start_weekday") == 5, F.col("_sat"))
        .when(F.col("_start_weekday") == 6, F.col("_sun"))
    )

    corrupt_record = F.col("_corrupt_record") if "_corrupt_record" in typed.columns else F.lit(None)
    issue_conditions = (
        (corrupt_record.isNotNull(), "corrupt_csv_row"),
        (_blank("sessionId"), "missing_session_id"),
        (_blank("userId"), "missing_user_id"),
        (_blank("stationId"), "missing_station_id"),
        (_blank("locationId"), "missing_location_id"),
        (_blank("kwhTotal"), "missing_kwh_total"),
        (~_blank("kwhTotal") & F.col("_kwh_total").isNull(), "invalid_kwh_total_type"),
        (F.col("_kwh_total") < 0, "negative_kwh_total"),
        (_blank("charging_fees"), "missing_charging_fees"),
        (~_blank("charging_fees") & F.col("_charging_fees").isNull(), "invalid_charging_fees_type"),
        (F.col("_charging_fees") < 0, "negative_charging_fees"),
        (_blank("created"), "missing_created"),
        (~_blank("created") & F.col("_created_at").isNull(), "invalid_created_timestamp"),
        (_blank("ended"), "missing_ended"),
        (~_blank("ended") & F.col("_ended_at").isNull(), "invalid_ended_timestamp"),
        (F.col("_ended_at") < F.col("_created_at"), "ended_before_created"),
        (
            F.col("_start_hour").isNull() | ~F.col("_start_hour").between(0, 23),
            "invalid_start_hour",
        ),
        (F.col("_end_hour").isNull() | ~F.col("_end_hour").between(0, 23), "invalid_end_hour"),
        (
            F.col("_charge_time_hrs").isNull()
            | (F.col("_charge_time_hrs") <= 0)
            | (F.col("_charge_time_hrs") > 24),
            "invalid_charge_duration",
        ),
        (F.col("_start_weekday").isNull(), "invalid_weekday"),
        (
            _blank("platform") | ~F.trim(F.col("platform")).isin("android", "ios", "web"),
            "invalid_platform",
        ),
        (
            F.col("_manager_vehicle").isNull() | ~F.col("_manager_vehicle").isin(0, 1),
            "invalid_manager_vehicle",
        ),
        (
            F.col("_facility_type").isNull() | (F.col("_facility_type") <= 0),
            "invalid_facility_type",
        ),
        (~one_hot_values_valid | (one_hot_sum != 1), "invalid_weekday_one_hot"),
        (
            F.col("_start_weekday").isNotNull()
            & one_hot_values_valid
            & (one_hot_sum == 1)
            & (expected_one_hot != 1),
            "weekday_one_hot_mismatch",
        ),
        (~_blank("sessionId") & (F.col("_session_row_number") > 1), "duplicate_session_id"),
    )

    raw_issues = F.array(
        *[F.when(condition, F.lit(issue_name)) for condition, issue_name in issue_conditions]
    )
    return (
        typed.withColumn("_raw_quality_issues", raw_issues)
        .withColumn(
            "quality_issues",
            F.expr("filter(_raw_quality_issues, issue -> issue is not null)"),
        )
        .drop("_raw_quality_issues")
    )


def clean_charging_data(df: DataFrame) -> DataFrame:
    """返回有效的 DWD 充电会话数据，并统一列名。"""
    checked = df if "quality_issues" in df.columns else detect_charging_quality_issues(df)

    return checked.where(F.size("quality_issues") == 0).select(
        F.trim("sessionId").alias("session_id"),
        F.col("_kwh_total").alias("kwh_total"),
        F.col("_charging_fees").alias("charging_fees"),
        F.col("_created_at").alias("created_at"),
        F.col("_ended_at").alias("ended_at"),
        F.col("_start_hour").alias("start_hour"),
        F.col("_end_hour").alias("end_hour"),
        F.col("_charge_time_hrs").alias("charge_time_hrs"),
        F.col("_start_weekday").alias("start_weekday"),
        F.trim("weekday").alias("weekday_name"),
        F.to_date("_created_at").alias("start_date"),
        F.trim("platform").alias("platform"),
        F.trim("userId").alias("user_id"),
        F.trim("stationId").alias("station_id"),
        F.trim("locationId").alias("location_id"),
        F.col("_manager_vehicle").alias("manager_vehicle"),
        F.col("_facility_type").alias("facility_type"),
    )


def rejected_charging_data(df: DataFrame) -> DataFrame:
    """返回未通过一条或多条质量规则的原始 ODS 数据。"""
    checked = df if "quality_issues" in df.columns else detect_charging_quality_issues(df)
    visible_columns = [name for name in RAW_REQUIRED_COLUMNS if name in checked.columns]
    visible_columns.extend(
        name
        for name in ("_corrupt_record", "_source_file", "_ingested_at", "quality_issues")
        if name in checked.columns
    )
    return checked.where(F.size("quality_issues") > 0).select(*visible_columns)


def quality_issue_summary(df: DataFrame) -> DataFrame:
    """返回总行数、有效/拒绝行数以及每类问题的数量。"""
    checked = df if "quality_issues" in df.columns else detect_charging_quality_issues(df)
    overview = checked.agg(
        F.count("*").alias("total_rows"),
        F.sum(F.when(F.size("quality_issues") == 0, 1).otherwise(0)).alias("valid_rows"),
        F.sum(F.when(F.size("quality_issues") > 0, 1).otherwise(0)).alias("rejected_rows"),
    ).selectExpr(
        "stack(3, "
        "'total_rows', total_rows, "
        "'valid_rows', valid_rows, "
        "'rejected_rows', rejected_rows) AS (metric, count)"
    )
    issues = (
        checked.select(F.explode("quality_issues").alias("issue"))
        .groupBy("issue")
        .count()
        .select(F.concat(F.lit("issue:"), F.col("issue")).alias("metric"), "count")
    )
    return overview.unionByName(issues).orderBy("metric")
