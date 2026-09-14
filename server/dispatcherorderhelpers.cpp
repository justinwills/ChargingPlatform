#include "dispatcherorderhelpers.h"

#include <QDateTime>
#include <QtGlobal>

QJsonObject DispatcherOrderHelpers::buildChargingSnapshot(const OrderInfo &order,
                                  const PileInfo &pile,
                                  const StationInfo &station)
{
    constexpr int demoFullChargeSeconds = 60;
    constexpr int startSocPercent = 5;
    constexpr int chargingSocLimit = 100;

    const auto startTime = QDateTime::fromString(
        order.startTime, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    const int elapsedSeconds = startTime.isValid()
        ? qMax(0, static_cast<int>(startTime.secsTo(QDateTime::currentDateTime())))
        : 0;
    const int durationMinutes = elapsedSeconds / 60;
    const double estimatedAmount = pile.power * elapsedSeconds / 3600.0;
    const double estimatedFee = estimatedAmount * station.price;
    const int remainingSeconds = qMax(0, demoFullChargeSeconds - elapsedSeconds);
    const int socPercent = qBound(
        startSocPercent,
        startSocPercent
            + (elapsedSeconds * (chargingSocLimit - startSocPercent)
               / demoFullChargeSeconds),
        chargingSocLimit);

    return QJsonObject{
        {QStringLiteral("elapsedSeconds"), elapsedSeconds},
        {QStringLiteral("durationMinutes"), durationMinutes},
        {QStringLiteral("estimatedAmount"), estimatedAmount},
        {QStringLiteral("estimatedFee"), estimatedFee},
        {QStringLiteral("socPercent"), socPercent},
        {QStringLiteral("remainingSeconds"), remainingSeconds},
        {QStringLiteral("chargeComplete"), remainingSeconds == 0},
        {QStringLiteral("demoFullChargeSeconds"), demoFullChargeSeconds}
    };
}
