# ChargingPlatform

## 目录结构

- `db/` - 共用的SQLite数据库层与表结构。
- `protocol/` - TCP消息的JSON帧编解码。
- `server/` - 监听器、客户端线程、请求分发器，以及HTTP大屏接口。
- `client/` - 客户端的TCP连接封装。
- `dashboard/` - 大数据可视化大屏的静态网页。
- `tests/` - 独立的Qt控制台测试项目。

## 源码结构

根目录下的 `db/`、`protocol/`、`server/`、`client/` 是根 `CMakeLists.txt`
实际使用的构建目标。客户端的管理员页面在 `client/admin/` 下，作为
`ChargingClient` 的一部分构建。开发时使用根目录下的这些目标；那个未使用的
镜像目录不参与构建。

构建生成的文件放在 `build/` 目录下，本地的SQLite运行时文件和测试文件已被
Git忽略。

## 测试项目

- `tests/DatabaseTest.pro` 运行纯数据库层的测试驱动程序。
- `tests/ProtocolTest.pro` 在本地起一对测试用的服务器/客户端，检查JSON
  action是否正常工作。

在Qt Creator里打开需要的`.pro`文件，配置好项目后直接运行即可。

## 大数据可视化大屏

`ChargingServer` 启动时还会额外起一个小型HTTP服务器，监听 **8080** 端口，
跟主协议用的8888端口TCP服务器是完全独立的两回事。它提供两类内容：

- `GET /api/stats?days=7|30` - 返回JSON格式的数据，包括今日/本月/总营收、
  所选时间窗口内的营收趋势、电桩状态分布、各充电站在线率。底层复用的是
  跟主协议里 `admin_stats` 完全相同的 `Database::` 查询函数。
- `GET /api/bigdata` - 返回Spark数仓已计算完成的总体、站点、用户、设备、
  星期、节假日、天气影响、设备运行和数据质量JSON快照。运行方式见
  `bigdata/README.md`。
- 其余所有路径 - 会去 `dashboard/` 文件夹下找对应的静态文件（目前只有
  `dashboard.html`），所以直接用浏览器打开 `http://localhost:8080/` 就能
  看到大屏页面。

大屏页面（`dashboard/dashboard.html`）是纯HTML/JS写的，用CDN引入的
[ECharts](https://echarts.apache.org/) 画图。页面优先读取 `/api/bigdata`，并在
普通静态HTTP服务器环境下自动回退到 `dashboard/data/bigdata.json`。页面展示
充电业务、站点、用户、设备、星期、节假日、天气影响和设备运行八类指标；点击
页面右上角的“刷新数据”可重新读取最新快照。

### Hadoop 数据导入

`bigdata/etl/load_data.py` 使用 PySpark 从 HDFS 读取 `users.csv`、
`stations.csv`、`devices.csv`、`weather_hourly.csv`、`charging_orders.csv` 和
`device_status_log.csv`，完成字段转换、无效记录过滤和主键去重后，再写回
HDFS。默认目录为
`hdfs:///charging-platform/data`，也可通过参数指定：

```bash
pip install -r requirements-bigdata.txt
spark-submit --master local[2] bigdata/etl/load_data.py \
  --data-root hdfs:///charging-platform/data \
  --format parquet
```

若需在本机联调，可把 `--data-root` 改成 `file:///绝对路径/data`。Spark 的 CSV
输出是包含多个 `part-*.csv` 的目录；只有演示用小数据需要单个分片时才添加
`--partitions 1`。

构建时会自动把整个 `dashboard/` 文件夹复制到编译出来的 `ChargingServer`
可执行文件旁边（见 `server/CMakeLists.txt`），所以运行时不需要依赖源码树
里的相对路径也能找到这个文件夹。

查看方法：启动 `ChargingServer` 后，在同一台电脑上用浏览器打开
`http://localhost:8080/` 即可。如果服务器跑在别的机器上，把 `localhost`
换成那台机器的地址。

## 腾讯地图导航

充电站详情页会打开一个内嵌的导航页面，支持驾车/步行路线选择。需要在所选的
Qt kit里安装 `WebEngineWidgets` 组件，才能在客户端内部显示腾讯路线页面；
如果这个组件不可用，项目依然能正常构建，只是会显示一个链接，点击后用系统
浏览器打开同样的路线。在当前本地的Qt 6.11.2安装里，`WebEngineWidgets`
只在 `msvc2022_64` kit里可用，MinGW kit里没有。

启动服务器前，需要把 `TENCENT_MAP_KEY` 设置为一个腾讯位置服务的WebService
密钥。客户端调用腾讯URI API时用的是 `TENCENT_MAP_REFERER`；如果没设置这个
变量，会自动回退用 `TENCENT_MAP_KEY` 的值。

```powershell
$env:TENCENT_MAP_KEY = "your-webservice-key"
$env:TENCENT_MAP_REFERER = "your-browser-key"
```
