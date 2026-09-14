"""Device utilization and operating-health metrics for DWS tables."""

from pyspark.sql import DataFrame, SparkSession


def _require(df: DataFrame, required: set[str], source_name: str) -> None:
    missing = sorted(required - set(df.columns))
    if missing:
        raise ValueError(f"Missing required {source_name} columns: " + ", ".join(missing))


def device_kpis(charging_df: DataFrame, device_df: DataFrame) -> DataFrame:
    """Return one row per device with order volume and time utilization."""
    _require(
        charging_df,
        {"session_id", "device_id", "kwh_total", "charge_time_hrs", "start_date"},
        "charging DWD",
    )
    _require(
        device_df,
        {
            "device_id", "station_id", "device_code", "charger_type",
            "rated_power_kw", "connector_count", "manufacturer", "current_status",
        },
        "device DWD",
    )
    charging_df.createOrReplaceTempView("_dwd_charging_device")
    device_df.createOrReplaceTempView("_dwd_device_dimension")
    return SparkSession.builder.getOrCreate().sql(
        """
        WITH coverage AS (
            SELECT
                GREATEST(DATEDIFF(MAX(start_date), MIN(start_date)) + 1, 1)
                    AS observation_days
            FROM _dwd_charging_device
        ), device_usage AS (
            SELECT
                device_id,
                COUNT(DISTINCT session_id) AS charging_sessions,
                COUNT(DISTINCT start_date) AS charging_days,
                ROUND(SUM(kwh_total), 3) AS total_kwh,
                ROUND(SUM(charge_time_hrs), 3) AS total_charging_hours,
                ROUND(AVG(kwh_total), 3) AS avg_kwh_per_session,
                ROUND(AVG(kwh_total / charge_time_hrs), 3) AS avg_output_power_kw
            FROM _dwd_charging_device
            GROUP BY device_id
        )
        SELECT
            dimension.device_id,
            dimension.station_id,
            dimension.device_code,
            dimension.charger_type,
            dimension.rated_power_kw,
            dimension.connector_count,
            dimension.manufacturer,
            dimension.current_status,
            COALESCE(usage.charging_sessions, 0) AS charging_sessions,
            COALESCE(usage.charging_days, 0) AS charging_days,
            COALESCE(usage.total_kwh, CAST(0 AS DECIMAL(24, 3))) AS total_kwh,
            COALESCE(usage.total_charging_hours, 0.0) AS total_charging_hours,
            usage.avg_kwh_per_session,
            usage.avg_output_power_kw,
            ROUND(
                COALESCE(usage.total_charging_hours, 0.0)
                    / (coverage.observation_days * 24.0),
                6
            ) AS charging_time_utilization_rate,
            coverage.observation_days
        FROM _dwd_device_dimension dimension
        CROSS JOIN coverage
        LEFT JOIN device_usage usage ON usage.device_id = dimension.device_id
        ORDER BY charging_sessions DESC, dimension.device_id
        """
    )


def device_operation_kpis(status_df: DataFrame, device_df: DataFrame) -> DataFrame:
    """Return one row per device summarizing availability, faults and health."""
    _require(
        status_df,
        {
            "device_id", "station_id", "record_time", "online_minutes",
            "offline_minutes", "fault_minutes", "fault_count",
            "successful_sessions", "failed_sessions", "avg_output_power_kw",
            "availability_rate", "health_score", "maintenance_flag",
        },
        "device-status DWD",
    )
    _require(
        device_df,
        {"device_id", "device_code", "charger_type", "rated_power_kw", "current_status"},
        "device DWD",
    )
    status_df.createOrReplaceTempView("_dwd_device_status")
    device_df.createOrReplaceTempView("_dwd_device_operation_dimension")
    return SparkSession.builder.getOrCreate().sql(
        """
        WITH operation AS (
            SELECT
                device_id,
                station_id,
                COUNT(DISTINCT TO_DATE(record_time)) AS observed_days,
                SUM(online_minutes) AS online_minutes,
                SUM(offline_minutes) AS offline_minutes,
                SUM(fault_minutes) AS fault_minutes,
                SUM(fault_count) AS fault_count,
                SUM(successful_sessions) AS successful_sessions,
                SUM(failed_sessions) AS failed_sessions,
                SUM(maintenance_flag) AS maintenance_days,
                ROUND(
                    SUM(online_minutes) / SUM(online_minutes + offline_minutes),
                    6
                ) AS availability_rate,
                ROUND(AVG(health_score), 3) AS avg_health_score,
                ROUND(MIN(health_score), 3) AS min_health_score,
                ROUND(
                    CASE
                        WHEN SUM(successful_sessions + failed_sessions) > 0
                        THEN SUM(
                            avg_output_power_kw
                                * (successful_sessions + failed_sessions)
                        ) / SUM(successful_sessions + failed_sessions)
                    END,
                    3
                ) AS avg_output_power_kw
            FROM _dwd_device_status
            GROUP BY device_id, station_id
        )
        SELECT
            dimension.device_id,
            operation.station_id,
            dimension.device_code,
            dimension.charger_type,
            dimension.rated_power_kw,
            dimension.current_status,
            operation.observed_days,
            operation.online_minutes,
            operation.offline_minutes,
            operation.fault_minutes,
            operation.fault_count,
            operation.successful_sessions,
            operation.failed_sessions,
            operation.maintenance_days,
            operation.availability_rate,
            operation.avg_output_power_kw,
            operation.avg_health_score,
            operation.min_health_score
        FROM _dwd_device_operation_dimension dimension
        LEFT JOIN operation ON operation.device_id = dimension.device_id
        ORDER BY operation.availability_rate ASC NULLS FIRST, dimension.device_id
        """
    )
