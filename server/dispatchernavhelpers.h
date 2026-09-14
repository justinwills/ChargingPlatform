#ifndef DISPATCHERNAVHELPERS_H
#define DISPATCHERNAVHELPERS_H

#include <QJsonObject>
#include <QString>

// Navigation/geocoding helpers shared by RequestDispatcher's station-query
// (dispatcher_stations.cpp) and navigation (dispatcher_navigation.cpp)
// handlers. Implemented in dispatcher_navhelpers.cpp. These were originally
// free functions in an anonymous namespace inside requestdispatcher.cpp;
// they're given external linkage here only so more than one .cpp can call
// them, and are not part of RequestDispatcher's public API.
namespace DispatcherNavHelpers {

struct GeocodedLocation {
    double latitude = 0;
    double longitude = 0;
};

bool geocodeAddress(const QString &address, GeocodedLocation *location, QString *error);

double distanceKm(double latitude1, double longitude1,
                   double latitude2, double longitude2);

bool requestTencentRoute(const QString &mode,
                          double fromLatitude, double fromLongitude,
                          double toLatitude, double toLongitude,
                          QJsonObject *route, QString *error);

} // namespace DispatcherNavHelpers

#endif // DISPATCHERNAVHELPERS_H
