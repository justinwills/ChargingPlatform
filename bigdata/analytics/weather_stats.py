"""Weather exposure and charging-demand metrics."""

from pyspark.sql import DataFrame, SparkSession


def _require(df: DataFrame, required: set[str], source_name: str) -> None:
    missing = sorted(required - set(df.columns))
    if missing:
        raise ValueError(f"Missing required {source_name} columns: " + ", ".join(missing))


def weather_impact_kpis(
    charging_df: DataFrame,
    weather_df: DataFrame,
    station_df: DataFrame,
) -> DataFrame:
    """Compare demand by weather using station-hours as the exposure base."""
    _require(
        charging_df,
        {"session_id", "weather_id", "kwh_total", "charging_fees", "charge_time_hrs"},
        "charging DWD",
    )
    _require(
        weather_df,
        {
            "weather_id", "location_id", "weather_type", "weather_name",
            "temperature_c", "humidity_pct", "precipitation_mm",
            "wind_speed_mps", "visibility_km", "is_rain",
        },
        "weather DWD",
    )
    _require(station_df, {"station_id", "location_id"}, "station DWD")
    charging_df.createOrReplaceTempView("_dwd_charging_weather")
    weather_df.createOrReplaceTempView("_dwd_weather_hourly")
    station_df.createOrReplaceTempView("_dwd_weather_station")
    return SparkSession.builder.getOrCreate().sql(
        """
        WITH weather_exposure AS (
            SELECT
                weather.weather_type,
                weather.weather_name,
                weather.is_rain,
                COUNT(*) AS station_hours,
                ROUND(AVG(weather.temperature_c), 3) AS avg_temperature_c,
                ROUND(AVG(weather.humidity_pct), 3) AS avg_humidity_pct,
                ROUND(AVG(weather.precipitation_mm), 3) AS avg_precipitation_mm,
                ROUND(AVG(weather.wind_speed_mps), 3) AS avg_wind_speed_mps,
                ROUND(AVG(weather.visibility_km), 3) AS avg_visibility_km
            FROM _dwd_weather_hourly weather
            INNER JOIN _dwd_weather_station station
                ON station.location_id = weather.location_id
            GROUP BY weather.weather_type, weather.weather_name, weather.is_rain
        ), weather_usage AS (
            SELECT
                weather.weather_type,
                weather.weather_name,
                weather.is_rain,
                COUNT(DISTINCT charging.session_id) AS charging_sessions,
                ROUND(SUM(charging.kwh_total), 3) AS total_kwh,
                ROUND(SUM(charging.charging_fees), 2) AS total_charging_fees,
                ROUND(AVG(charging.kwh_total), 3) AS avg_kwh_per_session,
                ROUND(AVG(charging.charge_time_hrs), 4) AS avg_charge_time_hrs
            FROM _dwd_charging_weather charging
            INNER JOIN _dwd_weather_hourly weather
                ON weather.weather_id = charging.weather_id
            GROUP BY weather.weather_type, weather.weather_name, weather.is_rain
        )
        SELECT
            exposure.weather_type,
            exposure.weather_name,
            exposure.is_rain,
            exposure.station_hours,
            COALESCE(usage.charging_sessions, 0) AS charging_sessions,
            ROUND(
                COALESCE(usage.charging_sessions, 0) * 1000.0
                    / exposure.station_hours,
                3
            ) AS sessions_per_1000_station_hours,
            COALESCE(usage.total_kwh, CAST(0 AS DECIMAL(24, 3))) AS total_kwh,
            COALESCE(usage.total_charging_fees, CAST(0 AS DECIMAL(24, 2)))
                AS total_charging_fees,
            usage.avg_kwh_per_session,
            usage.avg_charge_time_hrs,
            exposure.avg_temperature_c,
            exposure.avg_humidity_pct,
            exposure.avg_precipitation_mm,
            exposure.avg_wind_speed_mps,
            exposure.avg_visibility_km
        FROM weather_exposure exposure
        LEFT JOIN weather_usage usage
            ON usage.weather_type = exposure.weather_type
            AND usage.weather_name = exposure.weather_name
            AND usage.is_rain = exposure.is_rain
        ORDER BY sessions_per_1000_station_hours DESC, exposure.weather_type
        """
    )
