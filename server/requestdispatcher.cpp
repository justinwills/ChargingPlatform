#include "requestdispatcher.h"
#include "database.h"
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrlQuery>
#include <QDateTime>
#include <QtGlobal>
#include <algorithm>
#include <cmath>

namespace {

struct GeocodedLocation {
    double latitude = 0;
    double longitude = 0;
};

bool geocodeAddress(const QString &address, GeocodedLocation *location, QString *error)
{
    const QString key = qEnvironmentVariable("TENCENT_MAP_KEY").trimmed();
    if (key.isEmpty()) {
        if (error) *error = QStringLiteral("未配置TENCENT_MAP_KEY");
        return false;
    }

    QUrl url(QStringLiteral("https://apis.map.qq.com/ws/geocoder/v1/"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("address"), address);
    query.addQueryItem(QStringLiteral("key"), key);
    url.setQuery(query);

    QNetworkAccessManager manager;
    QNetworkReply *reply = manager.get(QNetworkRequest(url));
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    timeout.setInterval(5000);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start();
    loop.exec();

    if (!reply->isFinished()) {
        reply->abort();
        if (error) *error = QStringLiteral("地图服务请求超时");
        reply->deleteLater();
        return false;
    }
    if (reply->error() != QNetworkReply::NoError) {
        if (error) *error = reply->errorString();
        reply->deleteLater();
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(reply->readAll(), &parseError);
    reply->deleteLater();
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error) *error = QStringLiteral("地图服务返回了无效数据");
        return false;
    }

    const QJsonObject response = document.object();
    if (response.value("status").toInt(-1) != 0) {
        if (error) *error = response.value("message").toString();
        return false;
    }
    const QJsonObject result = response.value("result").toObject();
    const QJsonObject point = result.value("location").toObject();
    if (!point.contains("lat") || !point.contains("lng")) {
        if (error) *error = QStringLiteral("地址未解析出经纬度");
        return false;
    }

    location->latitude = point.value("lat").toDouble();
    location->longitude = point.value("lng").toDouble();
    return true;
}

double distanceKm(double latitude1, double longitude1,
                  double latitude2, double longitude2)
{
    constexpr double earthRadiusKm = 6371.0;
    constexpr double pi = 3.14159265358979323846;
    const auto radians = [pi](double value) { return value * pi / 180.0; };
    const double lat1 = radians(latitude1);
    const double lat2 = radians(latitude2);
    const double deltaLat = lat2 - lat1;
    const double deltaLon = radians(longitude2 - longitude1);
    const double sineLat = std::sin(deltaLat / 2.0);
    const double sineLon = std::sin(deltaLon / 2.0);
    const double a = sineLat * sineLat
        + std::cos(lat1) * std::cos(lat2) * sineLon * sineLon;
    return earthRadiusKm * 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));
}

