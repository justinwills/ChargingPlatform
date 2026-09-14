#ifndef DISPATCHERORDERHELPERS_H
#define DISPATCHERORDERHELPERS_H

#include <QJsonObject>

#include "database.h"

// Shared by RequestDispatcher's order handlers (dispatcher_orders.cpp).
// Computes the live charging snapshot (elapsed time, estimated amount/fee,
// SoC, completion) shown while an order is in progress. Implemented in
// dispatcherorderhelpers.cpp. Originally a free function in an anonymous
// namespace inside requestdispatcher.cpp.
namespace DispatcherOrderHelpers {

QJsonObject buildChargingSnapshot(const OrderInfo &order,
                                   const PileInfo &pile,
                                   const StationInfo &station);

} // namespace DispatcherOrderHelpers

#endif // DISPATCHERORDERHELPERS_H
