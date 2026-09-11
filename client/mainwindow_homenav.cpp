#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "navigationpage.h"

#include <QMessageBox>

// Home page navigation and the station-detail -> navigation flow.

void MainWindow::on_BtnHome_clicked()
{
    ui->stackedWidget->setCurrentWidget(ui->pageHome);
    if (userId > 0 && connection->isConnected()) {
        connection->sendRequest(QStringLiteral("query_stations"), {});
    }
}

void MainWindow::on_BtnCharge_clicked()
{
    openChargePage();
}

void MainWindow::openChargePage()
{
    if (m_chargePagePending) {
        return;
    }
    if (userId <= 0 || !connection->isConnected()) {
        ui->stackedWidget->setCurrentWidget(ui->pageCharge);
        if (connection->isConnected()) {
            on_BtnLoadOrderStation_clicked();
        }
        return;
    }

    // 以服务器返回的最新结果为准判断是否还有未完成的充电订单，
    // 避免客户端缓存状态过期导致已结算后仍提示"请先结算"。
    m_chargePagePending = true;
    m_pendingAction = QStringLiteral("query_user_ongoing_order");
    connection->sendRequest(QStringLiteral("query_user_ongoing_order"), {
        {"userId", userId}
    });
}

void MainWindow::on_BtnStationBack_clicked()
{
    ui->stackedWidget->setGeometry(0, 0, 360, 540);
    ui->widgetNavigation->show();
    ui->stackedWidget->setCurrentWidget(ui->pageHome);
    ui->BtnHome->setChecked(true);
    if (userId > 0 && connection->isConnected()) {
        connection->sendRequest(QStringLiteral("query_stations"), {});
    }
}

void MainWindow::on_BtnStartChargeHere_clicked()
{
    openChargePage();
}

void MainWindow::on_BtnNavigateHere_clicked()
{
    if (m_lastStationId <= 0 || m_lastStationName.isEmpty()) {
        QMessageBox::warning(this, tr("导航"), tr("当前充电站信息不完整，请重新打开详情页"));
        return;
    }

    m_navigationStarted = false;
    m_hasNavigationOrigin = false;
    m_navigationPage->setDestination(
        m_lastStationId, m_lastStationName, m_lastStationAddress,
        m_lastStationLat, m_lastStationLng);

    const QString searchedAddress = ui->editAddress->text().trimmed();
    m_navigationPage->setOriginHint(searchedAddress);
    showNavigationPage();

    if (searchedAddress.isEmpty()) {
        m_navigationPage->requestCurrentLocation();
    } else {
        requestNavigation(m_navigationPage->mode(), searchedAddress);
    }
}

void MainWindow::showNavigationPage()
{
    ui->widgetNavigation->hide();
    ui->stackedWidget->setGeometry(0, 0, 360, 612);
    ui->stackedWidget->setCurrentWidget(m_navigationPage);
}

void MainWindow::leaveNavigationPage()
{
    m_navigationStarted = false;
    m_hasNavigationOrigin = false;
    m_navigationPage->resetNavigation();
    ui->stackedWidget->setGeometry(0, 0, 360, 540);
    ui->stackedWidget->setCurrentWidget(ui->pageStationDetail);
    ui->widgetNavigation->show();
    ui->BtnHome->setChecked(true);
}

void MainWindow::requestNavigation(const QString &mode, const QString &originAddress)
{
    if (!connection->isConnected()) {
        m_navigationPage->showError(tr("尚未连接服务器，无法规划路线"));
        return;
    }

    QJsonObject params{
        {QStringLiteral("stationId"), m_lastStationId},
        {QStringLiteral("mode"), mode}
    };
    if (!originAddress.trimmed().isEmpty()) {
        params[QStringLiteral("originAddress")] = originAddress.trimmed();
    } else if (m_hasNavigationOrigin) {
        params[QStringLiteral("originLatitude")] = m_navigationOriginLat;
        params[QStringLiteral("originLongitude")] = m_navigationOriginLng;
    } else {
        m_navigationPage->showError(tr("请允许获取当前位置，或手动输入详细起点"));
        return;
    }

    m_navigationPage->setBusy(true);
    connection->sendRequest(QStringLiteral("start_navigation"), params);
}

