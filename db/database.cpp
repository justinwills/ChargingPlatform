#include "database.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QDebug>
#include <QThread>
#include <QStringList>

QString Database::s_dbPath;

// ============ 多线程支持 ============
// 背景：QSqlDatabase的一条连接只能在"创建它的那个线程"里使用，
// 其他线程拿着同一个连接对象去查询会报错
// （"requested database does not belong to the calling thread"）。
// PC服务器端是多线程结构（每个客户端连接一个独立线程，见概要设计说明书2.3节），
// 所以Database类不能只维护一条全局连接，得让"每个用到数据库的线程"
// 自己拥有一条独立的连接——这正是Qt官方文档里"Threads and the SQL Module"
// 一节推荐的标准做法：连接名按线程区分，各用各的，指向同一个数据库文件。
//
// currentThreadDb()：当前线程第一次调用时，自动开一条属于这个线程的新连接；
// 之后这个线程再调用，直接复用同一条，不会重复开。调用方（也就是Database类
// 内部的每个函数）完全不用关心这些细节，正常调用Database::xxx()即可，
// 该函数在哪个线程被调用，就会自动使用哪个线程自己的连接。
QSqlDatabase Database::currentThreadDb()
{
    const QString connName = QStringLiteral("conn_%1")
        .arg(reinterpret_cast<quintptr>(QThread::currentThreadId()));

    if (QSqlDatabase::contains(connName)) {
        QSqlDatabase db = QSqlDatabase::database(connName);
        if (!db.isOpen()) db.open();
        return db;
    }

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connName);
    db.setDatabaseName(s_dbPath);
    if (!db.open()) {
        qDebug() << "线程" << QThread::currentThreadId() << "打开数据库连接失败：" << db.lastError().text();
    }
    return db;
}

bool Database::init(const QString &dbPath)
{
    s_dbPath = dbPath;
    QSqlDatabase db = currentThreadDb();

    if (!db.isOpen()) {
        qDebug() << "数据库打开失败：" << db.lastError().text();
        return false;
    }

    createTables();
    seedTestData();
    return true;
}

// 建立全部7张表，对应《概要设计说明书》的字段设计
void Database::createTables()
{
    QSqlQuery query(currentThreadDb());

    // 用户表
    query.exec("create table if not exists users("
               "id integer primary key autoincrement, "
               "phone text unique not null, "
               "nickname text, "
               "avatar_path text, "
               "balance real default 0, "
               "status text default '正常', "
               "created_at datetime default current_timestamp)");

    // 充电站表
    query.exec("create table if not exists stations("
               "id integer primary key autoincrement, "
               "name text not null, "
               "address text, "
               "longitude real, "
               "latitude real, "
               "price real, "
               "pile_count integer default 0)");

    // 充电桩表
    query.exec("create table if not exists piles("
               "id integer primary key autoincrement, "
               "station_id integer, "
               "code text, "
               "type text, "
               "power real, "
               "status text default '闲置', "
               "total_sessions integer default 0, "
               "total_duration integer default 0, "
               "foreign key(station_id) references stations(id))");

    // 充电订单表
    query.exec("create table if not exists orders("
               "id integer primary key autoincrement, "
               "user_id integer, "
               "pile_id integer, "
               "start_time datetime, "
               "end_time datetime, "
               "amount real, "
               "fee real, "
               "status text default '充电中', "
               "foreign key(user_id) references users(id), "
               "foreign key(pile_id) references piles(id))");

    // 管理员表
    query.exec("create table if not exists admins("
               "id integer primary key autoincrement, "
               "username text unique not null, "
               "password text not null)");

    // 登录记录表（数据库端 第31项，新增）：只记"谁在什么时候登录过"，不存密码之类的敏感信息
    query.exec("create table if not exists login_logs("
               "id integer primary key autoincrement, "
               "phone text not null, "
               "login_time datetime default current_timestamp, "
               "ip_address text)");

    query.exec("create table if not exists operation_logs("
               "id integer primary key autoincrement, "
               "operator_id integer, "
               "operator_type text, "
               "operation_type text, "
               "target_table text, "
               "target_id integer, "
               "content text, "
               "operation_time datetime default current_timestamp)");

    // Keep older local SQLite files compatible after this schema update.
    query.exec("alter table login_logs add column ip_address text");
}

