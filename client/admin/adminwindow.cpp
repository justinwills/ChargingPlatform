#include "adminwindow.h"
#include "admindesign.h"

#include <QComboBox>
#include <QDateEdit>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTableWidget>
#include <QtCharts/QChartView>

// AdminWindow is built page-by-page in the buildXxx() methods defined in
// the sibling adminwindow_*.cpp files:
//   adminwindow_loginpage.cpp      - buildLoginPage()
//   adminwindow_dashboardshell.cpp - buildDashboardShell()
//   adminwindow_statstab.cpp       - buildStatsTab()      (Tab 0)
//   adminwindow_userstab.cpp       - buildUsersTab()       (Tab 1)
//   adminwindow_stationstab.cpp    - buildStationsTab()    (Tab 2)
//   adminwindow_orderstab.cpp      - buildOrdersTab()      (Tab 3)
// This file only holds the constructor (which wires member init + calls
// the builders in order), plus small style/navigation glue.
// Other AdminWindow behavior lives in:
//   adminwindow_style.cpp      - stylesheet()
//   adminwindow_login.cpp      - login()/logout()/send()/requestInitialData()
//   adminwindow_datarefresh.cpp - refresh*/toggle*/add*/adjust*/set* senders
//   adminwindow_statscards.cpp - populateStatsCards/populateRevenueTrend/Chart
//   adminwindow_pilestable.cpp - populatePilesTable/showPilesForStation/showAllPiles
//   adminwindow_response.cpp   - handleResponse()/handleError()/showError()

AdminWindow::AdminWindow(QWidget *parent)
    : QMainWindow(parent), connection(new ClientConnection(this)),
      pages(new QStackedWidget(this)),
      usernameEdit(new QLineEdit), passwordEdit(new QLineEdit),
      loginStatus(new QLabel),
      userFilterEdit(new QLineEdit), usersTable(new QTableWidget),
      stationNameEdit(new QLineEdit), stationAddressEdit(new QLineEdit),
      stationLongitudeEdit(new QDoubleSpinBox), stationLatitudeEdit(new QDoubleSpinBox),
      stationPriceEdit(new QDoubleSpinBox), stationPileCountEdit(new QSpinBox),
      stationsTable(new QTableWidget), pilesTable(new QTableWidget),
      pileFilterLabel(new QLabel), showAllPilesBtn(new QPushButton),
      stationAdjustIdSpin(new QSpinBox), stationAdjustPileCountSpin(new QSpinBox),
      pileIdSpin(new QSpinBox), pileStatusCombo(new QComboBox),
      orderPhoneFilter(new QLineEdit), orderStationFilter(new QComboBox),
      orderStatusFilter(new QComboBox), orderFromDate(new QDateEdit),
      orderToDate(new QDateEdit), ordersTable(new QTableWidget),
      statTodayValue(new QLabel), statMonthValue(new QLabel), statTotalValue(new QLabel),
      statPileIdleCount(new QLabel), statPileBusyCount(new QLabel), statPileFaultCount(new QLabel),
      revenueTrendLabel(new QLabel), revenuePeriodCombo(new QComboBox),
      revenueChartView(new QChartView)
{
    setWindowTitle(QStringLiteral("东软电动汽车充电平台 · 管理后台"));
    resize(1120, 700);
    setMinimumSize(960, 600);
    setCentralWidget(pages);

    applyCommonStyle();

    buildLoginPage();
    buildDashboardShell();
    buildStatsTab();
    buildUsersTab();
    buildStationsTab();
    buildOrdersTab();

    // ══════════════════════════════════════════════════════════════════════════
    // Connection signals
    // ══════════════════════════════════════════════════════════════════════════
    connect(connection, &ClientConnection::responseReceived, this, &AdminWindow::handleResponse);
    connect(connection, &ClientConnection::connectionError, this, &AdminWindow::handleError);
}

// ─── Style Application ─────────────────────────────────────────────────────────
void AdminWindow::applyCommonStyle()
{
    setStyleSheet(stylesheet());
}

void AdminWindow::applyLoginStyle() {}
void AdminWindow::applyDashboardStyle() {}

// ─── Navigation ────────────────────────────────────────────────────────────────
void AdminWindow::switchTab(int index)
{
    contentStack->setCurrentIndex(index);
    updateNavButtons(index);
}

void AdminWindow::updateNavButtons(int activeIndex)
{
    for (int i = 0; i < navButtons.size(); ++i) {
        navButtons[i]->setChecked(i == activeIndex);
    }
}

