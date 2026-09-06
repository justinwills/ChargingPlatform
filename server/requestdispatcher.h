#ifndef REQUESTDISPATCHER_H
#define REQUESTDISPATCHER_H

#include <QJsonObject>

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
//   query_pile_detail 电桩详情与所属站点电桩列表 params: {pileId}
//   start_charging 发起充电                params: {userId, pileId}
//   query_order    查询订单                params: {orderId}
//   settle_order   结算                    params: {orderId, amount, fee}
//
// 错误码约定：0=成功，1=参数缺失/格式错误，2=业务规则不允许（如余额不足/电桩占用），3=系统内部错误
class RequestDispatcher
{
public:
    // 传入完整请求JSON（包含action和params），返回完整响应JSON（包含code/msg/data）
    static QJsonObject handle(const QJsonObject &request);

private:
    static QJsonObject handleLogin(const QJsonObject &params);
    static QJsonObject handleAdminLogin(const QJsonObject &params);
    static QJsonObject handleQueryStations(const QJsonObject &params);
    static QJsonObject handleQueryStationDetail(const QJsonObject &params);
    static QJsonObject handleQueryPileDetail(const QJsonObject &params);
    static QJsonObject handleStartCharging(const QJsonObject &params);
    static QJsonObject handleQueryOrder(const QJsonObject &params);
    static QJsonObject handleSettleOrder(const QJsonObject &params);

    static QJsonObject ok(const QJsonObject &data);
    static QJsonObject fail(int code, const QString &msg);
};

#endif // REQUESTDISPATCHER_H
