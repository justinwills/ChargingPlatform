#include "requestdispatcher.h"
#include "database.h"
#include "dispatcherorderhelpers.h"

// ---------- 充电流程/订单：发起充电、待结算、订单查询、结算 ----------

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
            const QJsonObject snapshot = DispatcherOrderHelpers::buildChargingSnapshot(order, pile, station);
            for (auto it = snapshot.constBegin(); it != snapshot.constEnd(); ++it) {
                data[it.key()] = it.value();
            }
            if (snapshot.value(QStringLiteral("chargeComplete")).toBool()
                && Database::markOrderPendingSettlement(
                    order.id,
                    snapshot.value(QStringLiteral("estimatedAmount")).toDouble(),
                    snapshot.value(QStringLiteral("estimatedFee")).toDouble())) {
                data["status"] = QStringLiteral("待结算");
                data["amount"] = snapshot.value(QStringLiteral("estimatedAmount"));
                data["fee"] = snapshot.value(QStringLiteral("estimatedFee"));
                data["socPercent"] = 100;
                data["remainingSeconds"] = 0;
            }
        }
    }
    return ok(data);
}

// 查询当前用户名下是否存在未完成的充电订单（充电中/待结算），并带上权威的
    // 订单数据，客户端进入充电页时必须以此结果为准，避免客户端缓存数据过期后误报。
    QJsonObject RequestDispatcher::handleQueryOngoingOrder(const QJsonObject &params)
{
    if (!params.contains("userId")) {
        return fail(1, "缺少userId参数");
    }
    const int userId = params.value("userId").toInt();
    if (userId <= 0) {
        return fail(1, "userId参数无效");
    }

    int orderId = -1;
    const bool ongoing = Database::hasOngoingOrder(userId, &orderId);
    QJsonObject data;
    data["hasOngoing"] = ongoing;
    if (ongoing && orderId > 0) {
        OrderInfo order;
        if (Database::getOrderById(orderId, &order)) {
            data["orderId"] = order.id;
            data["pileId"] = order.pileId;
            data["startTime"] = order.startTime;
            data["amount"] = order.amount;
            data["fee"] = order.fee;
            data["status"] = order.status;
            if (order.status == QStringLiteral("充电中")) {
                PileInfo pile;
                StationInfo station;
                if (Database::getPileById(order.pileId, &pile)
                    && Database::getStationById(pile.stationId, &station)) {
                    const QJsonObject snapshot = DispatcherOrderHelpers::buildChargingSnapshot(order, pile, station);
                    for (auto it = snapshot.constBegin(); it != snapshot.constEnd(); ++it) {
                        data[it.key()] = it.value();
                    }
                    if (snapshot.value(QStringLiteral("chargeComplete")).toBool()
                        && Database::markOrderPendingSettlement(
                            order.id,
                            snapshot.value(QStringLiteral("estimatedAmount")).toDouble(),
                            snapshot.value(QStringLiteral("estimatedFee")).toDouble())) {
                        data["status"] = QStringLiteral("待结算");
                        data["amount"] = snapshot.value(QStringLiteral("estimatedAmount"));
                        data["fee"] = snapshot.value(QStringLiteral("estimatedFee"));
                        data["socPercent"] = 100;
                        data["remainingSeconds"] = 0;
                    }
                }
            }
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

    double newBalance = -1;
    if (!Database::settleOrder(orderId, amount, fee, &newBalance)) {
        return fail(2, "结算失败：订单不存在、已结算过、或余额不足");
    }
    QJsonObject data;
    if (newBalance >= 0) {
        data["balance"] = newBalance;
    }
    return ok(data);
}
