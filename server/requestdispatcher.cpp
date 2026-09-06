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
    if (action == "admin_login")        return handleAdminLogin(params);
    if (action == "query_stations")     return handleQueryStations(params);
    if (action == "query_station_detail") return handleQueryStationDetail(params);
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

    int ongoingOrderId = -1;
    if (Database::hasOngoingOrder(user.id, &ongoingOrderId)) {
        data["ongoingOrderId"] = ongoingOrderId;
    }
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

QJsonObject RequestDispatcher::handleQueryStations(const QJsonObject &params)
{
    const QString addressKeyword = params.value("address").toString().trimmed();
    if (addressKeyword.isEmpty()) return fail(1, "缺少address参数");

    GeocodedLocation location;
    QString geocodeError;
    const bool hasLocation = geocodeAddress(addressKeyword, &location, &geocodeError);
    if (!hasLocation && !qEnvironmentVariable("TENCENT_MAP_KEY").isEmpty()) {
        return fail(3, QStringLiteral("地址解析失败：%1").arg(geocodeError));
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
    for (const StationResult &result : results) {
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
    data["geocoded"] = hasLocation;
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
