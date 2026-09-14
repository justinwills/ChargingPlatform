#include "requestdispatcher.h"
#include "database.h"

#include <QJsonArray>

// ---------- PC服务器端后台管理：用户/电站/电桩/统计/订单 ----------

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

QJsonObject RequestDispatcher::handleQueryUsers(const QJsonObject &params)
{
    QJsonArray users;
    for (const UserInfo &user : Database::getAllUsers(params.value("phoneKeyword").toString().trimmed())) {
        users.append(QJsonObject{
            {"userId", user.id}, {"phone", user.phone}, {"nickname", user.nickname},
            {"balance", user.balance}, {"status", user.status}, {"createdAt", user.createdAt}
        });
    }
    return ok({{"users", users}, {"count", users.size()}});
}

QJsonObject RequestDispatcher::handleSetUserStatus(const QJsonObject &params)
{
    if (!params.contains("userId") || !params.contains("status")) {
        return fail(1, "缺少userId或status参数");
    }
    const QString status = params.value("status").toString();
    if (status != QStringLiteral("正常") && status != QStringLiteral("冻结")) {
        return fail(1, "status参数无效");
    }
    if (!Database::setUserStatus(params.value("userId").toInt(), status)) {
        return fail(2, "用户不存在或状态更新失败");
    }
    return ok(QJsonObject());
}

QJsonObject RequestDispatcher::handleAdminAddStation(const QJsonObject &params)
{
    const QString name = params.value("name").toString().trimmed();
    const QString address = params.value("address").toString().trimmed();
    if (name.isEmpty() || address.isEmpty()
        || !params.contains("longitude") || !params.contains("latitude")
        || !params.contains("price")) {
        return fail(1, "缺少充电站名称、地址、经纬度或价格");
    }

    const double longitude = params.value("longitude").toDouble();
    const double latitude = params.value("latitude").toDouble();
    const double price = params.value("price").toDouble();
    const int pileCount = params.value("pileCount").toInt(0);
    if (!qIsFinite(longitude) || !qIsFinite(latitude) || !qIsFinite(price)
        || longitude < -180 || longitude > 180
        || latitude < -90 || latitude > 90 || price < 0
        || pileCount < 0 || pileCount > 200) {
        return fail(1, "经纬度或价格无效");
    }
    if (!Database::addStation(name, address, longitude, latitude, price, pileCount)) {
        return fail(2, "新增充电站失败");
    }
    return ok(QJsonObject());
}

QJsonObject RequestDispatcher::handleAdminSetStationPileCount(const QJsonObject &params)
{
    if (!params.contains("stationId") || !params.contains("pileCount")) {
        return fail(1, "缺少stationId或pileCount参数");
    }

    const int stationId = params.value("stationId").toInt();
    const int pileCount = params.value("pileCount").toInt();
    if (stationId <= 0 || pileCount < 0 || pileCount > 200) {
        return fail(1, "站点ID或电桩数量无效");
    }

    if (!Database::setStationPileCount(stationId, pileCount)) {
        return fail(2, "调整电桩数量失败，请确认站点存在，且减少数量时有足够的闲置电桩可删除");
    }
    return ok(QJsonObject());
}

QJsonObject RequestDispatcher::handleAdminAddPile(const QJsonObject &params)
{
    if (!params.contains("stationId") || !params.contains("code")
        || !params.contains("type") || !params.contains("power")) {
        return fail(1, "缺少stationId、code、type或power参数");
    }

    const int stationId = params.value("stationId").toInt();
    const QString code = params.value("code").toString().trimmed();
    const QString type = params.value("type").toString().trimmed();
    const double power = params.value("power").toDouble();
    if (stationId <= 0 || code.isEmpty() || code.size() > 50
        || (type != QStringLiteral("快充") && type != QStringLiteral("慢充"))
        || !qIsFinite(power) || power <= 0 || power > 1000) {
        return fail(1, "站点、电桩编号、类型或功率无效");
    }

    int pileId = -1;
    if (!Database::addPile(stationId, code, type, power, &pileId)) {
        return fail(2, "新增电桩失败，请确认站点存在、电桩数未达200上限，且站内编号未重复");
    }
    return ok({{"pileId", pileId}});
}

