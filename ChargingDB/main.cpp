#include "database.h"
#include <QCoreApplication>
#include <QDebug>

// 这个 main.cpp 只是用来测试 database.h/database.cpp 能不能正常工作，
// 不是充电用户端或PC服务器端的正式代码。
// 确认没问题后，把 database.h 和 database.cpp 这两个文件复制到
// 真正的充电用户端项目和PC服务器端项目里就可以直接用了（见 README）。
int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    if (!Database::init("charging.db")) {
        qDebug() << "数据库初始化失败！";
        return -1;
    }
    qDebug() << "数据库初始化成功，charging.db 已创建。";

    // ===== 1. 管理员登录（PC服务器端） =====
    qDebug() << "\n===== 管理员登录测试 =====";
    qDebug() << "admin/123456 正确密码：" << Database::checkAdminLogin("admin", "123456");
    qDebug() << "admin/000000 错误密码：" << Database::checkAdminLogin("admin", "000000");

    // ===== 2. 手机号登录 / 自动注册（充电用户端） =====
    qDebug() << "\n===== 手机号登录测试 =====";
    UserInfo existingUser;
    Database::phoneLogin("13800000001", &existingUser); // 种子数据里已有的号码
    qDebug().noquote() << QString("老用户登录：%1，昵称=%2，余额=%3")
        .arg(existingUser.id).arg(existingUser.nickname).arg(existingUser.balance);

    UserInfo newUser;
    Database::phoneLogin("13912345678", &newUser); // 全新号码，应自动注册
    qDebug().noquote() << QString("新用户自动注册：%1，昵称=%2（应为“用户5678”）")
        .arg(newUser.id).arg(newUser.nickname);

    // ===== 2b. 登录记录（数据库端 第31项，新增） =====
    qDebug() << "\n===== 登录记录测试（新增） =====";
    const auto allLogs = Database::getLoginHistory();
    qDebug().noquote() << QString("全部登录记录数：%1（应 >= 2，刚才两次登录都会自动记一笔）").arg(allLogs.size());
    const auto newUserLogs = Database::getLoginHistory("13912345678");
    if (!newUserLogs.isEmpty()) {
        qDebug().noquote() << QString("手机号13912345678最近一次登录时间：%1").arg(newUserLogs.first().loginTime);
    }

    // ===== 3. 充电站列表 + 空闲电桩数（充电用户端 第2项） =====
    qDebug() << "\n===== 充电站列表 =====";
    const auto stations = Database::getAllStations();
    for (const auto &s : stations) {
        qDebug().noquote() << QString("站点%1：%2 | %3元/度 | 空闲电桩 %4/%5 | 在线率 %6%")
            .arg(s.id).arg(s.name).arg(s.price)
            .arg(Database::getFreePileCount(s.id)).arg(s.pileCount)
            .arg(Database::getStationOnlineRate(s.id), 0, 'f', 0);
    }

    // ===== 4. 电桩列表 + 状态分布统计（PC服务器端 第16-17项） =====
    qDebug() << "\n===== 电桩列表（附所属电站） =====";
    const auto piles = Database::getAllPiles();
    for (const auto &p : piles) {
        qDebug().noquote() << QString("电桩%1（%2）状态：%3，所属：%4")
            .arg(p.code).arg(p.type).arg(p.status).arg(p.stationName);
    }

    qDebug() << "\n===== 电桩状态分布统计 =====";
    const auto stats = Database::getPileStatusStats();
    for (auto it = stats.constBegin(); it != stats.constEnd(); ++it) {
        qDebug().noquote() << QString("%1：%2个").arg(it.key()).arg(it.value());
    }

    // ===== 5. 完整充电流程：发起充电 -> 结算（充电用户端 第9-12项） =====
    qDebug().noquote() << QString("\n===== 充电流程测试（用户%1，电桩A01） =====").arg(existingUser.id);
    bool ongoing = Database::hasOngoingOrder(existingUser.id);
    qDebug() << "充电前检查，是否已有进行中订单：" << ongoing;

    if (!ongoing) {
        int orderId = -1;
        // 先找到 A01 的电桩ID（种子数据里是闲置状态）
        int a01Id = -1;
        for (const auto &p : piles) {
            if (p.code == "A01") { a01Id = p.id; break; }
        }
        bool started = Database::startCharging(existingUser.id, a01Id, &orderId);
        qDebug() << "发起充电：" << started << "，订单号：" << orderId;

        if (started) {
            bool settled = Database::settleOrder(orderId, 10.5 /*度*/, 15.75 /*元*/);
            qDebug() << "结算：" << settled;

            UserInfo afterSettle;
            Database::getUserById(existingUser.id, &afterSettle);
            qDebug().noquote() << QString("结算后余额：%1（应比登录时少15.75）")
                .arg(afterSettle.balance);
        }
    }

    // ===== 6. 销售业绩汇总（PC服务器端 第14-15项） =====
    qDebug() << "\n===== 销售业绩 =====";
    qDebug().noquote() << QString("今日营收：%1 | 本月营收：%2 | 总营收：%3")
        .arg(Database::getRevenueToday())
        .arg(Database::getRevenueThisMonth())
        .arg(Database::getRevenueTotal());

    return 0;
}
