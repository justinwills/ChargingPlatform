# ChargingPlatform 大数据处理与接口说明

## 1. 功能说明

系统使用 Hadoop HDFS 保存原始 CSV 和处理结果，使用 PySpark 清洗数据并计算
以下指标：

- 充电业务指标
- 充电站指标
- 用户指标
- 星期充电规律
- 设备充电与利用率指标
- 节假日充电规律
- 天气对充电需求的影响
- 设备可用率、故障与健康指标

处理完成后，指标会输出到：

```text
dashboard/data/bigdata.json
```

Qt 后端通过 HTTP 接口返回这个文件的内容，前端可以直接使用接口数据绘制图表。

## 2. 运行环境

推荐环境：

```text
Linux
Java 11
Python 3.10
Hadoop 3.x
Spark 3.3.4
Qt 5 或 Qt 6
CMake 3.16+
```

检查环境：

```bash
java -version
python3 --version
hadoop version
spark-submit --version
cmake --version
```

## 3. CSV 文件放在哪里

需要准备以下六个文件（文件名必须保持一致）：

```text
users.csv
stations.csv
devices.csv
weather_hourly.csv
charging_orders.csv
device_status_log.csv
```

先放到 Linux 项目的本地目录：

```text
test-data/raw/users.csv
test-data/raw/stations.csv
test-data/raw/devices.csv
test-data/raw/weather_hourly.csv
test-data/raw/charging_orders.csv
test-data/raw/device_status_log.csv
```

例如：

```bash
mkdir -p test-data/raw

cp /数据文件位置/{users,stations,devices,weather_hourly,charging_orders,device_status_log}.csv test-data/raw/
```

然后上传到 HDFS：

```text
/user/<Linux用户名>/charging-platform/data/raw/
```

## 4. 启动 Hadoop

启动 HDFS：

```bash
start-dfs.sh
```

检查 Hadoop 进程：

```bash
jps
```

应当看到：

```text
NameNode
DataNode
SecondaryNameNode
```

检查 HDFS：

```bash
hdfs dfs -ls /
```

当前 Spark 使用 `local[2]` 模式，所以不要求启动 YARN。

## 5. 上传 CSV 到 HDFS

设置路径：

```bash
HDFS_DATA_PATH="/user/${USER}/charging-platform/data"
HDFS_DATA_URI="hdfs:///user/${USER}/charging-platform/data"
```

创建目录：

```bash
hdfs dfs -mkdir -p "${HDFS_DATA_PATH}/raw"
```

上传六个文件：

```bash
hdfs dfs -put -f \
  test-data/raw/users.csv \
  test-data/raw/stations.csv \
  test-data/raw/devices.csv \
  test-data/raw/weather_hourly.csv \
  test-data/raw/charging_orders.csv \
  test-data/raw/device_status_log.csv \
  "${HDFS_DATA_PATH}/raw/"
```

检查文件：

```bash
hdfs dfs -ls -h "${HDFS_DATA_PATH}/raw"
```

## 6. 使用 Spark 处理六张 CSV

运行数据导入和清洗：

```bash
spark-submit \
  --master 'local[2]' \
  bigdata/etl/load_data.py \
  --data-root "${HDFS_DATA_URI}" \
  --format parquet \
  --mode overwrite
```

如果六张源文件还在本地同一个目录，也可以直接指定输入目录，并把清洗结果写到
HDFS：

```bash
spark-submit \
  --master 'local[2]' \
  bigdata/etl/load_data.py \
  --input-root "file:///项目绝对路径/outputs/ml_training_six_tables" \
  --output-root "${HDFS_DATA_URI}/processed" \
  --format parquet \
  --mode overwrite
```

处理结果保存到：

```text
/user/<Linux用户名>/charging-platform/data/processed/
```

检查结果：

```bash
hdfs dfs -ls -R "${HDFS_DATA_PATH}/processed"
```

其中包括：

```text
charging_orders.parquet
users.parquet
stations.parquet
devices.parquet
weather_hourly.parquet
device_status_log.parquet
```

## 7. 计算充电指标

运行 Spark 数仓任务：

```bash
HDFS_WAREHOUSE_URI="hdfs:///user/${USER}/charging-platform/warehouse"

spark-submit \
  --master 'local[2]' \
  bigdata/charging_warehouse.py \
  --input-root "${HDFS_DATA_URI}/raw" \
  --warehouse-root "${HDFS_WAREHOUSE_URI}" \
  --mode overwrite
```

这个任务负责计算：

