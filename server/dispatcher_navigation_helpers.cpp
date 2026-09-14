#include "requestdispatcher.h"
#include "database.h"
#include "dispatchernavhelpers.h"

// ---------- 导航辅助：起点/终点解析、导航响应组装、路线合理性校验 ----------

bool RequestDispatcher::resolveOrigin(const QJsonObject &params, double *lat, double *lng,
                                      QJsonObject *origin, QString *error)
{
    // 显式经纬度优先
    if (params.contains(QStringLiteral("originLatitude"))
        && params.contains(QStringLiteral("originLongitude"))) {
        double latitude = params.value(QStringLiteral("originLatitude")).toDouble();
        double longitude = params.value(QStringLiteral("originLongitude")).toDouble();
        if (!qIsFinite(latitude) || !qIsFinite(longitude)
            || latitude < -90 || latitude > 90
            || longitude < -180 || longitude > 180) {
            if (error) *error = QStringLiteral("起点经纬度无效");
            return false;
        }
        *lat = latitude;
        *lng = longitude;
        if (origin) {
            (*origin)[QStringLiteral("latitude")] = latitude;
            (*origin)[QStringLiteral("longitude")] = longitude;
        }
        return true;
    }
    if (params.contains(QStringLiteral("fromLatitude"))
        && params.contains(QStringLiteral("fromLongitude"))) {
        double latitude = params.value(QStringLiteral("fromLatitude")).toDouble();
        double longitude = params.value(QStringLiteral("fromLongitude")).toDouble();
        if (!qIsFinite(latitude) || !qIsFinite(longitude)
            || latitude < -90 || latitude > 90
            || longitude < -180 || longitude > 180) {
            if (error) *error = QStringLiteral("起点经纬度无效");
            return false;
        }
        *lat = latitude;
        *lng = longitude;
        if (origin) {
            (*origin)[QStringLiteral("latitude")] = latitude;
            (*origin)[QStringLiteral("longitude")] = longitude;
        }
        return true;
    }

    // 地址字符串：调用腾讯地图逆地理服务换算成经纬度
    const QString address = params.value(QStringLiteral("originAddress")).toString().trimmed();
    if (!address.isEmpty()) {
        DispatcherNavHelpers::GeocodedLocation location;
        QString geocodeError;
        if (!DispatcherNavHelpers::geocodeAddress(address, &location, &geocodeError)) {
            if (error) *error = QStringLiteral("起点地址解析失败：%1").arg(geocodeError);
            return false;
        }
        *lat = location.latitude;
        *lng = location.longitude;
        if (origin) {
            (*origin)[QStringLiteral("latitude")] = location.latitude;
            (*origin)[QStringLiteral("longitude")] = location.longitude;
            (*origin)[QStringLiteral("address")] = address;
        }
        return true;
    }

    if (error) *error = QStringLiteral("缺少或无效的起点（需要originLatitude/originLongitude或originAddress）");
    return false;
}

bool RequestDispatcher::resolveDestination(const QJsonObject &params, double *lat, double *lng,
                                           QJsonObject *destination, QString *error)
{
    const int stationId = params.value(QStringLiteral("stationId")).toInt(-1);
    StationInfo station;
    if (stationId > 0) {
        if (!Database::getStationById(stationId, &station)) {
            if (error) *error = QStringLiteral("找不到目标充电站");
            return false;
        }
        *lat = station.latitude;
        *lng = station.longitude;
        if (destination) {
            (*destination)[QStringLiteral("stationId")] = station.id;
            (*destination)[QStringLiteral("stationName")] = station.name;
            (*destination)[QStringLiteral("stationAddress")] = station.address;
            (*destination)[QStringLiteral("latitude")] = station.latitude;
            (*destination)[QStringLiteral("longitude")] = station.longitude;
        }
        return true;
    }

    // destination* 优先，其次 to*
    double latitude = 0;
    double longitude = 0;
    bool haveLat = false;
    bool haveLng = false;
    if (params.contains(QStringLiteral("destinationLatitude"))) {
        latitude = params.value(QStringLiteral("destinationLatitude")).toDouble();
        haveLat = qIsFinite(latitude);
    } else if (params.contains(QStringLiteral("toLatitude"))) {
        latitude = params.value(QStringLiteral("toLatitude")).toDouble();
        haveLat = qIsFinite(latitude);
    }
    if (params.contains(QStringLiteral("destinationLongitude"))) {
        longitude = params.value(QStringLiteral("destinationLongitude")).toDouble();
        haveLng = qIsFinite(longitude);
    } else if (params.contains(QStringLiteral("toLongitude"))) {
        longitude = params.value(QStringLiteral("toLongitude")).toDouble();
        haveLng = qIsFinite(longitude);
    }
    if (!haveLat || !haveLng) {
        if (error) *error = QStringLiteral("需要stationId或目标经纬度");
        return false;
    }
    if (latitude < -90 || latitude > 90 || longitude < -180 || longitude > 180) {
        if (error) *error = QStringLiteral("目标经纬度无效");
        return false;
    }
    *lat = latitude;
    *lng = longitude;
    if (destination) {
        (*destination)[QStringLiteral("latitude")] = latitude;
        (*destination)[QStringLiteral("longitude")] = longitude;
    }
    return true;
}

