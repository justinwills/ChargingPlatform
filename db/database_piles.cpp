#include "database.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

// ---------- 充电桩管理 ----------

bool Database::addPile(int stationId, const QString &code, const QString &type,
                       double power, int *outPileId)
{
    const QString normalizedCode = code.trimmed();
    if (stationId <= 0 || normalizedCode.isEmpty()
        || (type != QStringLiteral("快充") && type != QStringLiteral("慢充"))
        || power <= 0) {
        return false;
    }

    QSqlDatabase db = currentThreadDb();
    if (!db.transaction()) {
        qDebug() << "addPile 失败：无法开启事务";
        return false;
    }

    auto rollback = [&db]() {
        db.rollback();
        return false;
    };

    QSqlQuery stationQuery(db);
    stationQuery.prepare("select count(piles.id) from stations "
                         "left join piles on piles.station_id = stations.id "
                         "where stations.id = ? group by stations.id");
    stationQuery.addBindValue(stationId);
    if (!stationQuery.exec() || !stationQuery.next()) {
        qDebug() << "addPile 失败：站点不存在" << stationId;
        return rollback();
    }
    if (stationQuery.value(0).toInt() >= 200) {
        qDebug() << "addPile 失败：站点电桩数已达上限" << stationId;
        return rollback();
    }

    QSqlQuery duplicateQuery(db);
    duplicateQuery.prepare("select id from piles where station_id = ? and lower(code) = lower(?)");
    duplicateQuery.addBindValue(stationId);
    duplicateQuery.addBindValue(normalizedCode);
    if (!duplicateQuery.exec()) {
        qDebug() << "addPile 检查编号失败：" << duplicateQuery.lastError().text();
        return rollback();
    }
    if (duplicateQuery.next()) {
        qDebug() << "addPile 失败：同一站点的电桩编号重复" << normalizedCode;
        return rollback();
    }

    QSqlQuery insertQuery(db);
    insertQuery.prepare("insert into piles(station_id, code, type, power, status) "
                        "values(?, ?, ?, ?, '闲置')");
    insertQuery.addBindValue(stationId);
    insertQuery.addBindValue(normalizedCode);
    insertQuery.addBindValue(type);
    insertQuery.addBindValue(power);
    if (!insertQuery.exec()) {
        qDebug() << "addPile 新增电桩失败：" << insertQuery.lastError().text();
        return rollback();
    }
    const int pileId = insertQuery.lastInsertId().toInt();

    QSqlQuery updateCount(db);
    updateCount.prepare("update stations set pile_count = "
                        "(select count(*) from piles where station_id = ?) where id = ?");
    updateCount.addBindValue(stationId);
    updateCount.addBindValue(stationId);
    if (!updateCount.exec() || updateCount.numRowsAffected() != 1) {
        qDebug() << "addPile 更新站点电桩数失败：" << updateCount.lastError().text();
        return rollback();
    }

    if (!db.commit()) {
        qDebug() << "addPile 提交事务失败：" << db.lastError().text();
        db.rollback();
        return false;
    }
    if (outPileId) {
        *outPileId = pileId;
    }
    return true;
}

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

bool Database::setPileStatus(int pileId, const QString &status)
{
    // 管理员手动控制电桩状态（第18项的扩展）：直接覆盖为"闲置"/"在用"/"故障"三者之一。
    // 与restartPile一样，这里只负责电桩状态本身；如果该电桩当前有进行中订单，
    // 手动改成"闲置"或"故障"不会自动处理订单，由调用方自行提示管理员。
    if (status != QStringLiteral("闲置") && status != QStringLiteral("在用")
        && status != QStringLiteral("故障")) {
        return false;
    }
    QSqlQuery query(currentThreadDb());
    query.prepare("update piles set status = ? where id = ?");
    query.addBindValue(status);
    query.addBindValue(pileId);
    if (!query.exec()) {
        qDebug() << "setPileStatus 失败：" << query.lastError().text();
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
