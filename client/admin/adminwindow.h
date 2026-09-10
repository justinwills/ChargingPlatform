#ifndef ADMINWINDOW_H
#define ADMINWINDOW_H

#include <QMainWindow>
#include <QtCharts/QChartView>
#include <QJsonArray>
#include "../clientconnection.h"

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
QT_CHARTS_USE_NAMESPACE
#endif

class QLineEdit;
class QLabel;
class QDoubleSpinBox;
class QComboBox;
class QDateEdit;
class QTableWidget;
class QStackedWidget;
class QPushButton;
class QSpinBox;
class QScrollArea;
class QWidget;

class AdminWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit AdminWindow(QWidget *parent = nullptr);

private slots:
    void login();
    void logout();
    void handleResponse(const QJsonObject &response);
    void handleError(const QString &message);
    void refreshUsers();
    void toggleSelectedUser();
    void refreshStationsAndPiles();
    void addStation();
    void adjustStationPileCount();
    void setSelectedPileStatus();
    void refreshStats();
    void refreshOrders();
    void switchTab(int index);

private:
    void send(const QString &action, const QJsonObject &params = {});
    void requestInitialData();
    void showError(const QString &message);

    void applyLoginStyle();
    void applyDashboardStyle();
    void applyCommonStyle();
    void updateNavButtons(int activeIndex);
    void populateStatsCards(const QJsonObject &data);
    void populateRevenueTrend(const QJsonArray &trend);
    void populateRevenueChart(const QJsonArray &trend, int days);
    void populatePilesTable(const QJsonArray &piles);
    void showPilesForStation(int stationId, const QString &stationName);
    void showAllPiles();

    static QString stylesheet();

    ClientConnection *connection;

    // Stacked pages
    QStackedWidget *pages;
    QWidget *loginPage;
    QWidget *dashboardPage;

    // Login widgets
    QLineEdit *usernameEdit;
    QLineEdit *passwordEdit;
    QLabel *loginStatus;

    // Sidebar nav
    QList<QPushButton*> navButtons;
    QStackedWidget *contentStack;

    // Stat cards (Revenue & Stats page)
    QLabel *statTodayValue;
    QLabel *statMonthValue;
    QLabel *statTotalValue;
    QLabel *statPileIdleCount;
    QLabel *statPileBusyCount;
    QLabel *statPileFaultCount;
    QLabel *revenueTrendLabel;
    QComboBox *revenuePeriodCombo;
    QChartView *revenueChartView;

    // Users page
    QLineEdit *userFilterEdit;
    QTableWidget *usersTable;

    // Stations page
    QLineEdit *stationNameEdit;
    QLineEdit *stationAddressEdit;
    QDoubleSpinBox *stationLongitudeEdit;
    QDoubleSpinBox *stationLatitudeEdit;
    QDoubleSpinBox *stationPriceEdit;
    QSpinBox *stationPileCountEdit;
    QTableWidget *stationsTable;
    QTableWidget *pilesTable;
    QLabel *pileFilterLabel;
    QPushButton *showAllPilesBtn;
    QSpinBox *stationAdjustIdSpin;
    QSpinBox *stationAdjustPileCountSpin;
    QSpinBox *pileIdSpin;
    QComboBox *pileStatusCombo;

    // Cache of the last full piles list received from the server, so a
    // station row click can filter locally without another round trip.
    QJsonArray allPilesCache;
    int selectedPileStationId = -1;
    QString selectedPileStationName;

    // Orders page
    QLineEdit *orderPhoneFilter;
    QComboBox *orderStationFilter;
    QComboBox *orderStatusFilter;
    QDateEdit *orderFromDate;
    QDateEdit *orderToDate;
    QTableWidget *ordersTable;

    QString pendingAction;
};

#endif
