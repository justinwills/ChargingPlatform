#ifndef DATABASE_H
#define DATABASE_H

#include <QString>
#include <QList>
#include <QMap>
#include <QPair>
#include <QSqlDatabase>

// ===== 数据结构：对应《概要设计说明书》里的数据库表 =====
// 用途：查询结果的载体。充电用户端 / PC服务器端拿到这些结构体后，
// 既可以直接绑定到界面控件，也可以按字段组包成4.2节里定义的JSON报文（data字段）。

struct UserInfo {
    int id = -1;
    QString phone;
    QString nickname;
    QString avatarPath;
    double balance = 0;
    QString status;       // "正常" / "冻结"
    QString createdAt;
};

struct StationInfo {
    int id = -1;
    QString name;
    QString address;
    double longitude = 0;
    double latitude = 0;
    double price = 0;
    int pileCount = 0;
};

struct PileInfo {
    int id = -1;
    int stationId = -1;
    QString stationName;  // 联表查出来的所属电站名，方便直接展示，不用再单独查一次
    QString code;
    QString type;         // "快充" / "慢充"
    double power = 0;
    QString status;       // "在用" / "闲置" / "故障"
    int totalSessions = 0;
    int totalDuration = 0; // 累计充电时长（分钟）
};

struct OrderInfo {
    int id = -1;
    int userId = -1;
    int pileId = -1;
    QString startTime;
    QString endTime;
    double amount = 0;    // 充电量（度）
    double fee = 0;       // 费用（元）
    QString status;       // "充电中" / "待结算" / "已结算"
};

struct LoginLogInfo {
    int id = -1;
    QString phone;
    QString loginTime;
    QString ipAddress;
};

struct OperationLogInfo {
    int id = -1;
    int operatorId = -1;
    QString operatorType;
    QString operationType;
    QString targetTable;
    int targetId = -1;
    QString content;
    QString operationTime;
};

// 数据库连接与初始化工具类。
// database.h / database.cpp 放在仓库根目录的 db/ 下，其他 Qt 项目通过 INCLUDEPATH 和
// ../db/database.cpp 引用同一份源码，避免多份拷贝后字段或函数实现不一致。
//
// 下面每个函数对应需求矩阵里的一项具体功能，函数名旁边的注释标了是第几项、
// 归哪个模块负责，方便对照《需求进度管理表》去调用。
// 约定：bool 返回值表示"这次数据库操作本身有没有出错/条件是否满足"；
// 需要把查询结果带出来的，用 out 指针参数；出错时统一用 qDebug() 打印原因，
// 方便联调时在 应用程序输出 面板里直接看到发生了什么。
class Database
{
public:
    static bool init(const QString &dbPath = "charging.db");

    // ---------- 管理员登录（PC服务器端 第13项） ----------
    static bool checkAdminLogin(const QString &username, const QString &password);

    // ---------- 用户 / 手机号登录与信息维护（充电用户端 第5-8项） ----------
    // 手机号免密登录：手机号已存在则直接登录；不存在则自动注册
    // （默认昵称"用户+手机号后4位"，余额0），首次登录即完成注册
    static bool phoneLogin(const QString &phone, UserInfo *outUser);
    static bool getUserById(int userId, UserInfo *outUser);
    static bool updateUserProfile(int userId, const QString &nickname, const QString &avatarPath);
    static bool rechargeBalance(int userId, double amount);

    // ---------- 登录记录（数据库端 第31项，新增子模块）----------
    // phoneLogin内部每次登录成功（不管新老用户）都会自动调用这个函数记一笔，
    // 调用方一般不需要手动调，这里公开出来是方便PC服务器端以后要做"登录记录查询"时直接用
    static bool logLoginRecord(const QString &phone, const QString &ipAddress = QString());
    static QList<LoginLogInfo> getLoginHistory(const QString &phone = QString(), int limit = 50);

    static bool logOperation(int operatorId, const QString &operatorType,
                             const QString &operationType, const QString &targetTable,
                             int targetId, const QString &content);
    static QList<OperationLogInfo> getOperationLogs(const QString &targetTable = QString(),
                                                     int operatorId = -1,
                                                     int limit = 50);

    // ---------- 用户管理（PC服务器端 第22-24项） ----------
    // phoneKeyword 留空返回全部用户；非空则按手机号模糊搜索（第24项）
    static QList<UserInfo> getAllUsers(const QString &phoneKeyword = QString());
    static bool setUserStatus(int userId, const QString &status); // status传"正常"或"冻结"，对应第23项

    // ---------- 充电站（充电用户端 第2-3项 / PC服务器端 第19-21项） ----------
    static QList<StationInfo> getAllStations();
    static bool getStationById(int stationId, StationInfo *outStation);
    static bool addStation(const QString &name, const QString &address,
                            double longitude, double latitude, double price); // 第21项 新增电站
    static int getFreePileCount(int stationId);        // 空闲电桩数量，充电站列表卡片要用（第2项）
    static double getStationOnlineRate(int stationId);  // 在线率(0~100)：非"故障"电桩占比（第19项）

    // ---------- 充电桩（充电用户端 第3项 / PC服务器端 第16-18,20项） ----------
    static QList<PileInfo> getAllPiles();               // 全部电桩+所属电站名（第17项 电桩列表）
    static QList<PileInfo> getPilesByStation(int stationId); // 某电站的电桩（第3,20项 站点详情）
    static bool getPileById(int pileId, PileInfo *outPile);
    static bool restartPile(int pileId);                // 远程重启：模拟指令，重置状态为"闲置"（第18项）
    static QMap<QString, int> getPileStatusStats();      // 状态分布统计（第16项）

    // ---------- 充电流程 / 订单（充电用户端 第9-12项） ----------
    // 充电前订单检查：查该用户名下是否还有一笔未结算的"充电中"订单（第9项）
    static bool hasOngoingOrder(int userId, int *outOrderId = nullptr);
    // 发起充电：先确认电桩是"闲置"，是则建单并把电桩状态改成"在用"（第10项）
    static bool startCharging(int userId, int pileId, int *outOrderId);
    // 结算：从用户钱包扣 fee，订单标记"已结算"，电桩状态改回"闲置"并累加统计（第12项）
    // 余额不足时返回 false（对应《概要设计说明书》第五章"结算时余额不足"的错误处理要求）
    static bool settleOrder(int orderId, double amount, double fee);
    static bool getOrderById(int orderId, OrderInfo *outOrder);  // 充电中状态展示用（第11项）
    static QList<OrderInfo> getUserOrders(int userId);

    // ---------- 销售业绩（PC服务器端 第14-15项） ----------
    static double getRevenueToday();
    static double getRevenueThisMonth();
    static double getRevenueTotal();
    // 近 days 天每天的营收，QChart画折线图直接用；结果按日期从早到晚排列（第14项）
    static QList<QPair<QString, double>> getRevenueTrend(int days = 7);

private:
    static void createTables();
    static void seedTestData();

    // 多线程支持：每个线程拥有自己独立的数据库连接，见database.cpp顶部注释
    static QString s_dbPath;
    static QSqlDatabase currentThreadDb();
};

#endif // DATABASE_H
