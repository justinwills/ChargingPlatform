#include "requestdispatcher.h"
#include "database.h"
#include <QJsonArray>

QJsonObject RequestDispatcher::ok(const QJsonObject &data)
{
    QJsonObject resp;
    resp["code"] = 0;
    resp["msg"] = "ok";
    resp["data"] = data;
    return resp;
}

QJsonObject RequestDispatcher::fail(int code, const QString &msg)
{
    QJsonObject resp;
    resp["code"] = code;
    resp["msg"] = msg;
    resp["data"] = QJsonObject();
    return resp;
}

QJsonObject RequestDispatcher::handle(const QJsonObject &request)
{
    QString action = request.value("action").toString();
    QJsonObject params = request.value("params").toObject();

    if (action == "login")              return handleLogin(params);
    if (action == "admin_login")        return handleAdminLogin(params);
    if (action == "query_stations")     return handleQueryStations(params);
    if (action == "query_pile_detail")  return handleQueryPileDetail(params);
    if (action == "start_charging")     return handleStartCharging(params);
    if (action == "query_order")        return handleQueryOrder(params);
    if (action == "settle_order")       return handleSettleOrder(params);

    return fail(1, QStringLiteral("未知的action：%1").arg(action));
}

QJsonObject RequestDispatcher::handleLogin(const QJsonObject &params)
{
    QString phone = params.value("phone").toString();
    if (phone.isEmpty()) return fail(1, "缺少phone参数");

    UserInfo user;
    if (!Database::phoneLogin(phone, &user)) {
        return fail(3, "数据库操作失败");
    }
    QJsonObject data;
    data["userId"] = user.id;
    data["phone"] = user.phone;
    data["nickname"] = user.nickname;
    data["avatarPath"] = user.avatarPath;
    data["balance"] = user.balance;
    data["status"] = user.status;
    data["createdAt"] = user.createdAt;
    return ok(data);
}

QJsonObject RequestDispatcher::handleAdminLogin(const QJsonObject &params)
{
    QString username = params.value("username").toString();
    QString password = params.value("password").toString();
    if (username.isEmpty() || password.isEmpty()) return fail(1, "缺少username或password参数");

    if (!Database::checkAdminLogin(username, password)) {
        return fail(2, "账号或密码错误");
    }
    return ok(QJsonObject());
}

QJsonObject RequestDispatcher::handleQueryStations(const QJsonObject & /*params*/)
{
    QJsonArray arr;
    const auto stations = Database::getAllStations();
    for (const auto &s : stations) {
        QJsonObject o;
        o["stationId"] = s.id;
        o["name"] = s.name;
        o["address"] = s.address;
        o["longitude"] = s.longitude;
        o["latitude"] = s.latitude;
        o["price"] = s.price;
        o["pileCount"] = s.pileCount;
        o["freePileCount"] = Database::getFreePileCount(s.id);
        o["onlineRate"] = Database::getStationOnlineRate(s.id);
        arr.append(o);
    }
    QJsonObject data;
    data["stations"] = arr;
    return ok(data);
}

QJsonObject RequestDispatcher::handleQueryPileDetail(const QJsonObject &params)
{
    if (!params.contains("pileId")) return fail(1, "缺少pileId参数");
    int pileId = params.value("pileId").toInt();

    PileInfo pile;
    if (!Database::getPileById(pileId, &pile)) {
        return fail(2, "找不到该电桩");
    }
    QJsonObject data;
    data["pileId"] = pile.id;
    data["stationId"] = pile.stationId;
    data["stationName"] = pile.stationName;
    data["code"] = pile.code;
    data["type"] = pile.type;
    data["power"] = pile.power;
    data["status"] = pile.status;
    data["totalSessions"] = pile.totalSessions;
    data["totalDuration"] = pile.totalDuration;
    return ok(data);
}

QJsonObject RequestDispatcher::handleStartCharging(const QJsonObject &params)
{
    if (!params.contains("userId") || !params.contains("pileId")) {
        return fail(1, "缺少userId或pileId参数");
    }
    int userId = params.value("userId").toInt();
    int pileId = params.value("pileId").toInt();

    int existingOrderId = -1;
    if (Database::hasOngoingOrder(userId, &existingOrderId)) {
        return fail(2, QStringLiteral("您有未结算的充电订单(订单号%1)，请先完成结算").arg(existingOrderId));
    }

    int orderId = -1;
    if (!Database::startCharging(userId, pileId, &orderId)) {
        return fail(2, "电桩当前不是闲置状态，无法发起充电");
    }
    QJsonObject data;
    data["orderId"] = orderId;
    return ok(data);
}

QJsonObject RequestDispatcher::handleQueryOrder(const QJsonObject &params)
{
    if (!params.contains("orderId")) return fail(1, "缺少orderId参数");
    int orderId = params.value("orderId").toInt();

    OrderInfo order;
    if (!Database::getOrderById(orderId, &order)) {
        return fail(2, "找不到该订单");
    }
    QJsonObject data;
    data["orderId"] = order.id;
    data["userId"] = order.userId;
    data["pileId"] = order.pileId;
    data["startTime"] = order.startTime;
    data["endTime"] = order.endTime;
    data["amount"] = order.amount;
    data["fee"] = order.fee;
    data["status"] = order.status;
    return ok(data);
}

QJsonObject RequestDispatcher::handleSettleOrder(const QJsonObject &params)
{
    if (!params.contains("orderId") || !params.contains("amount") || !params.contains("fee")) {
        return fail(1, "缺少orderId/amount/fee参数");
    }
    int orderId = params.value("orderId").toInt();
    double amount = params.value("amount").toDouble();
    double fee = params.value("fee").toDouble();

    if (!Database::settleOrder(orderId, amount, fee)) {
        return fail(2, "结算失败：订单不存在、已结算过、或余额不足");
    }
    return ok(QJsonObject());
}
