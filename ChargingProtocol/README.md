# ChargingProtocol —— Socket通信层参考实现

对应《概要设计说明书》2.3节（C/S架构、Socket通信、服务端多线程）和4.2节
（JSON请求/响应协议）。这一层是在 `ChargingDB` 提供的 `Database` 类之上，
再包一层"怎么通过网络把请求送过去、把结果传回来"，帮邱辰笙（充电用户端）和
特布新（PC服务器端）省掉从零搭建这部分的时间——两边各自把自己的UI/业务逻辑
接到对应的类上就能用，不用重新处理粘包拆包、多线程这些细节。

## 文件夹内容

| 文件 | 作用 | 谁会用到 |
|---|---|---|
| `protocolcodec.h/.cpp` | JSON报文的打包/解包（含长度前缀，解决粘包拆包） | 客户端、服务端都要 |
| `serverlistener.h/.cpp` | 服务端监听器，每个新连接自动分配一个独立线程 | 特布新 |
| `clientthread.h/.cpp` | 单个客户端连接的处理线程（收请求→查数据库→回响应） | 特布新（内部用，一般不用改） |
| `requestdispatcher.h/.cpp` | 把action字符串分发到对应的`Database::`函数 | 特布新（往这里加新的action） |
| `clientconnection.h/.cpp` | 客户端连接封装（连接服务器、发请求、收响应信号） | 邱辰笙 |
| `database.h/.cpp` | 跟 `ChargingDB` 里同一份（已支持多线程，见下方说明） | 两边都要，直接拷贝过去 |

## 协议格式（对应概要设计说明书4.2节）

```
[4字节 大端序 长度前缀N][N字节 UTF-8 JSON文本]
```

JSON本体格式不变：
```json
请求: {"action": "query_stations", "params": {...}}
响应: {"code": 0, "msg": "ok", "data": {...}}
```
`code`: 0=成功，1=参数错误，2=业务规则不允许（比如余额不足/电桩被占用），3=系统内部错误

## 已经实现的action

`requestdispatcher.h` 顶部注释里有完整清单，目前包括概要设计说明书建议的6个
（login / query_stations / query_pile_detail / start_charging / query_order /
settle_order）+ 额外补充的 admin_login。特布新要加PC服务器端专属的action
（电桩管理、用户管理等），照着 `requestdispatcher.cpp` 里现成的写法抄一份，
换成对应的 `Database::` 函数调用即可，不用碰 `ClientThread`/`ServerListener`
这些底层的部分。

## 服务端怎么接入（特布新）

```cpp
// main.cpp 最开头
Database::init("charging.db");

// 想监听的地方
auto *server = new ServerListener(this);
connect(server, &ServerListener::clientLog, this, [](const QString &msg){
    qDebug() << msg;   // 或者接到界面日志区
});
server->listen(QHostAddress::Any, 8888);
```
每来一个新连接，`ServerListener` 会自动创建一个独立线程处理它（对应
"主框架应为多线程结构"的要求），主线程只管UI，不会被网络I/O卡住。

## 客户端怎么接入（邱辰笙）

```cpp
auto *conn = new ClientConnection(this);
connect(conn, &ClientConnection::responseReceived, this, &MainWindow::onServerResponse);
connect(conn, &ClientConnection::connectionError, this, &MainWindow::onConnError);
conn->connectToServer("127.0.0.1", 8888);

// 需要发请求的时候
conn->sendRequest("login", {{"phone", "13800000001"}});
```

## 关于多线程访问数据库（重要背景，不用改代码，了解一下就行）

`Database` 类内部维护的是"每个线程一条独立数据库连接"（第一次调用时自动开），
不是全局共用一条。这是因为Qt的数据库连接本来就规定"只能在创建它的那个线程里用"，
一开始的版本没注意这点，在多线程环境下测试时报了
`requested database does not belong to the calling thread` 的错——现在已经改好，
正常调用 `Database::xxx()` 就行，不用自己管理连接。

## 已验证：真实收发测试（不是纸面设计）

写了一个独立测试程序，真的起了一个服务端（多线程）+ 一个客户端，通过真实TCP
连接跑了10个请求，覆盖全部7种action，包含正常流程和该拒绝的异常流程：

```
✓ 登录（老用户）—— 昵称/余额正确
✓ 管理员登录 —— 正确密码通过，错误密码拒绝(code=2)
✓ 查询充电站列表 —— 2个站点，空闲电桩数/在线率计算正确
✓ 查询电桩详情
✓ 发起充电 —— 电桩闲置时成功，返回订单号
✓ 重复发起充电 —— 已有进行中订单时正确拒绝(code=2)
✓ 查询订单 —— 状态=充电中
✓ 结算 —— 成功
✓ 查询订单（结算后）—— 状态=已结算，费用正确
```
10/10全部通过，`qmake6 && make` 编译干净（0 warning 0 error）。

测试过程中还顺手修掉两个真实的多线程bug（跨线程创建QObject导致的警告、
线程还在跑就被销毁导致的崩溃），细节都写在 `clientthread.cpp` 的注释里。

## 还没测的部分

- 只测了单个客户端连接；没有测多个客户端同时连接、同时抢同一个电桩下单的并发场景
  （`startCharging`里虽然有做闲置状态校验，但严格的并发安全需要数据库层面加锁，
  这部分如果时间够可以再加固，第一阶段应该够用）
- UI层面的实际接入（邱辰笙/特布新各自项目里真正调用这些接口）还没做，这份只是
  底层通信层，需要各自接上界面代码