bool requestTencentRoute(const QString &mode,
                         double fromLatitude, double fromLongitude,
                         double toLatitude, double toLongitude,
                         QJsonObject *route, QString *error)
{
    const QString key = qEnvironmentVariable("TENCENT_MAP_KEY").trimmed();
    if (key.isEmpty()) {
        if (error) *error = QStringLiteral("未配置TENCENT_MAP_KEY");
        return false;
    }

    const QString service = mode == QStringLiteral("walking")
        ? QStringLiteral("walking")
        : mode == QStringLiteral("transit")
            ? QStringLiteral("transit")
            : QStringLiteral("driving");
    QUrl url(QStringLiteral("https://apis.map.qq.com/ws/direction/v1/%1/").arg(service));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("from"),
                       QStringLiteral("%1,%2").arg(fromLatitude, 0, 'f', 7)
                                               .arg(fromLongitude, 0, 'f', 7));
    query.addQueryItem(QStringLiteral("to"),
                       QStringLiteral("%1,%2").arg(toLatitude, 0, 'f', 7)
                                             .arg(toLongitude, 0, 'f', 7));
    query.addQueryItem(QStringLiteral("key"), key);
    if (service == QStringLiteral("driving") || service == QStringLiteral("transit")) {
        query.addQueryItem(QStringLiteral("policy"), QStringLiteral("LEAST_TIME"));
    }
    url.setQuery(query);

    QNetworkAccessManager manager;
    QNetworkReply *reply = manager.get(QNetworkRequest(url));
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    timeout.setInterval(8000);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start();
    loop.exec();

    if (!reply->isFinished()) {
        reply->abort();
        if (error) *error = QStringLiteral("路线规划请求超时");
        reply->deleteLater();
        return false;
    }
    if (reply->error() != QNetworkReply::NoError) {
        if (error) *error = reply->errorString();
        reply->deleteLater();
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(reply->readAll(), &parseError);
    reply->deleteLater();
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error) *error = QStringLiteral("路线服务返回了无效数据");
        return false;
    }

    const QJsonObject response = document.object();
    if (response.value(QStringLiteral("status")).toInt(-1) != 0) {
        if (error) *error = response.value(QStringLiteral("message")).toString();
        return false;
    }
    const QJsonArray routes = response.value(QStringLiteral("result"))
                                  .toObject().value(QStringLiteral("routes")).toArray();
    if (routes.isEmpty() || !routes.first().isObject()) {
        if (error) *error = QStringLiteral("地图服务没有找到可用路线");
        return false;
    }

    *route = routes.first().toObject();
    return true;
}

}

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
    if (action == "update_user_profile") return handleUpdateUserProfile(params);
    if (action == "recharge_balance")   return handleRechargeBalance(params);

    if (action == "admin_login")        return handleAdminLogin(params);
    if (action == "query_users")        return handleQueryUsers(params);
    if (action == "set_user_status")    return handleSetUserStatus(params);
    if (action == "admin_add_station") return handleAdminAddStation(params);
    if (action == "admin_query_stations") return handleAdminQueryStations(params);
    if (action == "admin_query_piles")  return handleAdminQueryPiles(params);
    if (action == "admin_restart_pile") return handleAdminRestartPile(params);
    if (action == "admin_stats")        return handleAdminStats(params);
    if (action == "admin_orders")       return handleAdminOrders(params);
    if (action == "query_stations")     return handleQueryStations(params);
    if (action == "query_station_detail") return handleQueryStationDetail(params);
    if (action == "plan_route" || action == "route_planning"
        || action == "start_navigation" || action == "switch_navigation_mode") {
        return handlePlanRoute(params);
    }
    if (action == "end_navigation")     return handleEndNavigation(params);
    if (action == "query_pile_detail")  return handleQueryPileDetail(params);
    if (action == "start_charging")     return handleStartCharging(params);
    if (action == "prepare_settlement") return handlePrepareSettlement(params);
    if (action == "query_order")        return handleQueryOrder(params);
    if (action == "settle_order")       return handleSettleOrder(params);

    return fail(1, QStringLiteral("未知的action：%1").arg(action));
}

QJsonObject RequestDispatcher::handleLogin(const QJsonObject &params)
{
    QString phone = params.value("phone").toString().trimmed();

    // 虽然客户端已验证但是为了避免错误，服务器端在验证多一次
    static const QRegularExpression phoneRegex("^1\\d{10}$");
    if (!phoneRegex.match(phone).hasMatch()) {
        return fail(1, "手机号格式错误，应为11位手机号");
    }

    UserInfo user;
    if (!Database::phoneLogin(phone, &user)) {
        return fail(3, "数据库操作失败");
    }

    if(user.status == "冻结"){
        return fail(2,"该账号已被冻结，无法登录");
    }

    QJsonObject data;
    data["userId"] = user.id;
    data["phone"] = user.phone;
    data["nickname"] = user.nickname;
    data["avatarPath"] = user.avatarPath;
    data["balance"] = user.balance;
    data["status"] = user.status;
    data["createdAt"] = user.createdAt;

    int ongoingOrderId = -1;
    if (Database::hasOngoingOrder(user.id, &ongoingOrderId)) {
        data["ongoingOrderId"] = ongoingOrderId;
    }
    return ok(data);
}

