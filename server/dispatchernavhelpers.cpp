#include "dispatchernavhelpers.h"

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <cmath>

namespace DispatcherNavHelpers {

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

    // Tencent's direction endpoint returns duration in minutes and the
    // compressed polyline as an array. Internally the dispatcher keeps using
    // seconds so ETA/validation and injected test providers share one unit.
    QJsonObject normalizedRoute = routes.first().toObject();
    normalizedRoute[QStringLiteral("duration")] =
        normalizedRoute.value(QStringLiteral("duration")).toInt() * 60;
    *route = normalizedRoute;
    return true;
}

} // namespace DispatcherNavHelpers