// 插入少量测试数据，只在表为空时插入一次（避免每次运行程序都重复插入）。
void Database::seedTestData()
{
    QSqlQuery query(currentThreadDb());

    // 默认管理员账号：admin / 123456
    query.exec("select count(*) from admins");
    if (query.next() && query.value(0).toInt() == 0) {
        query.exec("insert into admins(username, password) values('admin', '123456')");
    }

    // 2个测试充电站 + 5个测试电桩（状态故意各不相同，方便测试状态统计类功能）
    query.exec("select count(*) from stations");
    if (query.next() && query.value(0).toInt() == 0) {
        query.exec("insert into stations(name, address, longitude, latitude, price, pile_count) "
                    "values('东软科技园充电站', '大连市甘井子区东软路1号', 121.5, 38.9, 1.5, 3)");
        query.exec("insert into stations(name, address, longitude, latitude, price, pile_count) "
                    "values('万达广场充电站', '大连市西岗区万达路2号', 121.6, 38.91, 1.8, 2)");

        query.exec("insert into piles(station_id, code, type, power, status) "
                    "values(1, 'A01', '快充', 60, '闲置')");
        query.exec("insert into piles(station_id, code, type, power, status) "
                    "values(1, 'A02', '快充', 60, '在用')");
        query.exec("insert into piles(station_id, code, type, power, status) "
                    "values(1, 'A03', '慢充', 7, '故障')");
        query.exec("insert into piles(station_id, code, type, power, status) "
                    "values(2, 'B01', '快充', 60, '闲置')");
        query.exec("insert into piles(station_id, code, type, power, status) "
                    "values(2, 'B02', '慢充', 7, '闲置')");
    }

    // 1个测试用户
    query.exec("select count(*) from users");
    if (query.next() && query.value(0).toInt() == 0) {
        query.exec("insert into users(phone, nickname, balance) "
                    "values('13800000001', '用户0001', 100.0)");
    }
}

// ================= 管理员 =================
bool Database::checkAdminLogin(const QString &username, const QString &password)
{
    QSqlQuery query(currentThreadDb());
    query.prepare("select id from admins where username = ? and password = ?");
    query.addBindValue(username);
    query.addBindValue(password);
    if (!query.exec()) {
        qDebug() << "checkAdminLogin 查询失败：" << query.lastError().text();
        return false;
    }
    return query.next(); // 查到一行说明账号密码匹配
}

// ================= 用户 =================
bool Database::phoneLogin(const QString &phone, UserInfo *outUser)
{
    QSqlQuery query(currentThreadDb());
    query.prepare("select id, phone, nickname, avatar_path, balance, status, created_at "
                   "from users where phone = ?");
    query.addBindValue(phone);
    if (!query.exec()) {
        qDebug() << "phoneLogin 查询失败：" << query.lastError().text();
        return false;
    }

    if (query.next()) {
        // 手机号已存在，直接登录
        if (outUser) {
            outUser->id = query.value(0).toInt();
            outUser->phone = query.value(1).toString();
            outUser->nickname = query.value(2).toString();
            outUser->avatarPath = query.value(3).toString();
            outUser->balance = query.value(4).toDouble();
            outUser->status = query.value(5).toString();
            outUser->createdAt = query.value(6).toString();
        }
        logLoginRecord(phone);
        return true;
    }

    // 不存在则自动注册，默认昵称"用户+手机号后4位"
    QString defaultNickname = "用户" + phone.right(4);
    QSqlQuery insertQuery(currentThreadDb());
    insertQuery.prepare("insert into users(phone, nickname, balance, status) "
                         "values(?, ?, 100.0, '正常')");
    insertQuery.addBindValue(phone);
    insertQuery.addBindValue(defaultNickname);
    if (!insertQuery.exec()) {
        qDebug() << "phoneLogin 自动注册失败：" << insertQuery.lastError().text();
        return false;
    }

    if (outUser) {
        outUser->id = insertQuery.lastInsertId().toInt();
        outUser->phone = phone;
        outUser->nickname = defaultNickname;
        outUser->avatarPath = QString();
        outUser->balance = 0;
        outUser->status = "正常";
        outUser->createdAt = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    }
    logLoginRecord(phone);
    return true;
}

