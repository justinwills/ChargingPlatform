# PySpark 数据仓库

本目录实现充电业务的 ODS → DWD → DWS → ADS 流程，目标运行环境为
Spark 3.3.4、Python 3.10 和 Hadoop 3.x。

## 分层

- ODS：按全字符串固定 Schema 读取原始 CSV，保留异常原值和坏行。
- DWD：转换类型、标准化字段、标记并隔离缺失、重复、越界和关联异常。
- DWS：使用 SparkSQL 生成充电站、用户和星期汇总。
- ADS：生成总体充电 KPI 和数据质量统计。

## 运行

在仓库根目录通过 `spark-submit` 执行：

```bash
spark-submit \
  --master local[2] \
  bigdata/charging_warehouse.py \
  --charging-input hdfs:///charging/source/nvv2t.csv \
  --station-input hdfs:///charging/source/nvv2t_md_end.csv \
  --warehouse-root hdfs:///charging/warehouse
```

默认覆盖对应分层目录，也可以使用 `--mode error` 防止覆盖已有结果。当前数据没有
批次字段，禁止直接追加，否则重复执行会造成指标重复累计。

运行最小回归测试：

```bash
spark-submit --master local[2] bigdata/tests/test_charging_pipeline.py
```

若只执行 Task #61~#63 的三份数据导入与基础清洗，可运行：

```bash
spark-submit \
  --master local[2] \
  bigdata/etl/load_data.py \
  --data-root hdfs:///charging-platform/data \
  --format parquet
```

输入文件应位于 `<data-root>/raw/`，输出写入 `<data-root>/processed/`。
`--format csv` 会生成含多个 `part-*.csv` 的 Hadoop 目录；演示用小数据若需要
单个数据分片，可增加 `--partitions 1`。

当前教师数据的 `created`、`ended` 和电池 `record_time` 使用
`日/月/年 时:分`（例如 `18/11/2014 17:11`）格式。两个任务入口按该格式解析，
并将 Parquet 的 INT96 和日期写入模式设为 `CORRECTED`。

## 主要输出

```text
warehouse/ods/charging_sessions
warehouse/ods/stations
warehouse/dwd/charging_sessions
warehouse/dwd/charging_rejects
warehouse/dwd/stations
warehouse/dws/station_kpis
warehouse/dws/user_kpis
warehouse/dws/weekday_patterns
warehouse/ads/overall_charging_kpis
warehouse/ads/user_summary_kpis
warehouse/ads/data_quality
```

## 导出 HTTP 接口数据

数仓运行完成后，将现有 ADS/DWS 表整理为一个 JSON 快照：

```bash
spark-submit \
  --master 'local[2]' \
  bigdata/export_api_snapshot.py \
  --warehouse-root hdfs:///user/$USER/charging-platform/warehouse \
  --output dashboard/data/bigdata.json
```

开发环境启动 `ChargingServer` 前，让它直接读取源码目录中的最新快照：

```bash
export DASHBOARD_ROOT="$(pwd)/dashboard"
```

接口地址为 `GET http://服务器地址:8080/api/bigdata`。响应一次返回现有的总体
指标、用户汇总、站点指标、用户指标、星期规律和数据质量结果。每次重跑数仓后，
再执行一次快照导出命令即可刷新接口数据。
