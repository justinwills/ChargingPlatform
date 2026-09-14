"""
Battery Stats — Phase 2

Owner(s): 邱辰笙 (Task #77, #78), 洪维斌 (Task #79, #80)
"""

from pyspark.sql import DataFrame, SparkSession, functions as F

REQUIRED_BATTERY_COLUMNS = {
    "esd",
    "record_time",
    "pack_voltage",
    "charge_current",
    "available_energy",
    "available_capacity",
}


def _require_battery(df: DataFrame) -> None:
    missing = sorted(REQUIRED_BATTERY_COLUMNS - set(df.columns))
    if missing:
        raise ValueError("Missing required battery columns: " + ", ".join(missing))


def soc_trend(*args, **kwargs):
    """
    [Task #77] 大数据可视化大屏（Web端） / 电池数据分析 / SOC分析
    Owner: 邱辰笙

    根据dsv13r2.csv中的SOC数据分析车辆充电过程中电池荷电状态的变化，并通过折线图进行展示。
    """
    # TODO: implement
    raise NotImplementedError("Task #77: SOC分析")


def temperature_analysis(*args, **kwargs):
    """
    [Task #78] 大数据可视化大屏（Web端） / 电池数据分析 / 温度分析
    Owner: 邱辰笙

    根据最大/最小温度数据分析充电过程中的电池温度变化及异常情况。
    """
    # TODO: implement
    raise NotImplementedError("Task #78: 温度分析")


def voltage_current_analysis(*args, **kwargs):
    """
    [Task #79] 大数据可视化大屏（Web端） / 电池数据分析 / 电压电流分析
    Owner: 邱辰笙

    对电池组电压和充电电流进行统计分析，展示充电过程中电压、电流变化趋势。
    """
    # TODO: implement
    raise NotImplementedError("Task #79: 电压电流分析")


def energy_capacity_analysis(*args, **kwargs):
    """
    [Task #80] 大数据可视化大屏（Web端） / 电池数据分析 / 电池能量分析
    Owner: 邱辰笙

    根据available_energy及available_capacity分析车辆可用能量和容量变化情况。
    """
    # TODO: implement
    raise NotImplementedError("Task #80: 电池能量分析")



def voltage_current_analysis(df: DataFrame) -> DataFrame:
    """Task #79: pack voltage and charge current trend during charging.

    Owner: 洪维斌 (需求矩阵 official assignment; overrides stale docstring
    ownership at the top of this file, which predates the final task split)

    Returns one row per telemetry reading, ordered chronologically, so the
    front end can plot pack_voltage and charge_current as time-series lines.
    """
    _require_battery(df)
    df.createOrReplaceTempView("_battery_voltage_current")
    return SparkSession.builder.getOrCreate().sql(
        """
        SELECT
            esd,
            record_time,
            pack_voltage,
            charge_current,
            max_cell_voltage,
            min_cell_voltage
        FROM _battery_voltage_current
        ORDER BY esd, record_time
        """
    )


def energy_capacity_analysis(df: DataFrame) -> DataFrame:
    """Task #80: available energy and capacity trend during charging.

    Owner: 洪维斌

    Returns one row per telemetry reading, ordered chronologically, plus an
    ADS-level summary row (min/max/avg) appended via a second query so the
    dashboard can show both the trend line and headline stats without a
    second round trip.
    """
    _require_battery(df)
    df.createOrReplaceTempView("_battery_energy_capacity")
    return SparkSession.builder.getOrCreate().sql(
        """
        SELECT
            esd,
            record_time,
            available_energy,
            available_capacity,
            soc
        FROM _battery_energy_capacity
        ORDER BY esd, record_time
        """
    )


def energy_capacity_summary(df: DataFrame) -> DataFrame:
    """Task #80 (ADS): headline min/max/avg stats for energy and capacity.

    Owner: 洪维斌
    """
    _require_battery(df)
    df.createOrReplaceTempView("_battery_energy_summary")
    return SparkSession.builder.getOrCreate().sql(
        """
        SELECT
            COUNT(*) AS reading_count,
            ROUND(MIN(available_energy), 2) AS min_available_energy,
            ROUND(MAX(available_energy), 2) AS max_available_energy,
            ROUND(AVG(available_energy), 2) AS avg_available_energy,
            ROUND(MIN(available_capacity), 2) AS min_available_capacity,
            ROUND(MAX(available_capacity), 2) AS max_available_capacity,
            ROUND(AVG(available_capacity), 2) AS avg_available_capacity
        FROM _battery_energy_summary
        """
    )