// ---------- 登录记录（第31项）----------
bool Database::logLoginRecord(const QString &phone, const QString &ipAddress)
{
    QSqlQuery query(currentThreadDb());
    query.prepare("insert into login_logs(phone, ip_address) values(?, ?)");
    query.addBindValue(phone);
    query.addBindValue(ipAddress);
    if (!query.exec()) {
        qDebug() << "logLoginRecord 写入失败：" << query.lastError().text();
        return false;
    }
    return true;
}

QList<LoginLogInfo> Database::getLoginHistory(const QString &phone, int limit)
{
    QList<LoginLogInfo> list;
    QSqlQuery query(currentThreadDb());
    if (phone.isEmpty()) {
        query.prepare("select id, phone, login_time, ip_address from login_logs "
                       "order by login_time desc limit ?");
        query.addBindValue(limit);
    } else {
        query.prepare("select id, phone, login_time, ip_address from login_logs "
                       "where phone = ? order by login_time desc limit ?");
        query.addBindValue(phone);
        query.addBindValue(limit);
    }
    if (!query.exec()) {
        qDebug() << "getLoginHistory 查询失败：" << query.lastError().text();
        return list;
    }
    while (query.next()) {
        LoginLogInfo info;
        info.id = query.value(0).toInt();
        info.phone = query.value(1).toString();
        info.loginTime = query.value(2).toString();
        info.ipAddress = query.value(3).toString();
        list.append(info);
    }
    return list;
}

bool Database::logOperation(int operatorId, const QString &operatorType,
                            const QString &operationType, const QString &targetTable,
                            int targetId, const QString &content)
{
    QSqlQuery query(currentThreadDb());
    query.prepare("insert into operation_logs(operator_id, operator_type, operation_type, "
                  "target_table, target_id, content) values(?, ?, ?, ?, ?, ?)");
    query.addBindValue(operatorId);
    query.addBindValue(operatorType);
    query.addBindValue(operationType);
    query.addBindValue(targetTable);
    query.addBindValue(targetId);
    query.addBindValue(content);
    if (!query.exec()) {
        qDebug() << "logOperation 写入失败：" << query.lastError().text();
        return false;
    }
    return true;
}

QList<OperationLogInfo> Database::getOperationLogs(const QString &targetTable,
                                                    int operatorId,
                                                    int limit)
{
    QList<OperationLogInfo> list;
    QSqlQuery query(currentThreadDb());

    QString sql = "select id, operator_id, operator_type, operation_type, target_table, "
                  "target_id, content, operation_time from operation_logs";
    QStringList conditions;
    if (!targetTable.isEmpty()) {
        conditions << "target_table = ?";
    }
    if (operatorId >= 0) {
        conditions << "operator_id = ?";
    }
    if (!conditions.isEmpty()) {
        sql += " where " + conditions.join(" and ");
    }
    sql += " order by operation_time desc limit ?";

    query.prepare(sql);
    if (!targetTable.isEmpty()) {
        query.addBindValue(targetTable);
    }
    if (operatorId >= 0) {
        query.addBindValue(operatorId);
    }
    query.addBindValue(limit);

    if (!query.exec()) {
        qDebug() << "getOperationLogs 查询失败：" << query.lastError().text();
        return list;
    }

    while (query.next()) {
        OperationLogInfo info;
        info.id = query.value(0).toInt();
        info.operatorId = query.value(1).toInt();
        info.operatorType = query.value(2).toString();
        info.operationType = query.value(3).toString();
        info.targetTable = query.value(4).toString();
        info.targetId = query.value(5).toInt();
        info.content = query.value(6).toString();
        info.operationTime = query.value(7).toString();
        list.append(info);
    }
    return list;
}