QJsonObject RequestDispatcher::handleAdminQueryStations(const QJsonObject &)
{
    QJsonArray stations;
    for (const StationInfo &station : Database::getAllStations()) {
        stations.append(QJsonObject{
            {"stationId", station.id}, {"name", station.name}, {"address", station.address},
            {"longitude", station.longitude}, {"latitude", station.latitude},
            {"price", station.price}, {"pileCount", station.pileCount},
            {"freePileCount", Database::getFreePileCount(station.id)},
            {"onlineRate", Database::getStationOnlineRate(station.id)}
        });
    }
    return ok({{"stations", stations}});
}

QJsonObject RequestDispatcher::handleAdminQueryPiles(const QJsonObject &)
{
    QJsonArray piles;
    for (const PileInfo &pile : Database::getAllPiles()) {
        piles.append(QJsonObject{
            {"pileId", pile.id}, {"stationId", pile.stationId}, {"stationName", pile.stationName},
            {"code", pile.code}, {"type", pile.type}, {"power", pile.power},
            {"status", pile.status}, {"totalSessions", pile.totalSessions},
            {"totalDuration", pile.totalDuration}
        });
    }
    return ok({{"piles", piles}});
}

QJsonObject RequestDispatcher::handleAdminRestartPile(const QJsonObject &params)
{
    if (!params.contains("pileId")) return fail(1, "缺少pileId参数");
    if (!Database::restartPile(params.value("pileId").toInt())) {
        return fail(2, "电桩不存在或重启失败");
    }
    return ok(QJsonObject());
}

QJsonObject RequestDispatcher::handleAdminSetPileStatus(const QJsonObject &params)
{
    if (!params.contains("pileId") || !params.contains("status")) {
        return fail(1, "缺少pileId或status参数");
    }
    const int pileId = params.value("pileId").toInt();
    const QString status = params.value("status").toString().trimmed();
    if (status != QStringLiteral("闲置") && status != QStringLiteral("在用")
        && status != QStringLiteral("故障")) {
        return fail(1, "status参数无效，需为\"闲置\"、\"在用\"或\"故障\"");
    }
    if (!Database::setPileStatus(pileId, status)) {
        return fail(2, "电桩不存在或设置状态失败");
    }
    return ok(QJsonObject());
}

QJsonObject RequestDispatcher::handleAdminStats(const QJsonObject &params)
{
    const int requestedDays = params.value("days").toInt(7);
    const int days = requestedDays == 30 ? 30 : 7;
    QJsonObject pileStatus;
    const QMap<QString, int> stats = Database::getPileStatusStats();
    for (auto it = stats.cbegin(); it != stats.cend(); ++it) {
        pileStatus[it.key()] = it.value();
    }
    QJsonArray trend;
    for (const auto &point : Database::getRevenueTrend(days)) {
        trend.append(QJsonObject{{"date", point.first}, {"revenue", point.second}});
    }
    return ok({
        {"revenueToday", Database::getRevenueToday()},
        {"revenueThisMonth", Database::getRevenueThisMonth()},
        {"revenueTotal", Database::getRevenueTotal()},
        {"pileStatus", pileStatus}, {"trendDays", days}, {"revenueTrend", trend}
    });
}

QJsonObject RequestDispatcher::handleAdminOrders(const QJsonObject &params)
{
    QJsonArray orders;
    for (const OrderInfo &order : Database::getAllOrders(
             params.value("phoneKeyword").toString(),
             params.value("stationId").toInt(-1),
             params.value("fromDate").toString(),
             params.value("toDate").toString(),
             params.value("status").toString())) {
        orders.append(QJsonObject{
            {"orderId", order.id}, {"userId", order.userId}, {"phone", order.userPhone},
            {"pileId", order.pileId}, {"stationId", order.stationId},
            {"stationName", order.stationName},
            {"startTime", order.startTime}, {"endTime", order.endTime},
            {"amount", order.amount}, {"fee", order.fee}, {"status", order.status}
        });
    }
    QJsonArray trend;
    for (const auto &point : Database::getRevenueTrend(7)) {
        trend.append(QJsonObject{{"date", point.first}, {"revenue", point.second}});
    }
    return ok({{"orders", orders}, {"revenueTrend", trend}});
}