- 总充电次数、总充电量和总费用
- 平均充电时长和平均单次充电量
- 每个充电站的充电指标
- 每个用户的充电指标
- 星期一至星期日的充电规律
- 每台设备的充电量、输出功率和充电时间利用率
- 节假日、工作日和非节假日休息日的需求差异
- 按天气类型计算充电需求和每千站点小时会话数
- 每台设备的可用率、故障、维护、输出功率和健康评分

六张源表都会写入 ODS 和 DWD。订单清洗还会检查用户、站点、设备、天气外键，
以及设备所属站点、天气位置和订单开始小时是否一致；现有大屏指标仍从清洗后的
订单事实表和站点维表计算。

新增指标口径：

- 站点对比同时输出 `sessions_per_device`、`kwh_per_device` 和
  `charging_fees_per_device`，设备数量为零时这些值保持为空。
- 设备 `charging_time_utilization_rate` 为该设备累计充电小时数除以整个订单
  观察期小时数。
- 节假日 `day_type` 是互斥分类：`holiday`、`workday`、
  `non_holiday_rest_day`；同时保留原始节假日和周末标记。
- 天气需求率 `sessions_per_1000_station_hours` 使用所有站点天气小时作为分母，
  因而会保留没有订单的天气暴露时间。
- 设备运行可用率使用累计在线分钟除以累计记录分钟；平均输出功率按状态日志中
  的会话数量加权。

## 8. 输出 bigdata.json

运行：

```bash
spark-submit \
  --master 'local[2]' \
  bigdata/export_api_snapshot.py \
  --warehouse-root "${HDFS_WAREHOUSE_URI}" \
  --output dashboard/data/bigdata.json
```

检查文件：

```bash
ls -lh dashboard/data/bigdata.json
```

查看内容：

```bash
python3 -m json.tool dashboard/data/bigdata.json | head -n 80
```

所有接口指标都从这个文件读取。

## 9. 后端接口在哪里

接口代码位于：

```text
server/httpdashboard.cpp
```

接口地址：

```text
GET /api/bigdata
```

完整地址：

```text
http://服务器IP:8080/api/bigdata
```

接口中的指标位置：

| 指标 | JSON 位置 |
|---|---|
| 充电业务指标 | `data.overview` |
| 充电站指标 | `data.stations` |
| 用户总体指标 | `data.userSummary` |
| 每个用户的指标 | `data.users` |
| 每台设备的充电与利用率指标 | `data.devices` |
| 星期充电规律 | `data.weekdays` |
| 节假日充电规律 | `data.holidays` |
| 天气影响分析 | `data.weatherImpact` |
| 设备运行指标 | `data.deviceOperations` |
| 数据质量统计 | `data.quality` |

## 10. 编译并启动后端(其实就是在QT运行server)

编译：

```bash
cmake \
  -S . \
  -B build-linux \
  -DCMAKE_BUILD_TYPE=Release

cmake \
  --build build-linux \
  --target ChargingServer \
  --parallel "$(nproc)"
```

指定大屏数据目录：

```bash
export DASHBOARD_ROOT="$(pwd)/dashboard"
```

启动后端：

```bash
./build-linux/server/ChargingServer
```

正常情况下会显示：

```text
Charging server listening on port 8888
Dashboard API listening on port 8080
```

## 11. 调用接口

使用浏览器：

```text
http://127.0.0.1:8080/api/bigdata
```

使用 curl：

```bash
curl -s http://127.0.0.1:8080/api/bigdata \
  | python3 -m json.tool
```

JavaScript 调用：

```javascript
fetch("http://服务器IP:8080/api/bigdata")
  .then(response => response.json())
  .then(result => {
    console.log("充电业务指标", result.data.overview);
    console.log("充电站指标", result.data.stations);
    console.log("用户指标", result.data.users);
    console.log("设备指标", result.data.devices);
    console.log("星期规律", result.data.weekdays);
    console.log("节假日规律", result.data.holidays);
    console.log("天气影响", result.data.weatherImpact);
    console.log("设备运行指标", result.data.deviceOperations);
  });
```

## 12. 更新数据

更换 CSV 后依次执行：

1. 使用 `hdfs dfs -put -f` 覆盖 HDFS `raw/` 中同名的六张 CSV。
2. 重新运行 `load_data.py`。
3. 重新运行 `charging_warehouse.py`。
4. 重新运行 `export_api_snapshot.py`。

只更新数据时不需要重新编译或重启 `ChargingServer`，因为接口每次请求都会重新
读取最新的 `bigdata.json`。