bool Database::getUserById(int userId, UserInfo *outUser)
{
    QSqlQuery query(currentThreadDb());
    query.prepare("select id, phone, nickname, avatar_path, balance, status, created_at "
                   "from users where id = ?");
    query.addBindValue(userId);
    if (!query.exec() || !query.next()) {
        return false;
    }
    if (outUser) {
        outUser->id = query.value(0).toInt();
        outUser->phone = query.value(1).toString();
        outUser->nickname = query.value(2).toString();
        outUser->avatarPath = query.value(3).toString();
        outUser->balance = query.value(4).toDouble();
        outUser->status = query.value(5).toString();
        outUser->createdAt = query.value(6).toString();
    }
    return true;
}

bool Database::updateUserProfile(int userId, const QString &nickname, const QString &avatarPath)
{
    QSqlQuery query(currentThreadDb());
    query.prepare("update users set nickname = ?, avatar_path = ? where id = ?");
    query.addBindValue(nickname);
    query.addBindValue(avatarPath);
    query.addBindValue(userId);
    if (!query.exec()) {
        qDebug() << "updateUserProfile 失败：" << query.lastError().text();
        return false;
    }
    return query.numRowsAffected() > 0;
}

bool Database::rechargeBalance(int userId, double amount)
{
    if (amount <= 0) {
        qDebug() << "rechargeBalance 失败：充值金额必须为正数";
        return false;
    }
    QSqlQuery query(currentThreadDb());
    query.prepare("update users set balance = balance + ? where id = ?");
    query.addBindValue(amount);
    query.addBindValue(userId);
    if (!query.exec()) {
        qDebug() << "rechargeBalance 失败：" << query.lastError().text();
        return false;
    }
    return query.numRowsAffected() > 0;
}

QList<UserInfo> Database::getAllUsers(const QString &phoneKeyword)
{
    QList<UserInfo> result;
    QSqlQuery query(currentThreadDb());
    if (phoneKeyword.isEmpty()) {
        query.prepare("select id, phone, nickname, avatar_path, balance, status, created_at "
                       "from users order by id");
    } else {
        query.prepare("select id, phone, nickname, avatar_path, balance, status, created_at "
                       "from users where phone like ? order by id");
        query.addBindValue("%" + phoneKeyword + "%");
    }
    if (!query.exec()) {
        qDebug() << "getAllUsers 失败：" << query.lastError().text();
        return result;
    }
    while (query.next()) {
        UserInfo u;
        u.id = query.value(0).toInt();
        u.phone = query.value(1).toString();
        u.nickname = query.value(2).toString();
        u.avatarPath = query.value(3).toString();
        u.balance = query.value(4).toDouble();
        u.status = query.value(5).toString();
        u.createdAt = query.value(6).toString();
        result.append(u);
    }
    return result;
}

bool Database::setUserStatus(int userId, const QString &status)
{
    QSqlQuery query(currentThreadDb());
    query.prepare("update users set status = ? where id = ?");
    query.addBindValue(status);
    query.addBindValue(userId);
    if (!query.exec()) {
        qDebug() << "setUserStatus 失败：" << query.lastError().text();
        return false;
    }
    return query.numRowsAffected() > 0;
}

