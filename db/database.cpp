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

void Database::closeCurrentThreadConnection()
{
    const QString connName = QStringLiteral("conn_%1")
        .arg(reinterpret_cast<quintptr>(QThread::currentThreadId()));
    if (!QSqlDatabase::contains(connName)) {
        return;
    }

    {
        QSqlDatabase db = QSqlDatabase::database(connName, false);
        if (db.isOpen()) {
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(connName);
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
                    "values('东软科技园充电站', '大连市甘井子区东软路1号', 121.5, 38.9, 15, 3)");
        query.exec("insert into stations(name, address, longitude, latitude, price, pile_count) "
                    "values('万达广场充电站', '大连市西岗区万达路2号', 121.6, 38.91, 18, 2)");

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

        query.exec("insert into stations(name, address, longitude, latitude, price, pile_count) "
                "values('北京朝阳充电站', '北京市朝阳区建国路88号', 116.466, 39.908, 18, 3)");
        query.exec("insert into stations(name, address, longitude, latitude, price, pile_count) "
                "values('北京海淀充电站', '北京市海淀区中关村大街1号', 116.316, 39.983, 16, 2)");
        query.exec("insert into stations(name, address, longitude, latitude, price, pile_count) "
                "values('北京丰台充电站', '北京市丰台区南四环西路188号', 116.291, 39.850, 17, 2)");
        query.exec("insert into stations(name, address, longitude, latitude, price, pile_count) "
                "values('北京顺义充电站', '北京市顺义区新顺南大街18号', 116.654, 40.130, 15, 2)");
        query.exec("insert into stations(name, address, longitude, latitude, price, pile_count) "
                "values('北京通州充电站', '北京市通州区新华西街58号', 116.657, 39.909, 19, 2)");

        query.exec("insert into piles(station_id, code, type, power, status) "
                "values(3, 'BJ-C01', '快充', 60, '闲置')");
        query.exec("insert into piles(station_id, code, type, power, status) "
                "values(3, 'BJ-C02', '快充', 60, '在用')");
        query.exec("insert into piles(station_id, code, type, power, status) "
                "values(3, 'BJ-C03', '慢充', 7, '故障')");
        query.exec("insert into piles(station_id, code, type, power, status) "
                "values(4, 'BJ-H01', '快充', 60, '闲置')");
        query.exec("insert into piles(station_id, code, type, power, status) "
                "values(4, 'BJ-H02', '慢充', 7, '闲置')");
        query.exec("insert into piles(station_id, code, type, power, status) "
                "values(5, 'BJ-F01', '快充', 60, '闲置')");
        query.exec("insert into piles(station_id, code, type, power, status) "
                "values(5, 'BJ-F02', '慢充', 7, '闲置')");
        query.exec("insert into piles(station_id, code, type, power, status) "
                "values(6, 'BJ-S01', '快充', 60, '闲置')");
        query.exec("insert into piles(station_id, code, type, power, status) "
                "values(6, 'BJ-S02', '慢充', 7, '闲置')");
        query.exec("insert into piles(station_id, code, type, power, status) "
                "values(7, 'BJ-T01', '快充', 60, '闲置')");
        query.exec("insert into piles(station_id, code, type, power, status) "
                "values(7, 'BJ-T02', '慢充', 7, '闲置')");
    }

    // 1个测试用户
    query.exec("select count(*) from users");
    if (query.next() && query.value(0).toInt() == 0) {
        query.exec("insert into users(phone, nickname, balance) "
                    "values('13800000001', '用户0001', 0.0)");
    }

    // Demo revenue data for the admin report. Keep protocol_test.db deterministic.
    if (s_dbPath == QStringLiteral("charging.db")) {
        query.exec("select count(*) from orders where status = '已结算'");
        if (query.next() && query.value(0).toInt() == 0) {
            for (int dayOffset = 6; dayOffset >= 0; --dayOffset) {
                const QString endTime = QDateTime::currentDateTime()
                    .addDays(-dayOffset).addSecs(-3600)
                    .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
                const int pileId = dayOffset % 2 == 0 ? 1 : 4;
                const double amount = 8.0 + (6 - dayOffset);

                QSqlQuery priceQuery(currentThreadDb());
                priceQuery.prepare("select stations.price from stations "
                                  "join piles on piles.station_id = stations.id "
                                  "where piles.id = ?");
                priceQuery.addBindValue(pileId);
                double stationPrice = 15;
                if (priceQuery.exec() && priceQuery.next()) {
                    stationPrice = priceQuery.value(0).toDouble();
                }

                const double fee = amount * stationPrice;
                query.prepare("insert into orders(user_id, pile_id, start_time, end_time, amount, fee, status) "
                              "values(1, ?, ?, ?, ?, ?, '已结算')");
                query.addBindValue(pileId);
                query.addBindValue(QDateTime::fromString(endTime, QStringLiteral("yyyy-MM-dd HH:mm:ss"))
                                   .addSecs(-1800).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
                query.addBindValue(endTime);
                query.addBindValue(amount);
                query.addBindValue(fee);
                query.exec();
            }
        }
    }

}

// ================= 管理员 =================
