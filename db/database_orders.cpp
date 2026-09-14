#include "database.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QDebug>
#include <QStringList>

// ---------- 充电流程 / 订单 ----------

bool Database::hasOngoingOrder(int userId, int *outOrderId)
{
    QSqlQuery query(currentThreadDb());
    query.prepare("select id from orders where user_id = ? "
                  "and status in ('充电中', '待结算') order by id limit 1");
    query.addBindValue(userId);
    if (!query.exec() || !query.next()) {
        return false;
    }
    if (outOrderId) {
        *outOrderId = query.value(0).toInt();
    }
    return true;
}

bool Database::markOrderPendingSettlement(int orderId, double amount, double fee)
{
    QSqlDatabase db = currentThreadDb();
    if (!db.transaction()) {
        qDebug() << "markOrderPendingSettlement 失败：无法开启事务";
        return false;
    }

    auto rollback = [&db]() {
        db.rollback();
        return false;
    };

    QSqlQuery orderQuery(db);
    orderQuery.prepare("select pile_id, start_time, status from orders where id = ?");
    orderQuery.addBindValue(orderId);
    if (!orderQuery.exec() || !orderQuery.next()) {
        return rollback();
    }

    const int pileId = orderQuery.value(0).toInt();
    const QString startTime = orderQuery.value(1).toString();
    const QString status = orderQuery.value(2).toString();
    if (status != QStringLiteral("充电中") && status != QStringLiteral("待结算")) {
        return rollback();
    }

    const int durationMinutes = qMax(0, static_cast<int>(
        QDateTime::fromString(startTime, QStringLiteral("yyyy-MM-dd HH:mm:ss"))
            .secsTo(QDateTime::currentDateTime()) / 60));
    const QString now = QDateTime::currentDateTime().toString(
        QStringLiteral("yyyy-MM-dd HH:mm:ss"));

    QSqlQuery updateOrder(db);
    updateOrder.prepare("update orders set end_time = ?, amount = ?, fee = ?, "
                        "status = '待结算' where id = ? "
                        "and status in ('充电中', '待结算')");
    updateOrder.addBindValue(now);
    updateOrder.addBindValue(amount);
    updateOrder.addBindValue(fee);
    updateOrder.addBindValue(orderId);
    if (!updateOrder.exec() || updateOrder.numRowsAffected() != 1) {
        return rollback();
    }

    if (status == QStringLiteral("充电中")) {
        QSqlQuery freePile(db);
        freePile.prepare("update piles set status = '闲置', "
                         "total_sessions = total_sessions + 1, "
                         "total_duration = total_duration + ? "
                         "where id = ? and status = '在用'");
        freePile.addBindValue(durationMinutes);
        freePile.addBindValue(pileId);
        if (!freePile.exec() || freePile.numRowsAffected() != 1) {
            return rollback();
        }
    }

    if (!db.commit()) {
        qDebug() << "markOrderPendingSettlement 提交事务失败：" << db.lastError().text();
        db.rollback();
        return false;
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

bool Database::settleOrder(int orderId, double amount, double fee, double *outBalance)
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
    QString status = orderQuery.value(3).toString();
    if (status != "待结算") {
        qDebug() << "settleOrder 失败：订单当前不是待结算状态";
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
    deductBalance.prepare("update users set balance = balance - ? "
                          "where id = ? and balance >= ?");
    deductBalance.addBindValue(fee);
    deductBalance.addBindValue(userId);
    deductBalance.addBindValue(fee);
    if (!deductBalance.exec() || deductBalance.numRowsAffected() != 1) {
        qDebug() << "settleOrder 扣款失败：余额不足或用户不存在"
                 << deductBalance.lastError().text();
        return rollback();
    }

    if (!db.commit()) {
        qDebug() << "settleOrder 提交事务失败：" << db.lastError().text();
        db.rollback();
        return false;
    }

    if (outBalance) {
        *outBalance = balance - fee;
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

QList<OrderInfo> Database::getAllOrders(const QString &phoneKeyword, int stationId,
                                        const QString &fromDate, const QString &toDate,
                                        const QString &status)
{
    QList<OrderInfo> result;
    QString sql = "select orders.id, orders.user_id, users.phone, orders.pile_id, "
                  "stations.id, stations.name, orders.start_time, orders.end_time, "
                  "orders.amount, orders.fee, orders.status "
                  "from orders join users on orders.user_id = users.id "
                  "join piles on orders.pile_id = piles.id "
                  "join stations on piles.station_id = stations.id where 1 = 1";
    QSqlQuery query(currentThreadDb());
    if (!phoneKeyword.trimmed().isEmpty()) {
        sql += " and users.phone like ?";
    }
    if (stationId > 0) {
        sql += " and stations.id = ?";
    }
    if (!fromDate.trimmed().isEmpty()) {
        sql += " and date(coalesce(orders.end_time, orders.start_time)) >= date(?)";
    }
    if (!toDate.trimmed().isEmpty()) {
        sql += " and date(coalesce(orders.end_time, orders.start_time)) <= date(?)";
    }
    if (!status.trimmed().isEmpty()) {
        sql += " and orders.status = ?";
    }
    sql += " order by orders.id desc";
    query.prepare(sql);
    if (!phoneKeyword.trimmed().isEmpty()) {
        query.addBindValue(QStringLiteral("%1%").arg(phoneKeyword.trimmed()));
    }
    if (stationId > 0) query.addBindValue(stationId);
    if (!fromDate.trimmed().isEmpty()) query.addBindValue(fromDate.trimmed());
    if (!toDate.trimmed().isEmpty()) query.addBindValue(toDate.trimmed());
    if (!status.trimmed().isEmpty()) query.addBindValue(status.trimmed());
    if (!query.exec()) {
        qDebug() << "getAllOrders 失败：" << query.lastError().text();
        return result;
    }
    while (query.next()) {
        OrderInfo order;
        order.id = query.value(0).toInt();
        order.userId = query.value(1).toInt();
        order.userPhone = query.value(2).toString();
        order.pileId = query.value(3).toInt();
        order.stationId = query.value(4).toInt();
        order.stationName = query.value(5).toString();
        order.startTime = query.value(6).toString();
        order.endTime = query.value(7).toString();
        order.amount = query.value(8).toDouble();
        order.fee = query.value(9).toDouble();
        order.status = query.value(10).toString();
        result.append(order);
    }
    return result;
}

// ================= 销售业绩 =================
