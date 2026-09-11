#include "requestdispatcher.h"
#include "database.h"
#include "dispatchernavhelpers.h"

// RequestDispatcher's implementation is split by action domain across
// sibling dispatcher_*.cpp files:
//   dispatcher_account.cpp    - handleLogin/handleUpdateUserProfile/handleRechargeBalance
//   dispatcher_admin.cpp      - all handleAdmin* actions
//   dispatcher_stations.cpp   - handleQueryStations/handleQueryPileDetail/handleQueryStationDetail
//   dispatcher_navigation.cpp - handlePlanRoute/handleStartNavigation/
//                               handleSwitchNavigationMode/handleEndNavigation
//   dispatcher_navigation_helpers.cpp - resolveOrigin/resolveDestination/
//                               buildNavigationResponse/validateRoute
//   dispatcher_orders.cpp     - charging/settlement/order-query actions
// Shared free-function helpers (originally in an anonymous namespace here)
// now live in dispatchernavhelpers.h/.cpp and dispatcherorderhelpers.h/.cpp
// so more than one of the files above can use them.
// This file holds only the navigation-session statics, requestRoute()
// (the route-provider seam used by tests), ok()/fail(), and the
// action -> handler dispatch table in handle().

RequestDispatcher::NavigationState RequestDispatcher::s_navigation;
RequestDispatcher::RouteProvider RequestDispatcher::routeProvider;

bool RequestDispatcher::requestRoute(const QString &mode, double fromLat, double fromLng,
                                     double toLat, double toLng, QJsonObject *route,
                                     QString *error)
{
    if (routeProvider) {
        return routeProvider(mode, fromLat, fromLng, toLat, toLng, route, error);
    }
    return DispatcherNavHelpers::requestTencentRoute(mode, fromLat, fromLng, toLat, toLng, route, error);
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
    if (action == "admin_add_pile")    return handleAdminAddPile(params);
    if (action == "admin_set_station_pile_count") return handleAdminSetStationPileCount(params);
    if (action == "admin_query_stations") return handleAdminQueryStations(params);
    if (action == "admin_query_piles")  return handleAdminQueryPiles(params);
    if (action == "admin_restart_pile") return handleAdminRestartPile(params);
    if (action == "admin_set_pile_status") return handleAdminSetPileStatus(params);
    if (action == "admin_stats")        return handleAdminStats(params);
    if (action == "admin_orders")       return handleAdminOrders(params);
    if (action == "query_stations")     return handleQueryStations(params);
    if (action == "query_station_detail") return handleQueryStationDetail(params);
    if (action == "plan_route" || action == "route_planning") {
        return handlePlanRoute(params);
    }
    if (action == "start_navigation")      return handleStartNavigation(params);
    if (action == "switch_navigation_mode") return handleSwitchNavigationMode(params);
    if (action == "end_navigation")        return handleEndNavigation(params);
    if (action == "query_pile_detail")  return handleQueryPileDetail(params);
    if (action == "start_charging")     return handleStartCharging(params);
    if (action == "prepare_settlement") return handlePrepareSettlement(params);
    if (action == "query_order")        return handleQueryOrder(params);
    if (action == "query_user_ongoing_order") return handleQueryOngoingOrder(params);
    if (action == "settle_order")       return handleSettleOrder(params);

    return fail(1, QStringLiteral("未知的action：%1").arg(action));
}
