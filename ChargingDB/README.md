# ChargingDB — 数据库端

## 文件夹内容
- `database.h` / `database.cpp` —— 现成的 `Database` 类：打开数据库、建5张表、填测试数据，
  **外加一整套可以直接调用的查询/写入函数**，覆盖《需求进度管理表》（需求矩阵）里
  所有需要访问数据库的功能项——具体对照表见下方。
- `schema.sql` —— 同一套表结构的参考文件（可以直接在 `sqlitebrowser` 里打开看）
- `main.cpp` + `ChargingDB.pro` —— **只是用来测试**，不是正式项目代码。会依次测试
  管理员登录、老用户登录+新用户自动注册、充电站/电桩列表、状态统计、完整充电流程
  （发起充电→结算→验证余额扣款）、销售业绩汇总。

## 使用前先测一遍（在虚拟机里）
1. 把这个文件夹拷进虚拟机（跟之前 FinanceCalculator 一样，用共享文件夹或者网盘都行）
2. 用 Qt Creator 打开 `ChargingDB.pro`，Configure Project，Ctrl+R
3. 因为是 `console` 程序，运行结果会显示在 Qt Creator 下方的"应用程序输出"面板里
4. 如果一切正常，应该能看到：管理员登录测试、手机号登录+自动注册测试、充电站/电桩列表、
   状态分布统计、一条完整的充电流程（含扣款验证）、销售业绩汇总

**这份代码目前的状态**：已经在真实的Qt6环境（qmake6 + Qt 6.4.2）里编译并测试通过，
`main.cpp` 里的每一项测试结果都跟预期完全一致（详见下方"已验证结果"）。放心直接用，
遇到问题大概率是各自开发环境的差异，不是代码本身的逻辑问题。

## 已验证结果（真实运行输出）
```
管理员登录：admin/123456 → true　　admin/000000 → false
老用户登录：昵称=用户0001，余额=100
新用户自动注册：昵称=用户5678（手机号后4位）
充电站1：空闲电桩 1/3，在线率 67%（1个故障）
充电站2：空闲电桩 2/2，在线率 100%
电桩状态统计：在用1 / 故障1 / 闲置3
充电流程：发起充电成功(订单号1) → 结算(10.5度/15.75元) → 余额 100→84.25 ✓
销售业绩：今日=本月=总计=15.75
```

## 怎么用到正式项目里（充电用户端 / PC服务器端）
等各自的 Qt 项目建好之后：
1. 把 `database.h` 和 `database.cpp` 拷贝到自己的项目文件夹里
2. 项目上右键 → Add Existing Files... → 选中这两个文件
3. 项目的 `.pro` 文件里确认有一行 `QT += sql`
4. 在自己项目的 `main.cpp` 里，**在创建第一个窗口之前**先调用一次 `Database::init("charging.db");`
5. **重要**：充电用户端和PC服务器端两边，`charging.db` 的路径必须指向**同一个文件**
   （比如放在同一个共享文件夹里，或者都用绝对路径写死）
6. **如果服务端要做成多线程**（每个客户端连接一个独立线程，参照概要设计说明书2.3节）：
   `Database` 类已经支持多线程了——每个线程第一次调用任何 `Database::xxx()` 函数时，
   会自动开一条属于这个线程自己的数据库连接，不用额外做什么，正常调用就行。
   （这是从一个真实bug里改出来的：一开始所有函数共用一条数据库连接，结果在子线程里
   一调用就报错"requested database does not belong to the calling thread"——Qt的
   数据库连接本来就不能跨线程共用，现在已经修好了，细节看 `database.cpp` 顶部注释）

## 函数清单 对照 需求矩阵（需求进度管理表）

查询结果对应的结构体（`UserInfo`、`StationInfo`、`PileInfo`、`OrderInfo`）定义在
`database.h` 最上面。

