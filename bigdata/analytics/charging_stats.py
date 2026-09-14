"""SparkSQL charging-operation metrics for DWS and ADS layers."""

from pyspark.sql import DataFrame, functions as F


REQUIRED_DWD_COLUMNS = {
    "session_id",
    "kwh_total",
    "charging_fees",
    "charge_time_hrs",
    "start_hour",
    "start_weekday",
    "weekday_name",
    "user_id",
    "station_id",
}


def _require_dwd(df: DataFrame) -> None:
    missing = sorted(REQUIRED_DWD_COLUMNS - set(df.columns))
    if missing:
        raise ValueError("Missing required DWD columns: " + ", ".join(missing))

# 计算整体充电 KPI总充电次数。总充电量。总充电费用。平均充电时长。平均单次充电量。
def overall_charging_kpis(df: DataFrame) -> DataFrame:
    """Task #66: return one ADS row containing overall charging KPIs."""
    _require_dwd(df)
    df.createOrReplaceTempView("_dwd_charging_overall")
    return df.sparkSession.sql(
        """
        SELECT
            COUNT(DISTINCT session_id) AS total_charging_sessions,
            ROUND(SUM(kwh_total), 3) AS total_kwh,
            ROUND(SUM(charging_fees), 2) AS total_charging_fees,
            ROUND(AVG(charge_time_hrs), 4) AS avg_charge_time_hrs,
            ROUND(SUM(kwh_total) / COUNT(DISTINCT session_id), 3)
                AS avg_kwh_per_session
        FROM _dwd_charging_overall
        """
    )


def station_kpis(charging_df: DataFrame, station_df: DataFrame) -> DataFrame:
    """Task #67: aggregate sessions and join station names/device counts."""
    _require_dwd(charging_df)
    required_station = {"station_id", "station_name", "device_count"}
    missing = sorted(required_station - set(station_df.columns))
    if missing:
        raise ValueError("Missing required station columns: " + ", ".join(missing))

    charging_df.createOrReplaceTempView("_dwd_charging_station")
    station_df.createOrReplaceTempView("_dwd_station_dimension")
    return charging_df.sparkSession.sql(
        """
        WITH station_usage AS (
            SELECT
                station_id,
                COUNT(DISTINCT session_id) AS charging_sessions,
                ROUND(SUM(kwh_total), 3) AS total_kwh,
                ROUND(SUM(charging_fees), 2) AS total_charging_fees,
                ROUND(AVG(charge_time_hrs), 4) AS avg_charge_time_hrs
            FROM _dwd_charging_station
            GROUP BY station_id
        )
        SELECT
            COALESCE(dimension.station_id, usage.station_id) AS station_id,
            dimension.station_name,
            dimension.device_count,
            COALESCE(usage.charging_sessions, 0) AS charging_sessions,
            COALESCE(usage.total_kwh, CAST(0 AS DECIMAL(24, 3))) AS total_kwh,
            COALESCE(usage.total_charging_fees, CAST(0 AS DECIMAL(24, 2)))
                AS total_charging_fees,
            usage.avg_charge_time_hrs,
            ROUND(
                CASE WHEN dimension.device_count > 0
                    THEN COALESCE(usage.charging_sessions, 0)
                        / dimension.device_count
                END,
                3
            ) AS sessions_per_device,
            ROUND(
                CASE WHEN dimension.device_count > 0
                    THEN COALESCE(usage.total_kwh, 0)
                        / dimension.device_count
                END,
                3
            ) AS kwh_per_device,
            ROUND(
                CASE WHEN dimension.device_count > 0
                    THEN COALESCE(usage.total_charging_fees, 0)
                        / dimension.device_count
                END,
                2
            ) AS charging_fees_per_device
        FROM _dwd_station_dimension dimension
        FULL OUTER JOIN station_usage usage
          ON usage.station_id = dimension.station_id
        ORDER BY charging_sessions DESC, station_id
        """
    )

# 每个用户生成一行：- 用户充电次数。- 累计充电量。- 累计充电费用。- 平均单次充电量。- 平均充电时长。
def user_kpis(df: DataFrame) -> DataFrame:
    """Task #68: return one DWS row per user; row count is total users."""
    _require_dwd(df)
    df.createOrReplaceTempView("_dwd_charging_user")
    return df.sparkSession.sql(
        """
        SELECT
            user_id,
            COUNT(DISTINCT session_id) AS charging_sessions,
            ROUND(SUM(kwh_total), 3) AS total_kwh,
            ROUND(SUM(charging_fees), 2) AS total_charging_fees,
            ROUND(AVG(kwh_total), 3) AS avg_kwh_per_session,
            ROUND(AVG(charge_time_hrs), 4) AS avg_charge_time_hrs
        FROM _dwd_charging_user
        GROUP BY user_id
        ORDER BY charging_sessions DESC, user_id
        """
    )

#在用户汇总基础上继续生成 ADS 总体用户指标：
# - 总用户数。
# - 平均每个用户充电次数。
# - 平均每个用户累计充电量。
def user_summary_kpis(df: DataFrame) -> DataFrame:
    """Task #68 ADS summary containing user count and behavior averages."""
    _require_dwd(df)
    df.createOrReplaceTempView("_dwd_charging_user_summary")
    return df.sparkSession.sql(
        """
        WITH user_usage AS (
            SELECT
                user_id,
                COUNT(DISTINCT session_id) AS charging_sessions,
                SUM(kwh_total) AS total_kwh
            FROM _dwd_charging_user_summary
            GROUP BY user_id
        )
        SELECT
            COUNT(*) AS total_users,
            ROUND(AVG(charging_sessions), 3) AS avg_sessions_per_user,
            ROUND(AVG(total_kwh), 3) AS avg_kwh_per_user
        FROM user_usage
        """
    )

