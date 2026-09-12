"""
bigdata/api_routes.py — Phase 2 stub
FastAPI route handlers for the Web dashboard (charging/revenue/session
trends, station distribution, filtered stats). See run_api.py for how this
gets mounted.

Owner(s): 洪维斌, 邱辰笙
"""

import pandas as pd


def charging_volume_trend(*args, **kwargs):
    """
    [Task #70] 大数据可视化大屏（Web端） / 数据可视化 / 充电量趋势图
    Owner: 邱辰笙

    使用ECharts绘制不同时间段的充电量变化趋势，展示整体充电需求变化。
    """
    # TODO: implement
    raise NotImplementedError("Task #70: 充电量趋势图")


def revenue_trend(*args, **kwargs):
    """
    [Task #71] 大数据可视化大屏（Web端） / 数据可视化 / 营收趋势图
    Owner: 邱辰笙

    根据charging_fees统计不同时间段的充电费用，并使用ECharts折线图展示营收变化趋势。
    """
    # TODO: implement
    raise NotImplementedError("Task #71: 营收趋势图")


def session_count_trend(*args, **kwargs):
    """
    [Task #72] 大数据可视化大屏（Web端） / 数据可视化 / 充电次数趋势图
    Owner: 洪维斌

    统计不同时间段的充电会话数量，并通过柱状图或折线图展示充电次数变化。
    """
    # TODO: implement
    raise NotImplementedError("Task #72: 充电次数趋势图")


def station_distribution(*args, **kwargs):
    """
    [Task #76] 大数据可视化大屏（Web端） / 数据可视化 / 充电站分布展示
    Owner: 洪维斌

    根据station_name、address及站点信息展示教师数据集中的充电站分布情况。
    """
    # TODO: implement
    raise NotImplementedError("Task #76: 充电站分布展示")


def filtered_stats(*args, **kwargs):
    """
    [Task #81] 大数据可视化大屏（Web端） / 页面交互 / 图表筛选
    Owner: 洪维斌

    支持按照日期、星期、充电站等条件筛选统计数据，并动态更新ECharts图表。
    """
    # TODO: implement
    raise NotImplementedError("Task #81: 图表筛选")


def register_stats_routes(app):
    """
    [Task #107] 智能推荐与系统集成 / 后端接口 / 数据接口
    Owner: 邱辰笙

    建立后端接口，为Web大屏提供充电量、费用、站点统计、电池分析等数据。

    Wires up GET endpoints backed by the functions above (and by
    analytics/charging_stats.py, analytics/battery_stats.py,
    analytics/station_ranking.py). Each handler currently returns 501 until
    its underlying function is implemented — replace the try/except once
    the corresponding TODO above is done.
    """

    @app.get("/api/stats/charging-volume-trend")
    def _charging_volume_trend(days: int = 7):
        try:
            return charging_volume_trend(days=days)
        except NotImplementedError as e:
            return {"error": str(e)}, 501

    @app.get("/api/stats/revenue-trend")
    def _revenue_trend(days: int = 7):
        try:
            return revenue_trend(days=days)
        except NotImplementedError as e:
            return {"error": str(e)}, 501

    @app.get("/api/stats/session-count-trend")
    def _session_count_trend(days: int = 7):
        try:
            return session_count_trend(days=days)
        except NotImplementedError as e:
            return {"error": str(e)}, 501

    @app.get("/api/stats/station-distribution")
    def _station_distribution():
        try:
            return station_distribution()
        except NotImplementedError as e:
            return {"error": str(e)}, 501

    @app.get("/api/stats/filtered")
    def _filtered_stats(date: str = None, weekday: str = None, station_id: str = None):
        try:
            return filtered_stats(date=date, weekday=weekday, station_id=station_id)
        except NotImplementedError as e:
            return {"error": str(e)}, 501


