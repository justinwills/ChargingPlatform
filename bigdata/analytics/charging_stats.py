"""
Charging statistics task scaffold.

Owners: 邱辰笙, 洪维斌
"""

import pandas as pd


def overall_charging_kpis(*args, **kwargs):
    """
    [Task #72] 大数据可视化大屏（Web端）/ 数据统计 / 充电业务指标
    Owner: 邱辰笙

    统计总充电次数、总充电量、总充电费用、平均充电时长及平均单次充电量等运营指标。
    """
    # TODO: implement
    raise NotImplementedError("Task #72: 充电业务指标")


def station_kpis(*args, **kwargs):
    """
    [Task #73] 大数据可视化大屏（Web端）/ 数据统计 / 充电站指标
    Owner: 邱辰笙

    根据 station_id 统计各充电站的充电次数、充电量、充电费用，并结合
    stations.csv 中的设备数量进行站点运营情况对比。
    """
    # TODO: implement
    raise NotImplementedError("Task #73: 充电站指标")


def user_kpis(*args, **kwargs):
    """
    [Task #74] 大数据可视化大屏（Web端）/ 数据统计 / 用户指标
    Owner: 邱辰笙

    根据 user_id 统计用户数量、充电次数、累计充电量及平均单次充电量等用户充电行为指标。
    """
    # TODO: implement
    raise NotImplementedError("Task #74: 用户指标")


def device_kpis(*args, **kwargs):
    """
    [Task #75] 大数据可视化大屏（Web端）/ 数据统计 / 设备指标
    Owner: 邱辰笙

    根据 device_id 统计各充电桩的充电次数、充电量、平均输出功率及运行状态，
    并分析设备利用情况。
    """
    # TODO: implement
    raise NotImplementedError("Task #75: 设备指标")


def weekday_patterns(*args, **kwargs):
    """
    [Task #76] 大数据可视化大屏（Web端）/ 数据统计 / 星期充电规律
    Owner: 邱辰笙

    根据 weekday 及 Mon~Sun 字段分析不同星期的充电次数和充电量变化情况。
    """
    # TODO: implement
    raise NotImplementedError("Task #76: 星期充电规律")


def holiday_patterns(*args, **kwargs):
    """
    [Task #77] 大数据可视化大屏（Web端）/ 数据统计 / 节假日充电规律
    Owner: 邱辰笙

    根据 is_holiday、holiday_name 和 is_workday 分析节假日、工作日与非节假日
    之间的充电需求差异。
    """
    # TODO: implement
    raise NotImplementedError("Task #77: 节假日充电规律")


def weather_impact(*args, **kwargs):
    """
    [Task #78] 大数据可视化大屏（Web端）/ 数据统计 / 天气影响分析
    Owner: 邱辰笙

    结合 weather_hourly.csv 中的天气类型、温度、降水量、湿度及降雨状态，
    分析天气因素对充电需求的影响。
    """
    # TODO: implement
    raise NotImplementedError("Task #78: 天气影响分析")


def device_runtime_metrics(*args, **kwargs):
    """
    [Task #79] 大数据可视化大屏（Web端）/ 数据统计 / 设备运行指标
    Owner: 邱辰笙

    根据 device_status_log.csv 统计设备可用率、故障次数、平均输出功率及健康评分，
    分析充电设备运行情况。
    """
    # TODO: implement
    raise NotImplementedError("Task #79: 设备运行指标")


def hourly_distribution(*args, **kwargs):
    """
    [Task #84] 大数据可视化大屏（Web端）/ 数据可视化 / 充电时段分析
    Owner: 洪维斌

    根据 start_hour 分析不同小时的充电需求，绘制24小时充电分布图，
    识别高频充电时段。
    """
    # TODO: implement
    raise NotImplementedError("Task #84: 充电时段分析")


def weekday_hour_heatmap(*args, **kwargs):
    """
    [Task #85] 大数据可视化大屏（Web端）/ 数据可视化 / 星期热力图
    Owner: 洪维斌

    根据星期和小时统计充电次数或充电量，使用 ECharts 热力图展示不同日期和时间
    的充电活跃程度。
    """
    # TODO: implement
    raise NotImplementedError("Task #85: 星期热力图")


def holiday_comparison(*args, **kwargs):
    """
    [Task #86] 大数据可视化大屏（Web端）/ 数据可视化 / 节假日对比图
    Owner: 洪维斌

    根据 is_holiday、holiday_name 和 is_workday 对比工作日、周末及节假日的充电需求。
    """
    # TODO: implement
    raise NotImplementedError("Task #86: 节假日对比图")


def weather_impact_chart(*args, **kwargs):
    """
    [Task #87] 大数据可视化大屏（Web端）/ 数据可视化 / 天气影响图
    Owner: 洪维斌

    根据天气类型、温度、降水量及降雨状态，对不同天气条件下的充电次数和充电量
    进行可视化分析。
    """
    # TODO: implement
    raise NotImplementedError("Task #87: 天气影响图")


def station_distribution(*args, **kwargs):
    """
    [Task #88] 大数据可视化大屏（Web端）/ 数据可视化 / 充电站分布展示
    Owner: 洪维斌

    根据 station_name、address、latitude 和 longitude 展示充电站地理分布情况。
    """
    # TODO: implement
    raise NotImplementedError("Task #88: 充电站分布展示")


def device_status_chart(*args, **kwargs):
    """
    [Task #89] 大数据可视化大屏（Web端）/ 数据可视化 / 设备运行状态图
    Owner: 洪维斌

    根据设备状态日志展示设备在线率、故障情况、健康评分及平均输出功率等运行指标。
    """
    # TODO: implement
    raise NotImplementedError("Task #89: 设备运行状态图")