#统计星期一到星期日的：
# - 充电次数。
# - 总充电量。
# - 平均单次充电量。
# 它先生成完整的七天维度，所以某一天没有数据时，也会输出该天，并用 0 补齐。
def weekday_patterns(df: DataFrame) -> DataFrame:
    """Task #69: return all seven weekdays, filling missing days with zero."""
    _require_dwd(df)
    df.createOrReplaceTempView("_dwd_charging_weekday")
    return df.sparkSession.sql(
        """
        WITH weekday_dimension AS (
            SELECT * FROM VALUES
                (0, 'Mon'), (1, 'Tue'), (2, 'Wed'), (3, 'Thu'),
                (4, 'Fri'), (5, 'Sat'), (6, 'Sun')
            AS weekdays(start_weekday, weekday_name)
        ), weekday_usage AS (
            SELECT
                start_weekday,
                COUNT(DISTINCT session_id) AS charging_sessions,
                ROUND(SUM(kwh_total), 3) AS total_kwh,
                ROUND(AVG(kwh_total), 3) AS avg_kwh_per_session
            FROM _dwd_charging_weekday
            GROUP BY start_weekday
        )
        SELECT
            dimension.start_weekday,
            dimension.weekday_name,
            COALESCE(usage.charging_sessions, 0) AS charging_sessions,
            COALESCE(usage.total_kwh, CAST(0 AS DECIMAL(20, 3))) AS total_kwh,
            COALESCE(usage.avg_kwh_per_session, CAST(0 AS DECIMAL(20, 3)))
                AS avg_kwh_per_session
        FROM weekday_dimension dimension
        LEFT JOIN weekday_usage usage
          ON dimension.start_weekday = usage.start_weekday
        ORDER BY dimension.start_weekday
        """
    )


def holiday_patterns(df: DataFrame) -> DataFrame:
    """Compare holidays, ordinary workdays and non-holiday rest days."""
    required = {"is_holiday", "holiday_name", "is_workday", "is_weekend"}
    missing = sorted(required - set(df.columns))
    if missing:
        raise ValueError("Missing holiday-analysis columns: " + ", ".join(missing))
    _require_dwd(df)
    df.createOrReplaceTempView("_dwd_charging_holiday")
    return df.sparkSession.sql(
        """
        SELECT
            CASE
                WHEN is_holiday = 1 THEN 'holiday'
                WHEN is_workday = 1 THEN 'workday'
                ELSE 'non_holiday_rest_day'
            END AS day_type,
            is_holiday,
            holiday_name,
            is_workday,
            is_weekend,
            COUNT(DISTINCT session_id) AS charging_sessions,
            ROUND(SUM(kwh_total), 3) AS total_kwh,
            ROUND(SUM(charging_fees), 2) AS total_charging_fees,
            ROUND(AVG(kwh_total), 3) AS avg_kwh_per_session,
            ROUND(AVG(charge_time_hrs), 4) AS avg_charge_time_hrs
        FROM _dwd_charging_holiday
        GROUP BY
            CASE
                WHEN is_holiday = 1 THEN 'holiday'
                WHEN is_workday = 1 THEN 'workday'
                ELSE 'non_holiday_rest_day'
            END,
            is_holiday,
            holiday_name,
            is_workday,
            is_weekend
        ORDER BY
            CASE day_type
                WHEN 'holiday' THEN 0
                WHEN 'workday' THEN 1
                ELSE 2
            END,
            holiday_name,
            is_weekend
        """
    )

#生成 0～23 点共 24 行数据，统计每个小时的充电次数。
# 即使某小时没有订单，也会输出该小时，并用 0 补齐。
def hourly_distribution(df: DataFrame) -> DataFrame:
    """Task #74: return 24 hourly counts without collecting on the driver."""
    _require_dwd(df)
    df.createOrReplaceTempView("_dwd_charging_hour")
    return df.sparkSession.sql(
        """
        WITH hours AS (SELECT explode(sequence(0, 23)) AS hour),
        usage AS (
            SELECT start_hour AS hour, COUNT(DISTINCT session_id) AS charging_sessions
            FROM _dwd_charging_hour
            GROUP BY start_hour
        )
        SELECT hours.hour, COALESCE(usage.charging_sessions, 0) AS charging_sessions
        FROM hours LEFT JOIN usage ON hours.hour = usage.hour
        ORDER BY hours.hour
        """
    )
#生成：7个星期 × 24个小时 = 168行
#每行记录某星期、某小时的充电次数。
def weekday_hour_heatmap(df: DataFrame) -> DataFrame:
    """Task #75: return 168 rows for an ECharts weekday/hour heatmap."""
    _require_dwd(df)
    df.createOrReplaceTempView("_dwd_charging_heatmap")
    result = df.sparkSession.sql(
        """
        WITH grid AS (
            SELECT hour, weekday
            FROM (SELECT explode(sequence(0, 23)) AS hour)
            CROSS JOIN (SELECT explode(sequence(0, 6)) AS weekday)
        ), usage AS (
            SELECT
                start_hour AS hour,
                start_weekday AS weekday,
                COUNT(DISTINCT session_id) AS charging_sessions
            FROM _dwd_charging_heatmap
            GROUP BY start_hour, start_weekday
        )
        SELECT
            grid.hour,
            grid.weekday,
            COALESCE(usage.charging_sessions, 0) AS charging_sessions
        FROM grid
        LEFT JOIN usage
          ON grid.hour = usage.hour AND grid.weekday = usage.weekday
        ORDER BY grid.hour, grid.weekday
        """
    )
    return result.withColumn(
        "echarts_value", F.array("hour", "weekday", "charging_sessions")
    )
