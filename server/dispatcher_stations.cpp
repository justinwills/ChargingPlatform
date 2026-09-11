#include "requestdispatcher.h"
#include "database.h"
#include "dispatchernavhelpers.h"

#include <QJsonArray>

// ---------- 充电站/电桩查询：站点列表、电桩详情、站点详情 ----------

QJsonObject RequestDispatcher::handleQueryStations(const QJsonObject &params)
{
    const QString addressKeyword = params.value("address").toString().trimmed();
    DispatcherNavHelpers::GeocodedLocation location;
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
        hasLocation = DispatcherNavHelpers::geocodeAddress(addressKeyword, &location, &geocodeError);
        if (!hasLocation && !qEnvironmentVariable("TENCENT_MAP_KEY").isEmpty()) {
            return fail(3, QStringLiteral("地址解析失败：%1").arg(geocodeError));
        }
    }

    struct StationResult {
        StationInfo station;
        double distance = -1;
    };
    QList<StationResult> results;
    for (const auto &s : Database::getAllStations()) {
        if (hasLocation) {
            results.append({s, DispatcherNavHelpers::distanceKm(location.latitude, location.longitude,
                                          s.latitude, s.longitude)});
        } else if (addressKeyword.isEmpty()
                   || s.name.contains(addressKeyword, Qt::CaseInsensitive)
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

