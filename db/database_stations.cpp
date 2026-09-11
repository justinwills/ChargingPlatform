#include "database.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

static bool insertIdlePiles(QSqlDatabase &db, int stationId, int startIndex, int count)
{
    for (int i = 0; i < count; ++i) {
        QSqlQuery pileQuery(db);
        pileQuery.prepare(
            "insert into piles(station_id, code, type, power, status) "
            "values(?, ?, '快充', 60, '闲置')"
        );

        pileQuery.addBindValue(stationId);
        pileQuery.addBindValue(
            QStringLiteral("S%1-%2")
                .arg(stationId)
                .arg(startIndex + i, 2, 10, QChar('0'))
        );

        if (!pileQuery.exec()) {
            qDebug() << "insertIdlePiles 失败："
                     << pileQuery.lastError().text();
            return false;
        }
    }

    return true;
}

// ---------- 充电站管理 ----------
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
                           double longitude, double latitude, double price,
                           int pileCount)
{
    QSqlDatabase db = currentThreadDb();
    if (!db.transaction()) {
        qDebug() << "addStation 失败：无法开启事务";
        return false;
    }

    auto rollback = [&db]() {
        db.rollback();
        return false;
    };

    QSqlQuery query(db);
    query.prepare("insert into stations(name, address, longitude, latitude, price, pile_count) "
                   "values(?, ?, ?, ?, ?, 0)");
    query.addBindValue(name);
    query.addBindValue(address);
    query.addBindValue(longitude);
    query.addBindValue(latitude);
    query.addBindValue(price);
    if (!query.exec()) {
        qDebug() << "addStation 失败：" << query.lastError().text();
        return rollback();
    }
    const int stationId = query.lastInsertId().toInt();
    if (pileCount > 0 && !insertIdlePiles(db, stationId, 1, pileCount)) {
        return rollback();
    }

    QSqlQuery updateCount(db);
    updateCount.prepare("update stations set pile_count = ? where id = ?");
    updateCount.addBindValue(qMax(0, pileCount));
    updateCount.addBindValue(stationId);
    if (!updateCount.exec()) {
        qDebug() << "addStation 更新电桩数量失败：" << updateCount.lastError().text();
        return rollback();
    }

    if (!db.commit()) {
        qDebug() << "addStation 提交事务失败：" << db.lastError().text();
        db.rollback();
        return false;
    }
    return true;
}

bool Database::setStationPileCount(int stationId, int pileCount)
{
    if (stationId <= 0 || pileCount < 0) {
        return false;
    }

    QSqlDatabase db = currentThreadDb();
    if (!db.transaction()) {
        qDebug() << "setStationPileCount 失败：无法开启事务";
        return false;
    }

    auto rollback = [&db]() {
        db.rollback();
        return false;
    };

    QSqlQuery countQuery(db);
    countQuery.prepare("select count(*) from piles where station_id = ?");
    countQuery.addBindValue(stationId);
    if (!countQuery.exec() || !countQuery.next()) {
        return rollback();
    }

    const int currentCount = countQuery.value(0).toInt();
    QSqlQuery stationQuery(db);
    stationQuery.prepare("select id from stations where id = ?");
    stationQuery.addBindValue(stationId);
    if (!stationQuery.exec() || !stationQuery.next()) {
        return rollback();
    }

    if (pileCount > currentCount) {
        if (!insertIdlePiles(db, stationId, currentCount + 1, pileCount - currentCount)) {
            return rollback();
        }
    } else if (pileCount < currentCount) {
        const int removeCount = currentCount - pileCount;
        QSqlQuery idleQuery(db);
        idleQuery.prepare("select count(*) from piles where station_id = ? and status = '闲置'");
        idleQuery.addBindValue(stationId);
        if (!idleQuery.exec() || !idleQuery.next()
            || idleQuery.value(0).toInt() < removeCount) {
            qDebug() << "setStationPileCount 失败：可删除的闲置电桩不足";
            return rollback();
        }

        QSqlQuery deleteQuery(db);
        deleteQuery.prepare("delete from piles where id in ("
                            "select id from piles where station_id = ? and status = '闲置' "
                            "order by id desc limit ?)");
        deleteQuery.addBindValue(stationId);
        deleteQuery.addBindValue(removeCount);
        if (!deleteQuery.exec() || deleteQuery.numRowsAffected() != removeCount) {
            qDebug() << "setStationPileCount 删除电桩失败：" << deleteQuery.lastError().text();
            return rollback();
        }
    }

    QSqlQuery updateCount(db);
    updateCount.prepare("update stations set pile_count = (select count(*) from piles where station_id = ?) "
                        "where id = ?");
    updateCount.addBindValue(stationId);
    updateCount.addBindValue(stationId);
    if (!updateCount.exec() || updateCount.numRowsAffected() != 1) {
        qDebug() << "setStationPileCount 更新电桩数量失败：" << updateCount.lastError().text();
        return rollback();
    }

    if (!db.commit()) {
        qDebug() << "setStationPileCount 提交事务失败：" << db.lastError().text();
        db.rollback();
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