// ================= 充电站 =================
QList<StationInfo> Database::getAllStations()
{
    QList<StationInfo> result;
    QSqlQuery query("select id, name, address, longitude, latitude, price, pile_count "
                     "from stations order by id", currentThreadDb());
    while (query.next()) {
        StationInfo s;
        s.id = query.value(0).toInt();
        s.name = query.value(1).toString();
        s.address = query.value(2).toString();
        s.longitude = query.value(3).toDouble();
        s.latitude = query.value(4).toDouble();
        s.price = query.value(5).toDouble();
        s.pileCount = query.value(6).toInt();
        result.append(s);
    }
    return result;
}

bool Database::getStationById(int stationId, StationInfo *outStation)
{
    QSqlQuery query(currentThreadDb());
    query.prepare("select id, name, address, longitude, latitude, price, pile_count "
                   "from stations where id = ?");
    query.addBindValue(stationId);
    if (!query.exec() || !query.next()) {
        return false;
    }
    if (outStation) {
        outStation->id = query.value(0).toInt();
        outStation->name = query.value(1).toString();
        outStation->address = query.value(2).toString();
        outStation->longitude = query.value(3).toDouble();
        outStation->latitude = query.value(4).toDouble();
        outStation->price = query.value(5).toDouble();
        outStation->pileCount = query.value(6).toInt();
    }
    return true;
}

bool Database::addStation(const QString &name, const QString &address,
                           double longitude, double latitude, double price)
{
    QSqlQuery query(currentThreadDb());
    query.prepare("insert into stations(name, address, longitude, latitude, price, pile_count) "
                   "values(?, ?, ?, ?, ?, 0)");
    query.addBindValue(name);
    query.addBindValue(address);
    query.addBindValue(longitude);
    query.addBindValue(latitude);
    query.addBindValue(price);
    if (!query.exec()) {
        qDebug() << "addStation 失败：" << query.lastError().text();
        return false;
    }
    return true;
}

int Database::getFreePileCount(int stationId)
{
    QSqlQuery query(currentThreadDb());
    query.prepare("select count(*) from piles where station_id = ? and status = '闲置'");
    query.addBindValue(stationId);
    if (!query.exec() || !query.next()) {
        return 0;
    }
    return query.value(0).toInt();
}

double Database::getStationOnlineRate(int stationId)
{
    QSqlQuery query(currentThreadDb());
    query.prepare("select count(*), sum(case when status != '故障' then 1 else 0 end) "
                   "from piles where station_id = ?");
    query.addBindValue(stationId);
    if (!query.exec() || !query.next()) {
        return 0;
    }
    int total = query.value(0).toInt();
    if (total == 0) {
        return 0;
    }
    int online = query.value(1).toInt();
    return (online * 100.0) / total;
}

// ================= 充电桩 =================
QList<PileInfo> Database::getAllPiles()
{
    QList<PileInfo> result;
    QSqlQuery query("select piles.id, piles.station_id, stations.name, piles.code, piles.type, "
                     "piles.power, piles.status, piles.total_sessions, piles.total_duration "
                     "from piles join stations on piles.station_id = stations.id order by piles.id", currentThreadDb());
    while (query.next()) {
        PileInfo p;
        p.id = query.value(0).toInt();
        p.stationId = query.value(1).toInt();
        p.stationName = query.value(2).toString();
        p.code = query.value(3).toString();
        p.type = query.value(4).toString();
        p.power = query.value(5).toDouble();
        p.status = query.value(6).toString();
        p.totalSessions = query.value(7).toInt();
        p.totalDuration = query.value(8).toInt();
        result.append(p);
    }
    return result;
}

