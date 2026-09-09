#ifndef REQUESTDISPATCHER_H
#define REQUESTDISPATCHER_H

#include <QJsonObject>
#include <QString>
#include <QJsonArray>
#include <functional>

// RequestDispatcher：把解析好的请求JSON（{"action":..., "params":{...}}）
// 分发给Database::里对应的函数处理，再组装成响应JSON（{"code":..., "msg":..., "data":{...}}）。
// 这一层完全不碰socket/线程细节，方便单独测试，也方便特布新往里面继续加
// PC服务器端后台管理需要的其他action（用户管理、电桩管理等，参照
// database.h里已经封装好的函数直接调用即可，不用重新写SQL）。
//
// 已实现的action（对应《概要设计说明书》4.2节建议的类型 + admin_login是额外补充的）：
//   login         手机号登录/自动注册      params: {phone}
//   admin_login   管理员登录（补充项）      params: {username, password}
//   query_stations 查询充电站列表           params: {}
//   plan_route    一次性路线规划            params: {origin(见下), destination(见下),
//                                                mode: "driving"/"walking"/"transit"}
//   start_navigation    开始导航            params: {origin(见下), destination(见下),
//                                                mode: "driving"/"walking"/"transit"}
//   switch_navigation_mode 切换导航模式     params: {mode}
//   end_navigation 结束导航                 params: {}
//   query_pile_detail 电桩详情与所属站点电桩列表 params: {pileId}
//   start_charging 发起充电                params: {userId, pileId}
//   query_order    查询订单                params: {orderId}
//   settle_order   结算                    params: {orderId, amount, fee}
//
// 起点(origin)参数：
//   originAddress        起点地址字符串（由服务器调用腾讯地图逆地理服务换算成经纬度）
//   或 originLatitude + originLongitude  （显式起点经纬度）
// 终点(destination)参数：
//   stationId            目标充电站ID（自动取站点的经纬度）
//   或 destinationLatitude + destinationLongitude
//
// 导航响应字段（plan_route / start_navigation / switch_navigation_mode）：
//   origin, destination, distance{meters,km}, duration{seconds,minutes}, eta,
//   polyline, steps[], traffic(暂为null)
//
// 错误码约定：0=成功，1=参数缺失/格式错误，2=业务规则不允许（如余额不足/电桩占用/路由校验失败），3=系统内部错误
class RequestDispatcher
{
public:
    // 传入完整请求JSON（包含action和params），返回完整响应JSON（包含code/msg/data）
    static QJsonObject handle(const QJsonObject &request);

    // 路线规划器依赖注入点：默认走腾讯地图HTTP接口；
    // 测试可直接替换为返回伪造路线的函数，避免依赖真实网络/API key。
    using RouteProvider = std::function<bool(const QString &mode,
                                             double fromLat, double fromLng,
                                             double toLat, double toLng,
                                             QJsonObject *route, QString *error)>;
    static RouteProvider routeProvider;

private:
    static QJsonObject handleLogin(const QJsonObject &params);
    static QJsonObject handleUpdateUserProfile(const QJsonObject &params);
    static QJsonObject handleRechargeBalance(const QJsonObject &params);

    static QJsonObject handleAdminLogin(const QJsonObject &params);
    static QJsonObject handleQueryUsers(const QJsonObject &params);
    static QJsonObject handleSetUserStatus(const QJsonObject &params);
    static QJsonObject handleAdminAddStation(const QJsonObject &params);
    static QJsonObject handleAdminSetStationPileCount(const QJsonObject &params);
    static QJsonObject handleAdminQueryStations(const QJsonObject &params);
    static QJsonObject handleAdminQueryPiles(const QJsonObject &params);
    static QJsonObject handleAdminRestartPile(const QJsonObject &params);
    static QJsonObject handleAdminStats(const QJsonObject &params);
    static QJsonObject handleAdminOrders(const QJsonObject &params);
    static QJsonObject handleQueryStations(const QJsonObject &params);
    static QJsonObject handleQueryStationDetail(const QJsonObject &params);
    static QJsonObject handlePlanRoute(const QJsonObject &params);
    static QJsonObject handleStartNavigation(const QJsonObject &params);
    static QJsonObject handleSwitchNavigationMode(const QJsonObject &params);
    static QJsonObject handleEndNavigation(const QJsonObject &params);
    static QJsonObject handleQueryPileDetail(const QJsonObject &params);
    static QJsonObject handleStartCharging(const QJsonObject &params);
    static QJsonObject handlePrepareSettlement(const QJsonObject &params);
    static QJsonObject handleQueryOrder(const QJsonObject &params);
    static QJsonObject handleQueryOngoingOrder(const QJsonObject &params);
    static QJsonObject handleSettleOrder(const QJsonObject &params);

    static QJsonObject ok(const QJsonObject &data);
    static QJsonObject fail(int code, const QString &msg);

    // 解析起点/终点，构造导航响应，供 plan_route / start_navigation / switch_navigation_mode 复用
    static bool resolveOrigin(const QJsonObject &params, double *lat, double *lng,
                              QJsonObject *origin, QString *error);
    static bool resolveDestination(const QJsonObject &params, double *lat, double *lng,
                                   QJsonObject *destination, QString *error);
    static QJsonObject buildNavigationResponse(const QString &mode,
                                               const QJsonObject &rawRoute,
                                               const QJsonObject &origin,
                                               const QJsonObject &destination,
                                               int stationId);
    // 校验规划结果是否物理上合理（距离/时长/折线），返回失败说明；合法返回空串
    static QString validateRoute(int distanceMeters, int durationSeconds,
                                 const QJsonValue &polyline, double originLat,
                                 double originLng, double destLat, double destLng);

    // 路线规划统一入口：优先使用 routeProvider，否则调用腾讯地图接口
    static bool requestRoute(const QString &mode, double fromLat, double fromLng,
                             double toLat, double toLng, QJsonObject *route, QString *error);

    // 导航会话生命周期状态
    struct NavigationState {
        bool active = false;
        QString mode;
        double originLatitude = 0;
        double originLongitude = 0;
        double destLatitude = 0;
        double destLongitude = 0;
        int stationId = -1;
        QString originAddress;
        QString stationName;
        QString stationAddress;
    };
    static NavigationState s_navigation;
};

#endif // REQUESTDISPATCHER_H