### 充电用户端（邱辰笙）—— 第1-12项
| # | 需求 | 对应函数 |
|---|---|---|
| 1 | 定位与地址转经纬度 | （不涉及数据库，直接调用腾讯地图Web API） |
| 2 | 充电站列表（距离/价格/空闲电桩数） | `getAllStations()` + `getFreePileCount(stationId)` |
| 3 | 充电站详情（该站全部电桩：编号/类型/状态/功率） | `getPilesByStation(stationId)` |
| 4 | 一键导航 | （不涉及数据库，腾讯地图Web API） |
| 5-6 | 手机号登录+自动注册+展示个人信息 | `phoneLogin(phone, &outUser)` |
| 7 | 修改头像/昵称 | `updateUserProfile(userId, nickname, avatarPath)` |
| 8 | 余额充值 | `rechargeBalance(userId, amount)` |
| 9 | 充电前检查是否有未结算订单 | `hasOngoingOrder(userId, &outOrderId)` |
| 10 | 选择电桩→发起充电 | `startCharging(userId, pileId, &outOrderId)` |
| 11 | 充电中状态展示 | `getOrderById(orderId, &outOrder)` |
| 12 | 结算（从余额扣款） | `settleOrder(orderId, amount, fee)` |

### PC服务器端（特布新）—— 第13-24项
| # | 需求 | 对应函数 |
|---|---|---|
| 13 | 管理员登录 | `checkAdminLogin(username, password)` |
| 14 | 营收趋势图表（供QChart用） | `getRevenueTrend(days)` |
| 15 | 今日/本月/总营收数字 | `getRevenueToday()` / `getRevenueThisMonth()` / `getRevenueTotal()` |
| 16 | 电桩状态分布统计 | `getPileStatusStats()` |
| 17 | 全部电桩列表（含所属电站名） | `getAllPiles()` |
| 18 | 电桩远程重启 | `restartPile(pileId)` |
| 19 | 充电站列表（含在线率） | `getAllStations()` + `getStationOnlineRate(stationId)` |
| 20 | 充电站详情 | `getPilesByStation(stationId)` |
| 21 | 新增充电站 | `addStation(name, address, lng, lat, price)` |
| 22 | 用户列表 | `getAllUsers()` |
| 23 | 冻结/解冻用户 | `setUserStatus(userId, "冻结"/"正常")` |
| 24 | 按手机号模糊搜索用户 | `getAllUsers(phoneKeyword)` |

### 登录记录（数据库端 第31项，新增子模块）
`phoneLogin()`内部现在会自动记一笔登录记录（新老用户都会记，不需要调用方额外处理），
对应新增的`login_logs`表（手机号+登录时间）。如果PC服务器端以后要做"登录记录查询"
这类功能，直接调下面这个函数：
```cpp
// 查全部登录记录（按时间倒序，默认最近50条）
QList<LoginLogInfo> logs = Database::getLoginHistory();
// 只查某个手机号的登录记录
QList<LoginLogInfo> logs = Database::getLoginHistory("13800000001");
```

所有查询函数返回 `bool`（表示操作成功/失败）用于单条操作，或者 `QList<...>` 用于列表。
需要返回单条数据的用 `out` 指针参数带出来，比如：
```cpp
UserInfo user;
if (Database::phoneLogin("13800000001", &user)) {
    qDebug() << user.nickname << user.balance;
}
```

## 已有的测试数据
- 管理员账号：`admin` / `123456`
- 2个充电站，5个电桩（状态故意设置得不一样：在用/闲置/故障都有，方便测试统计类功能）
- 1个测试用户：`13800000001`

如果开发过程中需要调整表结构/字段，直接改 `database.cpp` 里的 `createTables()` 部分——
团队里大家都用同一份文件，结构才不会各改各的对不上。

## 还没覆盖的部分（不属于数据库层的范围）
- 腾讯地图 Web API 的具体接入（地理编码+导航）—— 纯充电用户端的事，跟数据库无关
- Socket/TCP + 多线程的客户端-服务端通信层 —— 这是在 `Database` 类之上再包一层，
  通常在服务端：接收JSON请求 → 调用对应的 `Database::xxx()` 函数 → 把结果包成JSON响应
  返回去（具体报文格式见概要设计说明书4.2节）。**这部分现在已经有一版可用的参考实现了，
  见 `ChargingProtocol` 文件夹**——里面包含了JSON协议的打包/解包、服务端多线程监听、
  客户端连接封装，并且已经把 login/query_stations/start_charging/settle_order 等
  几个action接到这份 `Database` 上跑通了完整的真实收发测试。