QList<PileInfo> Database::getPilesByStation(int stationId)
{
    QList<PileInfo> result;
    QSqlQuery query(currentThreadDb());
    query.prepare("select piles.id, piles.station_id, stations.name, piles.code, piles.type, "
                   "piles.power, piles.status, piles.total_sessions, piles.total_duration "
                   "from piles join stations on piles.station_id = stations.id "
                   "where piles.station_id = ? order by piles.id");
    query.addBindValue(stationId);
    if (!query.exec()) {
        qDebug() << "getPilesByStation 失败：" << query.lastError().text();
        return result;
    }
    while (query.next()) {
        PileInfo p;
        p.id = query.value(0).toInt();
        p.stationId = query.value(1).toInt();
        p.stationName = query.value(2).toString();
        p.code = query.value(3).toString();
        p.type = query.value(4).toString();
        p.power = query.value(5).toDouble();
        p.status = query.value(6).toString();
        p.totalSessions = query.value(7).toInt();
        p.totalDuration = query.value(8).toInt();
        result.append(p);
    }
    return result;
}

bool Database::getPileById(int pileId, PileInfo *outPile)
{
    QSqlQuery query(currentThreadDb());
    query.prepare("select piles.id, piles.station_id, stations.name, piles.code, piles.type, "
                   "piles.power, piles.status, piles.total_sessions, piles.total_duration "
                   "from piles join stations on piles.station_id = stations.id "
                   "where piles.id = ?");
    query.addBindValue(pileId);
    if (!query.exec() || !query.next()) {
        return false;
    }
    if (outPile) {
        outPile->id = query.value(0).toInt();
        outPile->stationId = query.value(1).toInt();
        outPile->stationName = query.value(2).toString();
        outPile->code = query.value(3).toString();
        outPile->type = query.value(4).toString();
        outPile->power = query.value(5).toDouble();
        outPile->status = query.value(6).toString();
        outPile->totalSessions = query.value(7).toInt();
        outPile->totalDuration = query.value(8).toInt();
    }
    return true;
}

bool Database::restartPile(int pileId)
{
    // 模拟"远程重启"：真实电桩场景应该是发一条指令下去，这里简化成直接把状态重置为"闲置"。
    // 注意：如果该电桩当前是"在用"（有进行中订单）就被强制重启，对应订单要由调用方自行处理
    // （比如提示管理员"该电桩有进行中订单，重启会中断充电"），这里只负责电桩状态本身。
    QSqlQuery query(currentThreadDb());
    query.prepare("update piles set status = '闲置' where id = ?");
    query.addBindValue(pileId);
    if (!query.exec()) {
        qDebug() << "restartPile 失败：" << query.lastError().text();
        return false;
    }
    return query.numRowsAffected() > 0;
}

QMap<QString, int> Database::getPileStatusStats()
{
    QMap<QString, int> result;
    QSqlQuery query("select status, count(*) from piles group by status", currentThreadDb());
    while (query.next()) {
        result[query.value(0).toString()] = query.value(1).toInt();
    }
    return result;
}

// ================= 充电流程 / 订单 =================
bool Database::hasOngoingOrder(int userId, int *outOrderId)
{
    QSqlQuery query(currentThreadDb());
    query.prepare("select id from orders where user_id = ? and status = '充电中'");
    query.addBindValue(userId);
    if (!query.exec() || !query.next()) {
        return false;
    }
    if (outOrderId) {
        *outOrderId = query.value(0).toInt();
    }
    return true;
}

