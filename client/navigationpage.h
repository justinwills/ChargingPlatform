#ifndef NAVIGATIONPAGE_H
#define NAVIGATIONPAGE_H

#include <QJsonObject>
#include <QWidget>

class QButtonGroup;
class QLabel;
class QLineEdit;
class QPushButton;
class QUrl;

#ifdef CHARGING_HAS_WEBENGINE
class QWebEngineView;
#else
class QTextBrowser;
#endif

class NavigationPage : public QWidget
{
    Q_OBJECT

public:
    explicit NavigationPage(QWidget *parent = nullptr);

    void setDestination(int stationId, const QString &name, const QString &address,
                        double latitude, double longitude);
    void setOriginHint(const QString &address);
    void requestCurrentLocation();
    void showRoute(const QJsonObject &data);
    void showError(const QString &message);
    void setBusy(bool busy);
    void resetNavigation();

    QString mode() const;
    QString originAddress() const;

signals:
    void backRequested();
    void planRequested(const QString &originAddress, const QString &mode);
    void modeChanged(const QString &mode);
    void endRequested();
    void currentLocationResolved(double latitude, double longitude);

private:
    void buildUi();
    void loadTencentRoute(double originLatitude = 0.0,
                          double originLongitude = 0.0,
                          bool hasOriginCoordinates = false);
    QUrl tencentRouteUrl(double originLatitude, double originLongitude,
                         bool hasOriginCoordinates) const;
    void setActiveMode(const QString &mode);

    int m_stationId = -1;
    QString m_stationName;
    QString m_stationAddress;
    double m_stationLatitude = 0.0;
    double m_stationLongitude = 0.0;
    QString m_mode = QStringLiteral("driving");
    bool m_active = false;

    QButtonGroup *m_modeGroup = nullptr;
    QPushButton *m_driveButton = nullptr;
    QPushButton *m_walkButton = nullptr;
    QLineEdit *m_originEdit = nullptr;
    QLabel *m_destinationLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_durationLabel = nullptr;
    QLabel *m_etaLabel = nullptr;
    QLabel *m_distanceLabel = nullptr;
    QLabel *m_instructionLabel = nullptr;
    QPushButton *m_planButton = nullptr;
    QPushButton *m_endButton = nullptr;

#ifdef CHARGING_HAS_WEBENGINE
    QWebEngineView *m_webView = nullptr;
#else
    QTextBrowser *m_webView = nullptr;
#endif
};

#endif // NAVIGATIONPAGE_H