QJsonObject RequestDispatcher::handleUpdateUserProfile(const QJsonObject &params)
{
    if (!params.contains("userId")){
        return fail(1,"缺少userId参数");
    }

    int userId = params.value("userId").toInt();
    QString nickname = params.value("nickname").toString().trimmed();
    QString avatarPath = params.value("avatarPath").toString();

    if (userId <= 0 || nickname.isEmpty()) {
        return fail(1, "用户id或昵称无效");
    }
    if (!Database::updateUserProfile(userId, nickname, avatarPath)) {
        return fail(2, "更新用户信息失败");
    }

    // 更新后重新查询，向客户端返回数据库中的最新记录
    UserInfo user;
    if (!Database::getUserById(userId, &user)) {
        return fail(3, "读取更新后的用户信息失败");
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

QJsonObject RequestDispatcher::handleRechargeBalance(const QJsonObject &params)
{
    if(!params.contains("userId") || !params.contains("amount")){
        return fail(1,"缺少userId或amount参数");
    }

    int userId = params.value("userId").toInt();
    double amount = params.value("amount").toDouble();

    if(userId <= 0){
        return fail(1,"用户ID不存在");
    }

    if(amount <= 0){
        return fail(1,"充值金额必须大于0");
    }

    if (!Database::rechargeBalance(userId, amount)) {
        return fail(2, "充值失败");
    }

    UserInfo user;
    if (!Database::getUserById(userId, &user)) {
        return fail(3, "读取充值后的用户信息失败");
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
    if (!qIsFinite(longitude) || !qIsFinite(latitude) || !qIsFinite(price)
        || longitude < -180 || longitude > 180
        || latitude < -90 || latitude > 90 || price < 0) {
        return fail(1, "经纬度或价格无效");
    }
    if (!Database::addStation(name, address, longitude, latitude, price)) {
        return fail(2, "新增充电站失败");
    }
    return ok(QJsonObject());
}

QJsonObject RequestDispatcher::handleAdminQueryStations(const QJsonObject &)
{
    QJsonArray stations;
    for (const StationInfo &station : Database::getAllStations()) {
        stations.append(QJsonObject{
            {"stationId", station.id}, {"name", station.name}, {"address", station.address},
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

QJsonObject RequestDispatcher::handleAdminStats(const QJsonObject &)
{
    QJsonObject pileStatus;
    const QMap<QString, int> stats = Database::getPileStatusStats();
    for (auto it = stats.cbegin(); it != stats.cend(); ++it) {
        pileStatus[it.key()] = it.value();
    }
    QJsonArray trend;
    for (const auto &point : Database::getRevenueTrend(7)) {
        trend.append(QJsonObject{{"date", point.first}, {"revenue", point.second}});
    }
    return ok({
        {"revenueToday", Database::getRevenueToday()},
        {"revenueThisMonth", Database::getRevenueThisMonth()},
        {"revenueTotal", Database::getRevenueTotal()},
        {"pileStatus", pileStatus}, {"revenueTrend", trend}
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

QJsonObject RequestDispatcher::handleQueryStations(const QJsonObject &params)
{
    const QString addressKeyword = params.value("address").toString().trimmed();
    GeocodedLocation location;
    bool hasLocation = false;

    if (params.contains("latitude") && params.contains("longitude")) {
        location.latitude = params.value("latitude").toDouble();
        location.longitude = params.value("longitude").toDouble();
        if (!qIsFinite(location.latitude) || !qIsFinite(location.longitude)
            || location.latitude < -90 || location.latitude > 90
            || location.longitude < -180 || location.longitude > 180) {
            return fail(1, "latitude或longitude参数无效");
        }
        hasLocation = true;
    } else if (!addressKeyword.isEmpty()) {
        QString geocodeError;
        hasLocation = geocodeAddress(addressKeyword, &location, &geocodeError);
        if (!hasLocation && !qEnvironmentVariable("TENCENT_MAP_KEY").isEmpty()) {
            return fail(3, QStringLiteral("地址解析失败：%1").arg(geocodeError));
        }
    } else {
        return fail(1, "需要latitude/longitude或address参数");
    }

    struct StationResult {
        StationInfo station;
        double distance = -1;
    };
    QList<StationResult> results;
    for (const auto &s : Database::getAllStations()) {
        if (hasLocation) {
            results.append({s, distanceKm(location.latitude, location.longitude,
                                          s.latitude, s.longitude)});
        } else if (s.name.contains(addressKeyword, Qt::CaseInsensitive)
                   || s.address.contains(addressKeyword, Qt::CaseInsensitive)) {
            results.append({s, -1});
        }
    }

    if (hasLocation) {
        std::sort(results.begin(), results.end(),
                  [](const StationResult &left, const StationResult &right) {
                      return left.distance < right.distance;
                  });
    }

    QJsonArray arr;
    const int resultCount = qMin(results.size(), 5);
    for (int index = 0; index < resultCount; ++index) {
        const StationResult &result = results.at(index);
        const auto &s = result.station;
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
        if (result.distance >= 0) o["distanceKm"] = result.distance;
        arr.append(o);
    }
    QJsonObject data;
    data["stations"] = arr;
    data["sortedByDistance"] = hasLocation;
    data["count"] = arr.size();
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

    StationInfo station;
    if (!Database::getStationById(pile.stationId, &station)) {
        return fail(3, "找不到该电桩所属充电站");
    }

    QJsonObject data;
    data["pileId"] = pile.id;
    data["stationId"] = pile.stationId;
    data["stationName"] = pile.stationName;
    data["stationAddress"] = station.address;
    data["price"] = station.price;
    data["code"] = pile.code;
    data["type"] = pile.type;
    data["power"] = pile.power;
    data["status"] = pile.status;
    data["totalSessions"] = pile.totalSessions;
    data["totalDuration"] = pile.totalDuration;

    QJsonArray piles;
    for (const PileInfo &stationPile : Database::getPilesByStation(pile.stationId)) {
        QJsonObject item;
        item["pileId"] = stationPile.id;
        item["code"] = stationPile.code;
        item["type"] = stationPile.type;
        item["power"] = stationPile.power;
        item["status"] = stationPile.status;
        item["totalSessions"] = stationPile.totalSessions;
        item["totalDuration"] = stationPile.totalDuration;
        piles.append(item);
    }
    data["piles"] = piles;
    return ok(data);
}

QJsonObject RequestDispatcher::handleQueryStationDetail(const QJsonObject &params)
{
    if (!params.contains("stationId")) return fail(1, "缺少stationId参数");

    const int stationId = params.value("stationId").toInt();
    StationInfo station;
    if (!Database::getStationById(stationId, &station)) {
        return fail(2, "找不到该充电站");
    }

    QJsonObject data;
    data["stationId"] = station.id;
    data["name"] = station.name;
    data["address"] = station.address;
    data["longitude"] = station.longitude;
    data["latitude"] = station.latitude;
    data["price"] = station.price;
    data["pileCount"] = station.pileCount;
    data["freePileCount"] = Database::getFreePileCount(station.id);
    data["onlineRate"] = Database::getStationOnlineRate(station.id);

    QJsonArray piles;
    for (const PileInfo &pile : Database::getPilesByStation(station.id)) {
        QJsonObject item;
        item["pileId"] = pile.id;
        item["code"] = pile.code;
        item["type"] = pile.type;
        item["power"] = pile.power;
        item["status"] = pile.status;
        item["totalSessions"] = pile.totalSessions;
        item["totalDuration"] = pile.totalDuration;
        piles.append(item);
    }
    data["piles"] = piles;
    return ok(data);
}

QJsonObject RequestDispatcher::handlePlanRoute(const QJsonObject &params)
{
    QString mode = params.value(QStringLiteral("mode")).toString().trimmed().toLower();
    if (mode == QStringLiteral("驾车") || mode == QStringLiteral("car")) {
        mode = QStringLiteral("driving");
    } else if (mode == QStringLiteral("步行") || mode == QStringLiteral("walk")) {
        mode = QStringLiteral("walking");
    } else if (mode == QStringLiteral("公交") || mode == QStringLiteral("公交车")
               || mode == QStringLiteral("bus") || mode == QStringLiteral("public_transit")) {
        mode = QStringLiteral("transit");
    }
    if (mode.isEmpty()) mode = QStringLiteral("driving");
    if (mode != QStringLiteral("driving") && mode != QStringLiteral("walking")
        && mode != QStringLiteral("transit")) {
        return fail(1, QStringLiteral("mode必须是driving、walking或transit"));
    }

    auto readCoordinate = [&params](const QStringList &keys, double *value) {
        for (const QString &key : keys) {
            if (params.contains(key)) {
                *value = params.value(key).toDouble();
                return qIsFinite(*value);
            }
        }
        return false;
    };

    double fromLatitude = 0;
    double fromLongitude = 0;
    if (!readCoordinate({QStringLiteral("fromLatitude"), QStringLiteral("originLatitude")}, &fromLatitude)
        || !readCoordinate({QStringLiteral("fromLongitude"), QStringLiteral("originLongitude")}, &fromLongitude)
        || fromLatitude < -90 || fromLatitude > 90
        || fromLongitude < -180 || fromLongitude > 180) {
        return fail(1, QStringLiteral("缺少或无效的起点经纬度"));
    }

    double toLatitude = 0;
    double toLongitude = 0;
    const int stationId = params.value(QStringLiteral("stationId")).toInt(-1);
    StationInfo station;
    if (stationId > 0) {
        if (!Database::getStationById(stationId, &station)) {
            return fail(2, QStringLiteral("找不到目标充电站"));
        }
        toLatitude = station.latitude;
        toLongitude = station.longitude;
    } else if (!readCoordinate({QStringLiteral("toLatitude"), QStringLiteral("destinationLatitude")}, &toLatitude)
               || !readCoordinate({QStringLiteral("toLongitude"), QStringLiteral("destinationLongitude")}, &toLongitude)) {
        return fail(1, QStringLiteral("需要stationId或目标经纬度"));
    }
    if (toLatitude < -90 || toLatitude > 90 || toLongitude < -180 || toLongitude > 180) {
        return fail(1, QStringLiteral("目标经纬度无效"));
    }

    QJsonObject route;
    QString routeError;
    if (!requestTencentRoute(mode, fromLatitude, fromLongitude,
                             toLatitude, toLongitude, &route, &routeError)) {
        return fail(3, QStringLiteral("路线规划失败：%1").arg(routeError));
    }

    const int distanceMeters = route.value(QStringLiteral("distance")).toInt();
    const int durationSeconds = route.value(QStringLiteral("duration")).toInt();
    QJsonObject data;
    data[QStringLiteral("mode")] = mode;
    data[QStringLiteral("fromLatitude")] = fromLatitude;
    data[QStringLiteral("fromLongitude")] = fromLongitude;
    data[QStringLiteral("toLatitude")] = toLatitude;
    data[QStringLiteral("toLongitude")] = toLongitude;
    data[QStringLiteral("distanceMeters")] = distanceMeters;
    data[QStringLiteral("distanceKm")] = distanceMeters / 1000.0;
    data[QStringLiteral("durationSeconds")] = durationSeconds;
    data[QStringLiteral("durationMinutes")] = qCeil(durationSeconds / 60.0);
    data[QStringLiteral("polyline")] = route.value(QStringLiteral("polyline"));
    if (stationId > 0) {
        data[QStringLiteral("stationId")] = station.id;
        data[QStringLiteral("stationName")] = station.name;
        data[QStringLiteral("stationAddress")] = station.address;
    }

    QJsonArray steps;
    for (const QJsonValue &value : route.value(QStringLiteral("steps")).toArray()) {
        const QJsonObject source = value.toObject();
        steps.append(QJsonObject{
            {QStringLiteral("instruction"), source.value(QStringLiteral("instruction"))},
            {QStringLiteral("roadName"), source.value(QStringLiteral("road_name"))},
            {QStringLiteral("distanceMeters"), source.value(QStringLiteral("distance"))},
            {QStringLiteral("durationSeconds"), source.value(QStringLiteral("duration"))}
        });
    }
    data[QStringLiteral("steps")] = steps;
    return ok(data);
}

QJsonObject RequestDispatcher::handleEndNavigation(const QJsonObject &)
{
    return ok({{QStringLiteral("ended"), true}});
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

QJsonObject RequestDispatcher::handlePrepareSettlement(const QJsonObject &params)
{
    if (!params.contains("orderId") || !params.contains("amount") || !params.contains("fee")) {
        return fail(1, "缺少orderId/amount/fee参数");
    }

    const int orderId = params.value("orderId").toInt();
    const double amount = params.value("amount").toDouble();
    const double fee = params.value("fee").toDouble();
    if (orderId <= 0 || amount < 0 || fee < 0
        || !qIsFinite(amount) || !qIsFinite(fee)) {
        return fail(1, "orderId、amount或fee参数无效");
    }

    if (!Database::markOrderPendingSettlement(orderId, amount, fee)) {
        return fail(2, "订单不存在、已结束或无法进入待结算状态");
    }
    return ok({{"orderId", orderId}, {"amount", amount},
               {"fee", fee}, {"status", QStringLiteral("待结算")}});
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

    if (order.status == QStringLiteral("充电中")) {
        PileInfo pile;
        StationInfo station;
        if (Database::getPileById(order.pileId, &pile)
            && Database::getStationById(pile.stationId, &station)) {
            const auto startTime = QDateTime::fromString(
                order.startTime, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
            const int durationMinutes = qMax(0, static_cast<int>(
                startTime.secsTo(QDateTime::currentDateTime()) / 60));
            const double estimatedAmount = pile.power * durationMinutes / 60.0;
            const double estimatedFee = estimatedAmount * station.price;
            data["durationMinutes"] = durationMinutes;
            data["estimatedAmount"] = estimatedAmount;
            data["estimatedFee"] = estimatedFee;
        }
    }
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

    if (orderId <= 0 || amount < 0 || fee < 0
        || !qIsFinite(amount) || !qIsFinite(fee)) {
        return fail(1, "orderId、amount或fee参数无效");
    }

    if (!Database::settleOrder(orderId, amount, fee)) {
        return fail(2, "结算失败：订单不存在、已结算过、或余额不足");
    }
    return ok(QJsonObject());
}
