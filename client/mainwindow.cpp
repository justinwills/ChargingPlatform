#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "admin/adminwindow.h"
#include "flowlayout.h"
#include "navigationpage.h"

#include <QMessageBox>
#include <QButtonGroup>
#include <QGraphicsDropShadowEffect>
#include <QIcon>
#include <QSignalBlocker>
#include <QToolButton>

// MainWindow is a .ui-based (Qt Designer) form; behavior is split by
// page/domain across sibling mainwindow_*.cpp files:
//   mainwindow_chargeui.cpp        - setupChargePageUi() (Charge page layout)
//   mainwindow_chargeselectors.cpp - station/pile cards + selector dialogs
//   mainwindow_homenav.cpp         - Home page + station-detail navigation
//   mainwindow_stationdetail.cpp   - populateStationDetail/rebuildNearbyCards
//   mainwindow_accountpage.cpp     - Mine/Admin/Login
//   mainwindow_chargingsession.cpp - active charging session + payment preview
//   mainwindow_settingspage.cpp    - profile editing
//   mainwindow_rechargepage.cpp    - recharge flow
//   mainwindow_response.cpp        - onServerResponse() entry point + error path
//     mainwindow_response_charging.cpp - handleChargingResponse()
//     mainwindow_response_account.cpp  - handleAccountResponse()
// This file holds only the constructor/destructor, the connection-error
// slot, and small widget-setup helpers used across pages.

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
        , connection(new ClientConnection(this))
{
    ui->setupUi(this);

    m_navigationPage = new NavigationPage(ui->stackedWidget);
    ui->stackedWidget->addWidget(m_navigationPage);

    connect(m_navigationPage, &NavigationPage::backRequested, this, [this]() {
        if (m_navigationStarted && connection->isConnected()) {
            connection->sendRequest(QStringLiteral("end_navigation"), {});
        }
        leaveNavigationPage();
    });
    connect(m_navigationPage, &NavigationPage::endRequested, this, [this]() {
        if (m_navigationStarted && connection->isConnected()) {
            connection->sendRequest(QStringLiteral("end_navigation"), {});
        }
        leaveNavigationPage();
    });
    connect(m_navigationPage, &NavigationPage::planRequested, this,
            [this](const QString &originAddress, const QString &mode) {
        if (!originAddress.isEmpty()) {
            m_hasNavigationOrigin = false;
            requestNavigation(mode, originAddress);
        } else if (m_hasNavigationOrigin) {
            requestNavigation(mode);
        } else {
            m_navigationPage->requestCurrentLocation();
        }
    });
    connect(m_navigationPage, &NavigationPage::modeChanged, this,
            [this](const QString &mode) {
        if (m_navigationStarted && connection->isConnected()) {
            m_navigationPage->setBusy(true);
            connection->sendRequest(QStringLiteral("switch_navigation_mode"), {
                {QStringLiteral("mode"), mode}
            });
        } else if (m_hasNavigationOrigin) {
            requestNavigation(mode);
        } else {
            m_navigationPage->requestCurrentLocation();
        }
    });
    connect(m_navigationPage, &NavigationPage::currentLocationResolved, this,
            [this](double latitude, double longitude) {
        if (ui->stackedWidget->currentWidget() != m_navigationPage) {
            return;
        }
        m_navigationOriginLat = latitude;
        m_navigationOriginLng = longitude;
        m_hasNavigationOrigin = true;
        requestNavigation(m_navigationPage->mode());
    });

    ui->stackedWidget->setCurrentWidget(ui->pageLogin);
    ui->widgetNavigation->hide();
    ui->labelMonBadge->hide();
    ui->labelMonStatus->hide();

    auto *navShadow = new QGraphicsDropShadowEffect(ui->widgetNavigation);
    navShadow->setBlurRadius(24);
    navShadow->setOffset(0, -6);
    navShadow->setColor(QColor(15, 23, 42, 40));
    ui->widgetNavigation->setGraphicsEffect(navShadow);

    applyShadow(ui->balanceFrame);
    applyShadow(ui->loginCard);
    applyShadow(ui->profileCard);
    applyShadow(ui->menuCard);
    applyShadow(ui->startStationCard);
    applyShadow(ui->guideCard);
    applyShadow(ui->monitorCard);
    applyShadow(ui->sessionCard);
    applyShadow(ui->sdStatsCard);
    applyShadow(ui->labelOrderStationDetails);
    applyShadow(ui->walletCard);

    QButtonGroup *navGroup = new QButtonGroup(this);
    navGroup->addButton(ui->BtnHome);
    navGroup->addButton(ui->BtnCharge);
    navGroup->addButton(ui->BtnMine);

    navGroup->setExclusive(true);
    ui->BtnHome->setChecked(true);

    setupNavIcons();
    setupChargePageUi();

    connect(ui->comboPile, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int) { updateChargePileCard(); });

    connect(connection, &ClientConnection::responseReceived,
        this, &MainWindow::onServerResponse);

    connect(connection, &ClientConnection::connectionError,
            this, &MainWindow::onConnectionError);

    connect(connection,
            &ClientConnection::connected,
            this,
            []() {
                qDebug() << "已经连接到充电平台服务器";
            });

    connect(connection,
            &ClientConnection::disconnected,
            this,
            []() {
                qDebug() << "与服务器断开连接";
            });

    orderTimer.setInterval(1000);
    connect(&orderTimer, &QTimer::timeout,
        this, &MainWindow::on_BtnRefreshOrder_clicked);

    displayTimer.setInterval(1000);
    connect(&displayTimer, &QTimer::timeout, this, [this]() {
        if (!activeOrderStartTime.isValid() || activeOrderId < 0) {
            ui->labelElapsedTime->setText(tr("00:00:00"));
            return;
        }

        const qint64 elapsedSeconds = qMax<qint64>(
            0, activeOrderStartTime.secsTo(QDateTime::currentDateTime()));
        const int hours = static_cast<int>(elapsedSeconds / 3600);
        const int minutes = static_cast<int>((elapsedSeconds % 3600) / 60);
        const int seconds = static_cast<int>(elapsedSeconds % 60);
        ui->labelElapsedTime->setText(
            QStringLiteral("%1:%2:%3")
                .arg(hours, 2, 10, QLatin1Char('0'))
                .arg(minutes, 2, 10, QLatin1Char('0'))
                .arg(seconds, 2, 10, QLatin1Char('0')));
    });

}

MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::onConnectionError(const QString &message){
    ui->Btnlogin->setEnabled(true);
    ui->BtnConfirm_pageRecharge->setEnabled(true);
    ui->BtnConfirm_PageEdit->setEnabled(true);

    m_pendingAction.clear();
    m_chargePagePending = false;
    m_settlingOrderId = -1;
    if (m_balanceBeforeSettlement >= 0.0) {
        m_currentUser["balance"] = m_balanceBeforeSettlement;
        updateBalanceLabels(m_balanceBeforeSettlement);
        m_balanceBeforeSettlement = -1.0;
    }

    if (ui->stackedWidget->currentWidget() == m_navigationPage) {
        m_navigationPage->showError(message);
        return;
    }

    QMessageBox::warning(
        this,
        tr("网络错误"),
        message
        );
}


void MainWindow::applyShadow(QWidget *widget)
{
    auto *shadow = new QGraphicsDropShadowEffect(widget);
    shadow->setBlurRadius(24);
    shadow->setOffset(0, 6);
    shadow->setColor(QColor(19, 27, 46, 22));
    widget->setGraphicsEffect(shadow);
}

void MainWindow::setupNavIcons()
{
    const QSize navIconSize(24, 24);
    ui->BtnHome->setIconSize(navIconSize);
    ui->BtnCharge->setIconSize(navIconSize);
    ui->BtnMine->setIconSize(navIconSize);

    applyNavIcon(ui->BtnHome, ui->BtnHome->isChecked(), QStringLiteral("nav_home"));
    applyNavIcon(ui->BtnCharge, ui->BtnCharge->isChecked(), QStringLiteral("nav_charge"));
    applyNavIcon(ui->BtnMine, ui->BtnMine->isChecked(), QStringLiteral("nav_mine"));

    auto updateIcon = [this](QAbstractButton *button, const QString &base) {
        return [this, button, base](bool checked) {
            applyNavIcon(button, checked, base);
        };
    };

    connect(ui->BtnHome, &QToolButton::toggled,
            updateIcon(ui->BtnHome, QStringLiteral("nav_home")));
    connect(ui->BtnCharge, &QToolButton::toggled,
            updateIcon(ui->BtnCharge, QStringLiteral("nav_charge")));
    connect(ui->BtnMine, &QToolButton::toggled,
            updateIcon(ui->BtnMine, QStringLiteral("nav_mine")));
}

void MainWindow::applyNavIcon(QAbstractButton *button, bool checked, const QString &base)
{
    const QString suffix = checked ? QStringLiteral("_active") : QString();
    button->setIcon(QIcon(QStringLiteral(":/icons/%1%2.svg").arg(base).arg(suffix)));
}

void MainWindow::updateBalanceLabels(double balance)
{
    ui->labelMoney_c->setText(QStringLiteral("¥ %1").arg(balance, 0, 'f', 2));
    ui->labelRechargeBalance->setText(QStringLiteral("¥ %1").arg(balance, 0, 'f', 2));
}

QString MainWindow::formatRemainingChargeTime(int seconds) const
{
    seconds = qMax(0, seconds);
    if (seconds <= 0) {
        return tr("已充满，订单即将结束");
    }
    if (seconds < 60) {
        return tr("预计还需约 %1 秒充满").arg(seconds);
    }
    const int minutes = (seconds + 59) / 60;
    return tr("预计还需约 %1 分钟充满").arg(minutes);
}

int MainWindow::selectPileInCombo(int pileId)
{
    for (int i = 0; i < ui->comboPile->count(); ++i) {
        if (ui->comboPile->itemData(i).toInt() == pileId) {
            ui->comboPile->setCurrentIndex(i);
            return i;
        }
    }
    return -1;
}