bool Database::startCharging(int userId, int pileId, int *outOrderId)
{
    QSqlDatabase db = currentThreadDb();
    if (!db.transaction()) {
        qDebug() << "startCharging 失败：无法开启事务";
        return false;
    }

    auto rollback = [&db]() {
        db.rollback();
        return false;
    };

    QSqlQuery userQuery(db);
    userQuery.prepare("select id from users where id = ? and status = '正常'");
    userQuery.addBindValue(userId);
    if (!userQuery.exec() || !userQuery.next()) {
        qDebug() << "startCharging 失败：找不到该用户或用户已被冻结";
        return rollback();
    }

    // 条件更新同时完成"检查闲置"和"占用电桩"，避免并发请求重复占用。
    QSqlQuery occupyQuery(db);
    occupyQuery.prepare("update piles set status = '在用' "
                        "where id = ? and status = '闲置'");
    occupyQuery.addBindValue(pileId);
    if (!occupyQuery.exec()) {
        qDebug() << "startCharging 占用电桩失败：" << occupyQuery.lastError().text();
        return rollback();
    }
    if (occupyQuery.numRowsAffected() != 1) {
        qDebug() << "startCharging 失败：电桩不存在或当前不是闲置状态";
        return rollback();
    }

    QString now = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");

    QSqlQuery insertQuery(db);
    insertQuery.prepare("insert into orders(user_id, pile_id, start_time, status) "
                         "values(?, ?, ?, '充电中')");
    insertQuery.addBindValue(userId);
    insertQuery.addBindValue(pileId);
    insertQuery.addBindValue(now);
    if (!insertQuery.exec()) {
        qDebug() << "startCharging 建单失败：" << insertQuery.lastError().text();
        return rollback();
    }

    if (outOrderId) {
        *outOrderId = insertQuery.lastInsertId().toInt();
    }

    if (!db.commit()) {
        qDebug() << "startCharging 提交事务失败：" << db.lastError().text();
        db.rollback();
        return false;
    }
    return true;
}

bool Database::settleOrder(int orderId, double amount, double fee)
{
    QSqlDatabase db = currentThreadDb();
    if (!db.transaction()) {
        qDebug() << "settleOrder 失败：无法开启事务";
        return false;
    }

    auto rollback = [&db]() {
        db.rollback();
        return false;
    };

    // 取出订单，确认存在且还没结算过
    QSqlQuery orderQuery(db);
    orderQuery.prepare("select user_id, pile_id, start_time, status from orders where id = ?");
    orderQuery.addBindValue(orderId);
    if (!orderQuery.exec() || !orderQuery.next()) {
        qDebug() << "settleOrder 失败：找不到该订单";
        return rollback();
    }
    int userId = orderQuery.value(0).toInt();
    int pileId = orderQuery.value(1).toInt();
    QString startTime = orderQuery.value(2).toString();
    QString status = orderQuery.value(3).toString();
    if (status != "充电中") {
        qDebug() << "settleOrder 失败：订单当前不是充电中状态";
        return rollback();
    }

    // 校验钱包余额是否足够（对应《概要设计说明书》第五章"结算时余额不足"的错误处理要求）
    QSqlQuery balanceQuery(db);
    balanceQuery.prepare("select balance from users where id = ?");
    balanceQuery.addBindValue(userId);
    if (!balanceQuery.exec() || !balanceQuery.next()) {
        qDebug() << "settleOrder 失败：找不到该用户";
        return rollback();
    }
    double balance = balanceQuery.value(0).toDouble();
    if (balance < fee) {
        qDebug() << "settleOrder 失败：钱包余额不足";
        return rollback();
    }

    QString now = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    int durationMinutes = static_cast<int>(
        QDateTime::fromString(startTime, "yyyy-MM-dd HH:mm:ss")
            .secsTo(QDateTime::currentDateTime()) / 60);
    if (durationMinutes < 0) {
        durationMinutes = 0;
    }

    QSqlQuery updateOrder(db);
    updateOrder.prepare("update orders set end_time = ?, amount = ?, fee = ?, status = '已结算' "
                         "where id = ?");
    updateOrder.addBindValue(now);
    updateOrder.addBindValue(amount);
    updateOrder.addBindValue(fee);
    updateOrder.addBindValue(orderId);
    if (!updateOrder.exec() || updateOrder.numRowsAffected() != 1) {
        qDebug() << "settleOrder 更新订单失败：" << updateOrder.lastError().text();
        return rollback();
    }

    QSqlQuery deductBalance(db);
    deductBalance.prepare("update users set balance = balance - ? where id = ?");
    deductBalance.addBindValue(fee);
    deductBalance.addBindValue(userId);
    if (!deductBalance.exec() || deductBalance.numRowsAffected() != 1) {
        qDebug() << "settleOrder 扣款失败：" << deductBalance.lastError().text();
        return rollback();
    }

    QSqlQuery freePile(db);
    freePile.prepare("update piles set status = '闲置', "
                      "total_sessions = total_sessions + 1, "
                      "total_duration = total_duration + ? where id = ?");
    freePile.addBindValue(durationMinutes);
    freePile.addBindValue(pileId);
    if (!freePile.exec() || freePile.numRowsAffected() != 1) {
        qDebug() << "settleOrder 更新电桩失败：" << freePile.lastError().text();
        return rollback();
    }

    if (!db.commit()) {
        qDebug() << "settleOrder 提交事务失败：" << db.lastError().text();
        db.rollback();
        return false;
    }

    return true;
}

