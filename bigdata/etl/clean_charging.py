"""
Clean Charging — Phase 2

Owner(s): 洪维斌
"""

import pandas as pd
import numpy as np

# 星期缩写标准化映射：数据集里 weekday 列用的是 Mon/Tue/Wed/Thu/Fri/Sat/Sun，
# 但 Mon~Sun 那7个one-hot列用的是 Mon/Tues/Wed/Thurs/Fri/Sat/Sun（Tues/Thurs拼法不同），
# 这里统一成数字0-6（0=周一），后续按小时/星期分析都用这个数字版本，避免字符串拼法不一致导致分组出错。
_WEEKDAY_TO_NUM = {"Mon": 0, "Tue": 1, "Wed": 2, "Thu": 3, "Fri": 4, "Sat": 5, "Sun": 6}


def _safe_timestamp(value):
    """
    pd.to_datetime() 在 pandas 2.3.3 上对 "0014-11-18 15:40:26" 这种年份很小的字符串会
    莫名返回 NaT（已实测确认是这个版本的解析怪癖），但 pd.Timestamp() 对同一个字符串可以
    正常解析。这里逐个转换，格式错误的值返回 NaT，效果等同于 errors="coerce"，只是绕开了那个bug。
    """
    try:
        return pd.Timestamp(value)
    except (ValueError, TypeError):
        return pd.NaT


def process_charging_time(df: pd.DataFrame) -> pd.DataFrame:
    """
    [Task #64] 大数据可视化大屏（Web端） / 数据预处理 / 充电时间处理
    Owner: 洪维斌

    对nvv2t.csv中的startTime、endTime、chargeTimeHrs等字段进行转换和规范化，提取小时、星期等时间特征。

    实测数据说明（与最初假设不同，已按真实教师数据集调整）：
        - startTime / endTime 在原始数据里就是 0-23 的整数小时，不是完整时间戳，直接使用即可
        - 真正的完整时间戳在 created / ended 两列（形如 "0014-11-18 15:40:26"，年份是脱敏过的占位值）
        - weekday 列已提供 Mon/Tue/Wed/Thu/Fri/Sat/Sun，本函数转成 0-6 数字版本方便分组统计

    输出: 新增以下列
        - created / ended: 转为 pandas datetime 类型
        - chargeTimeHrs: 转为数值类型（防止读入时被当成字符串）
        - start_hour: 直接取用原始 startTime 列（已经是0-23小时）
        - start_weekday: 由 weekday 列映射成的 0-6 数字（0=周一），无法识别的值变NaN
        - start_date: 从 created 提取的日期（不含时间），供按天聚合使用
    """
    df = df.copy()

    df["created"] = df["created"].apply(_safe_timestamp)
    df["ended"] = df["ended"].apply(_safe_timestamp)
    df["chargeTimeHrs"] = pd.to_numeric(df["chargeTimeHrs"], errors="coerce")

    # startTime本身就是小时数字，直接转数值类型即可，不需要再从时间戳里提取
    df["start_hour"] = pd.to_numeric(df["startTime"], errors="coerce")
    df["start_weekday"] = df["weekday"].map(_WEEKDAY_TO_NUM)
    # 注意：这里不能用 df["created"].dt.date —— 因为 created 年份（如"0014"）超出了
    # pandas datetime64[ns] 能表示的范围（约1677~2262年），会导致.dt访问器报错。
    # created 列保留为 Timestamp 对象（不是datetime64类型），用 apply 逐个取 .date() 即可。
    df["start_date"] = df["created"].apply(lambda x: x.date() if pd.notna(x) else None)

    return df


def clean_charging_data(df: pd.DataFrame) -> pd.DataFrame:
    """
    [Task #65] 大数据可视化大屏（Web端） / 数据预处理 / 充电数据清洗
    Owner: 洪维斌

    对充电量、费用、充电时长等字段进行缺失值、重复值和异常值检查，保证可视化数据质量。

    输入: df 通常是先经过 process_charging_time() 处理后的DataFrame
    输出: 清洗后的DataFrame，同时在控制台打印一份清洗报告，方便答辩时展示"数据质量保证"这一步

    清洗规则（按真实列名 kwhTotal / charging_fees 调整）：
        1. 缺失值：created/ended/kwhTotal/charging_fees 任一为空的行直接剔除
        2. 重复值：完全重复的行（同一充电session被记录两次）只保留一条
        3. 异常值：
           - kwhTotal（充电电量）或 charging_fees（费用）为负数 → 剔除（物理上不可能为负）
           - chargeTimeHrs 大于 24 小时 → 剔除（充电一次超过24小时基本是异常记录）
    """
    df = df.copy()
    report = {}
    report["原始行数"] = len(df)

    core_cols = [c for c in ["created", "ended", "kwhTotal", "charging_fees"] if c in df.columns]
    df = df.dropna(subset=core_cols)
    report["剔除缺失值后"] = len(df)

    df = df.drop_duplicates()
    report["剔除重复行后"] = len(df)

    if "kwhTotal" in df.columns:
        df = df[df["kwhTotal"] >= 0]
    if "charging_fees" in df.columns:
        df = df[df["charging_fees"] >= 0]
    if "chargeTimeHrs" in df.columns:
        df = df[df["chargeTimeHrs"] <= 24]
    report["剔除异常值后"] = len(df)

    print("=== 数据清洗报告（Task #65）===")
    for step, count in report.items():
        print(f"{step}: {count} 行")
    print(f"共剔除 {report['原始行数'] - len(df)} 行问题数据"
          f"（{(report['原始行数']-len(df))/max(report['原始行数'],1)*100:.1f}%）")

    return df