QJsonObject RequestDispatcher::buildNavigationResponse(const QString &mode,
                                                       const QJsonObject &route,
                                                       const QJsonObject &origin,
                                                       const QJsonObject &destination,
                                                       int stationId)
{
    const int distanceMeters = route.value(QStringLiteral("distance")).toInt();
    const int durationSeconds = route.value(QStringLiteral("duration")).toInt();

    QJsonObject data;
    data[QStringLiteral("mode")] = mode;
    data[QStringLiteral("origin")] = origin;
    data[QStringLiteral("destination")] = destination;

    QJsonObject distance;
    distance[QStringLiteral("meters")] = distanceMeters;
    distance[QStringLiteral("km")] = distanceMeters / 1000.0;
    data[QStringLiteral("distance")] = distance;

    QJsonObject duration;
    duration[QStringLiteral("seconds")] = durationSeconds;
    duration[QStringLiteral("minutes")] = qCeil(durationSeconds / 60.0);
    data[QStringLiteral("duration")] = duration;

    // ETA = 当前时间 + 预计时长
    const QDateTime eta = QDateTime::currentDateTime().addSecs(durationSeconds);
    data[QStringLiteral("eta")] = eta.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));

    data[QStringLiteral("polyline")] = route.value(QStringLiteral("polyline"));

    QJsonArray steps;
    for (const QJsonValue &value : route.value(QStringLiteral("steps")).toArray()) {
        const QJsonObject source = value.toObject();
        QJsonObject step{
            {QStringLiteral("instruction"), source.value(QStringLiteral("instruction"))},
            {QStringLiteral("roadName"), source.value(QStringLiteral("road_name"))},
            {QStringLiteral("distanceMeters"), source.value(QStringLiteral("distance"))},
            {QStringLiteral("durationSeconds"), source.value(QStringLiteral("duration"))}
        };
        steps.append(step);
    }
    data[QStringLiteral("steps")] = steps;

    // Traffic metadata remains null until the Tencent response shape is confirmed.
    data[QStringLiteral("traffic")] = QJsonValue(QJsonValue::Null);

    if (stationId > 0) {
        data[QStringLiteral("stationId")] = destination.value(QStringLiteral("stationId")).toInt(stationId);
        data[QStringLiteral("stationName")] = destination.value(QStringLiteral("stationName")).toString();
        data[QStringLiteral("stationAddress")] = destination.value(QStringLiteral("stationAddress")).toString();
    }

    return data;
}

QString RequestDispatcher::validateRoute(int distanceMeters, int durationSeconds,
                                         const QJsonValue &polyline, double originLat,
                                         double originLng, double destLat, double destLng)
{
    if (distanceMeters <= 0) {
        return QStringLiteral("路线距离无效");
    }
    if (durationSeconds <= 0) {
        return QStringLiteral("路线时长无效（不能为负或零）");
    }
    const bool hasStringPolyline = polyline.isString() && !polyline.toString().isEmpty();
    const bool hasArrayPolyline = polyline.isArray() && !polyline.toArray().isEmpty();
    if (!hasStringPolyline && !hasArrayPolyline) {
        return QStringLiteral("路线缺少折线数据");
    }

    // 校验物理合理性：直线距离不应超过规划路线距离；平均速度不应高到不现实
    const double straightKm = DispatcherNavHelpers::distanceKm(originLat, originLng, destLat, destLng);
    const double routeKm = distanceMeters / 1000.0;
    if (routeKm + 0.5 < straightKm) {
        return QStringLiteral("路线距离与起终点严重不符");
    }
    const double averageSpeedKmh = routeKm / (durationSeconds / 3600.0);
    // 机动车极限参考速度（约300km/h以上视为不合理），步行/公交等较慢模式不受此约束
    if (averageSpeedKmh > 300.0) {
        return QStringLiteral("路线时长与距离矛盾（速度不现实）");
    }
    if (routeKm > 0 && durationSeconds < 60 && routeKm > 1.0) {
        return QStringLiteral("路线时长与距离矛盾（例如7公里不可能在1分钟内到达）");
    }
    return QString();
}