bool Database::getOrderById(int orderId, OrderInfo *outOrder)
{
    QSqlQuery query(currentThreadDb());
    query.prepare("select id, user_id, pile_id, start_time, end_time, amount, fee, status "
                   "from orders where id = ?");
    query.addBindValue(orderId);
    if (!query.exec() || !query.next()) {
        return false;
    }
    if (outOrder) {
        outOrder->id = query.value(0).toInt();
        outOrder->userId = query.value(1).toInt();
        outOrder->pileId = query.value(2).toInt();
        outOrder->startTime = query.value(3).toString();
        outOrder->endTime = query.value(4).toString();
        outOrder->amount = query.value(5).toDouble();
        outOrder->fee = query.value(6).toDouble();
        outOrder->status = query.value(7).toString();
    }
    return true;
}

QList<OrderInfo> Database::getUserOrders(int userId)
{
    QList<OrderInfo> result;
    QSqlQuery query(currentThreadDb());
    query.prepare("select id, user_id, pile_id, start_time, end_time, amount, fee, status "
                   "from orders where user_id = ? order by id desc");
    query.addBindValue(userId);
    if (!query.exec()) {
        qDebug() << "getUserOrders 失败：" << query.lastError().text();
        return result;
    }
    while (query.next()) {
        OrderInfo o;
        o.id = query.value(0).toInt();
        o.userId = query.value(1).toInt();
        o.pileId = query.value(2).toInt();
        o.startTime = query.value(3).toString();
        o.endTime = query.value(4).toString();
        o.amount = query.value(5).toDouble();
        o.fee = query.value(6).toDouble();
        o.status = query.value(7).toString();
        result.append(o);
    }
    return result;
}

// ================= 销售业绩 =================
double Database::getRevenueToday()
{
    QSqlQuery query("select sum(fee) from orders where status = '已结算' "
                     "and date(end_time) = date('now', 'localtime')", currentThreadDb());
    if (query.next()) {
        return query.value(0).toDouble();
    }
    return 0;
}

double Database::getRevenueThisMonth()
{
    QSqlQuery query("select sum(fee) from orders where status = '已结算' "
                     "and strftime('%Y-%m', end_time) = strftime('%Y-%m', 'now', 'localtime')", currentThreadDb());
    if (query.next()) {
        return query.value(0).toDouble();
    }
    return 0;
}

double Database::getRevenueTotal()
{
    QSqlQuery query("select sum(fee) from orders where status = '已结算'", currentThreadDb());
    if (query.next()) {
        return query.value(0).toDouble();
    }
    return 0;
}

QList<QPair<QString, double>> Database::getRevenueTrend(int days)
{
    QList<QPair<QString, double>> result;
    QSqlQuery query(currentThreadDb());
    query.prepare("select date(end_time) as d, sum(fee) from orders "
                   "where status = '已结算' and end_time >= date('now', ?) "
                   "group by d order by d");
    query.addBindValue(QString("-%1 days").arg(days));
    if (!query.exec()) {
        qDebug() << "getRevenueTrend 失败：" << query.lastError().text();
        return result;
    }
    while (query.next()) {
        result.append(qMakePair(query.value(0).toString(), query.value(1).toDouble()));
    }
    return result;
}
