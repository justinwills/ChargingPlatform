"""
Charging Stats — Phase 2 stub

Owner(s): 洪维斌, 邱辰笙
"""

import pandas as pd


def overall_charging_kpis(*args, **kwargs):
    """
    [Task #66] 大数据可视化大屏（Web端） / 数据统计 / 充电业务指标
    Owner: 邱辰笙

    统计总充电次数、总充电量、总充电费用、平均充电时长、平均单次充电量等运营指标。
    """
    # TODO: implement
    raise NotImplementedError("Task #66: 充电业务指标")


def station_kpis(*args, **kwargs):
    """
    [Task #67] 大数据可视化大屏（Web端） / 数据统计 / 充电站指标
    Owner: 邱辰笙

    根据stationId统计各充电站的充电次数、充电量、充电费用及设备数量，并进行站点之间的运营情况对比。
    """
    # TODO: implement
    raise NotImplementedError("Task #67: 充电站指标")


def user_kpis(*args, **kwargs):
    """
    [Task #68] 大数据可视化大屏（Web端） / 数据统计 / 用户指标
    Owner: 邱辰笙

    根据userId统计用户数量及用户充电行为，包括充电次数、累计充电量及平均单次充电量等。
    """
    # TODO: implement
    raise NotImplementedError("Task #68: 用户指标")


def weekday_patterns(*args, **kwargs):
    """
    [Task #69] 大数据可视化大屏（Web端） / 数据统计 / 星期充电规律
    Owner: 邱辰笙

    根据weekday及Mon~Sun字段分析不同星期的充电次数和充电量变化情况。
    """
    # TODO: implement
    raise NotImplementedError("Task #69: 星期充电规律")


def hourly_distribution(df: pd.DataFrame) -> list:
    """
    [Task #74] 大数据可视化大屏（Web端） / 数据可视化 / 充电时段分析
    Owner: 洪维斌

    根据startTime分析不同小时的充电需求，绘制24小时充电分布图，识别高频充电时段。

    输入: df 需已经过 etl/clean_charging.py 的 process_charging_time() 处理（含 start_hour 列）
    输出: 长度为24的列表，形如 [{"hour": 0, "count": 12}, {"hour": 1, "count": 5}, ...]
          缺少数据的小时会补0，保证前端ECharts画24小时柱状图时横轴不缺格子。
    """
    counts = df.groupby("start_hour").size()
    result = [{"hour": h, "count": int(counts.get(h, 0))} for h in range(24)]
    return result


def weekday_hour_heatmap(df: pd.DataFrame) -> list:
    """
    [Task #75] 大数据可视化大屏（Web端） / 数据可视化 / 星期热力图
    Owner: 洪维斌

    根据星期和小时统计充电次数或充电量，使用ECharts热力图展示不同日期和时间的充电活跃程度。

    输入: df 需已经过 process_charging_time() 处理（含 start_weekday, start_hour 列）
    输出: ECharts heatmap 组件要求的 [x轴索引, y轴索引, 值] 三元组列表，
          形如 [[0, 8, 3], [0, 9, 5], ...]，第一个数字是小时(0-23)，第二个数字是星期(0=周一...6=周日)，
          第三个数字是该组合下的充电次数。没有数据的格子输出0，避免前端热力图渲染缺格。
    """
    counts = df.groupby(["start_hour", "start_weekday"]).size()
    result = []
    for hour in range(24):
        for weekday in range(7):
            value = int(counts.get((hour, weekday), 0))
            result.append([hour, weekday, value])
    return result


