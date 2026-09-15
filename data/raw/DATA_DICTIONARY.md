# 六表训练数据说明

这些CSV是合成训练数据，不代表真实用户、设备状态或真实历史天气。

## 关联关系

- `charging_orders.user_id -> users.user_id`
- `charging_orders.station_id -> stations.station_id`
- `charging_orders.device_id -> devices.device_id`
- `charging_orders.weather_id -> weather_hourly.weather_id`
- `devices.station_id -> stations.station_id`
- `device_status_log.device_id -> devices.device_id`

## 表粒度

- `users.csv`：每位用户一行。
- `stations.csv`：每个充电站一行。
- `devices.csv`：每台充电桩一行。
- `weather_hourly.csv`：每个位置、每小时一行。
- `charging_orders.csv`：每笔充电订单一行。
- `device_status_log.csv`：每台设备、每天一行，记录当日在线和故障分钟数。

## 1. users.csv

| 字段 | 含义 |
|---|---|
| user_id | 用户主键 |
| registered_at | 注册时间，可用于新增用户和用户生命周期分析 |
| city | 用户所在城市 |
| member_level | bronze、silver、gold、platinum会员等级 |
| vehicle_type | sedan、suv、mpv、commercial车辆类型 |
| battery_capacity_kwh | 车辆电池容量，仅作为用户画像，不包含SOC遥测 |
| preferred_platform | android、ios或web |
| registration_channel | app、微信、站点扫码或合作渠道 |
| user_status | active或inactive |

## 2. stations.csv

| 字段 | 含义 |
|---|---|
| station_id | 充电站主键 |
| location_id | 天气位置关联键 |
| station_name、address、city | 站点名称和地址信息 |
| latitude、longitude | 站点坐标 |
| facility_type | 设施类型 |
| device_count | 站点设备数量，已与devices.csv核对一致 |
| open_time | 营业时间 |
| electricity_price_cny_kwh | 每千瓦时电价 |
| service_fee_cny_kwh | 每千瓦时服务费 |
| station_status | 站点当前状态 |
| updated_on | 站点信息更新时间 |

## 3. devices.csv

| 字段 | 含义 |
|---|---|
| device_id | 充电桩主键 |
| station_id | 所属站点外键 |
| device_code | 可展示的设备编码 |
| charger_type | DC_FAST或AC_SLOW |
| rated_power_kw | 额定功率 |
| connector_count | 充电枪数量 |
| manufacturer | 制造商 |
| commissioned_at | 投运日期 |
| last_maintenance_at | 最近维护日期 |
| current_status | online、offline或fault |

## 4. weather_hourly.csv

| 字段 | 含义 |
|---|---|
| weather_id | 天气记录主键 |
| location_id | 与站点关联的位置键 |
| weather_time | 整点时间 |
| weather_type、weather_name | 天气英文类别和中文名称 |
| temperature_c | 摄氏温度 |
| humidity_pct | 相对湿度百分比 |
| precipitation_mm | 小时降水量 |
| wind_speed_mps | 风速 |
| visibility_km | 能见度 |
| is_rain | 是否为小雨或大雨 |

## 5. charging_orders.csv

| 字段 | 含义 |
|---|---|
| session_id | 订单主键 |
| user_id、station_id、device_id | 用户、站点和设备外键 |
| location_id、weather_id | 位置和订单开始时刻天气外键 |
| created_at、ended_at | 充电开始和结束时间 |
| charge_time_hrs | 充电时长 |
| start_hour | 开始小时0至23 |
| weekday、weekday_name | 星期数字0至6和英文名称 |
| is_weekend | 是否自然周末 |
| is_holiday、holiday_name | 是否处于节假日区间以及节日名称 |
| is_workday | 是否按工作日处理，包含调休工作日 |
| energy_kwh | 订单充电量 |
| fee_amount_cny | 订单费用 |
| platform | 下单平台 |
| order_status | completed、interrupted或failed |
| payment_status | paid、pending、refunded或waived |

## 6. device_status_log.csv

| 字段 | 含义 |
|---|---|
| status_id | 日状态记录主键 |
| device_id、station_id | 设备和站点关联键 |
| record_time | 每日状态统计时点 |
| online_minutes、offline_minutes | 当日在线和离线分钟数 |
| fault_minutes、fault_count | 当日故障分钟数和次数 |
| primary_fault_code | 主要故障类型，NONE表示无故障 |
| successful_sessions、failed_sessions | 当日成功和失败会话数 |
| avg_output_power_kw | 当日订单平均输出功率 |
| availability_rate | 在线分钟数除以1440 |
| health_score | 结合设备年龄、故障、离线和负载生成的健康分 |
| maintenance_flag | 当日是否发生维护 |

## 机器学习注意事项

- 需求预测应先按站点和小时聚合订单，并保留没有订单的站点小时作为0样本。
- 星期直接由 `created_at` 计算，`weekday` 仅为方便核对。
- 订单包含 `is_weekend`、`is_holiday`、`holiday_name` 和 `is_workday`；调休周末按工作日标记。
- 天气通过 `weather_id` 关联。训练时使用天气类型、温度、湿度、降水等字段，不使用ID。
- 合成逻辑令小雨和大雨降低订单到达率，并让高湿/强降水轻微提高设备故障概率。
- 不要把 `fee_amount_cny` 用作订单量预测的未来输入，也不要使用预测时尚未产生的结束时间。

2015年节假日范围依据国务院办公厅安排；9月3日至5日纪念日假期依据国务院专项通知：

- https://www.nea.gov.cn/2015-01/08/c_133904166.htm
- https://www.ndrc.gov.cn/xxgk/zcfb/qt/201505/t20150513_967883.html
