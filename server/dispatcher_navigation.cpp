#include "requestdispatcher.h"

// ---------- 导航：规划路线、开始/切换/结束导航 ----------

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

    double fromLatitude = 0;
    double fromLongitude = 0;
    QJsonObject origin;
    QString originError;
    if (!resolveOrigin(params, &fromLatitude, &fromLongitude, &origin, &originError)) {
        return fail(1, originError);
    }

    double toLatitude = 0;
    double toLongitude = 0;
    QJsonObject destination;
    const int stationId = params.value(QStringLiteral("stationId")).toInt(-1);
    QString destError;
    if (!resolveDestination(params, &toLatitude, &toLongitude, &destination, &destError)) {
        return fail(2, destError);
    }

    QJsonObject route;
    QString routeError;
    if (!requestRoute(mode, fromLatitude, fromLongitude,
                             toLatitude, toLongitude, &route, &routeError)) {
        return fail(3, QStringLiteral("路线规划失败：%1").arg(routeError));
    }

    const int distanceMeters = route.value(QStringLiteral("distance")).toInt();
    const int durationSeconds = route.value(QStringLiteral("duration")).toInt();
    const QString validationError =
        validateRoute(distanceMeters, durationSeconds, route.value(QStringLiteral("polyline")),
                      fromLatitude, fromLongitude, toLatitude, toLongitude);
    if (!validationError.isEmpty()) {
        return fail(2, validationError);
    }

    return ok(buildNavigationResponse(mode, route, origin, destination, stationId));
}

QJsonObject RequestDispatcher::handleStartNavigation(const QJsonObject &params)
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

    double fromLatitude = 0;
    double fromLongitude = 0;
    QJsonObject origin;
    QString originError;
    if (!resolveOrigin(params, &fromLatitude, &fromLongitude, &origin, &originError)) {
        return fail(1, originError);
    }

    double toLatitude = 0;
    double toLongitude = 0;
    QJsonObject destination;
    const int stationId = params.value(QStringLiteral("stationId")).toInt(-1);
    QString destError;
    if (!resolveDestination(params, &toLatitude, &toLongitude, &destination, &destError)) {
        return fail(2, destError);
    }

    QJsonObject route;
    QString routeError;
    if (!requestRoute(mode, fromLatitude, fromLongitude,
                      toLatitude, toLongitude, &route, &routeError)) {
        return fail(3, QStringLiteral("路线规划失败：%1").arg(routeError));
    }

    const int distanceMeters = route.value(QStringLiteral("distance")).toInt();
    const int durationSeconds = route.value(QStringLiteral("duration")).toInt();
    const QString validationError =
        validateRoute(distanceMeters, durationSeconds, route.value(QStringLiteral("polyline")),
                      fromLatitude, fromLongitude, toLatitude, toLongitude);
    if (!validationError.isEmpty()) {
        return fail(2, validationError);
    }

    NavigationState &nav = s_navigation;
    nav.active = true;
    nav.mode = mode;
    nav.originLatitude = fromLatitude;
    nav.originLongitude = fromLongitude;
    nav.destLatitude = toLatitude;
    nav.destLongitude = toLongitude;
    nav.stationId = stationId;
    nav.originAddress = origin.value(QStringLiteral("address")).toString();
    nav.stationName = destination.value(QStringLiteral("stationName")).toString();
    nav.stationAddress = destination.value(QStringLiteral("stationAddress")).toString();

    return ok(buildNavigationResponse(mode, route, origin, destination, stationId));
}

QJsonObject RequestDispatcher::handleSwitchNavigationMode(const QJsonObject &params)
{
    if (!s_navigation.active) {
        return fail(2, QStringLiteral("当前没有进行中的导航，请先调用start_navigation"));
    }

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

    const NavigationState &nav = s_navigation;
    QJsonObject route;
    QString routeError;
    if (!requestRoute(mode, nav.originLatitude, nav.originLongitude,
                             nav.destLatitude, nav.destLongitude, &route, &routeError)) {
        return fail(3, QStringLiteral("路线规划失败：%1").arg(routeError));
    }

    const int distanceMeters = route.value(QStringLiteral("distance")).toInt();
    const int durationSeconds = route.value(QStringLiteral("duration")).toInt();
    const QString validationError =
        validateRoute(distanceMeters, durationSeconds, route.value(QStringLiteral("polyline")),
                      nav.originLatitude, nav.originLongitude, nav.destLatitude, nav.destLongitude);
    if (!validationError.isEmpty()) {
        return fail(2, validationError);
    }

    s_navigation.mode = mode;

    QJsonObject origin;
    origin[QStringLiteral("latitude")] = nav.originLatitude;
    origin[QStringLiteral("longitude")] = nav.originLongitude;
    if (!nav.originAddress.isEmpty()) {
        origin[QStringLiteral("address")] = nav.originAddress;
    }
    QJsonObject destination;
    destination[QStringLiteral("latitude")] = nav.destLatitude;
    destination[QStringLiteral("longitude")] = nav.destLongitude;
    if (nav.stationId > 0) {
        destination[QStringLiteral("stationId")] = nav.stationId;
        destination[QStringLiteral("stationName")] = nav.stationName;
        destination[QStringLiteral("stationAddress")] = nav.stationAddress;
    }

    return ok(buildNavigationResponse(mode, route, origin, destination, nav.stationId));
}

QJsonObject RequestDispatcher::handleEndNavigation(const QJsonObject &)
{
    QJsonObject data;
    if (s_navigation.active) {
        data[QStringLiteral("ended")] = true;
        data[QStringLiteral("previousMode")] = s_navigation.mode;
    } else {
        data[QStringLiteral("ended")] = false;
    }
    s_navigation = NavigationState();
    return ok(data);
}

