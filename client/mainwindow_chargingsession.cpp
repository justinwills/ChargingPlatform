#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "paymentpreview.h"

#include <QDialog>
#include <QMessageBox>

// Active charging session: start/stop, order refresh/settlement, and
// the payment preview dialog.

void MainWindow::on_BtnStartCharging_clicked()
{
    if (userId < 0) {
        QMessageBox::warning(this, tr("提示"), tr("请先登录"));
        return;
    }
    if (ui->comboPile->currentIndex() < 0) {
        QMessageBox::warning(this, tr("提示"), tr("请先选择充电站和电桩"));
        return;
    }

    connection->sendRequest(QStringLiteral("start_charging"), {
        {"userId", userId},
        {"pileId", ui->comboPile->currentData().toInt()}
    });
}

void MainWindow::on_BtnPileDetail_clicked()
{
    if (ui->comboPile->currentIndex() < 0) {
        return;
    }
    connection->sendRequest(QStringLiteral("query_pile_detail"), {
        {"pileId", ui->comboPile->currentData().toInt()}
    });
}

void MainWindow::on_BtnLoadOrderStation_clicked()
{
    connection->sendRequest(QStringLiteral("query_station_detail"), {
        {"stationId", ui->spinOrderStationId->value()}
    });
}

void MainWindow::on_BtnRefreshOrder_clicked()
{
    if (activeOrderId < 0) {
        ui->labelMonStatus->setText(tr("当前没有进行中的充电订单"));
        return;
    }

    ui->labelMonStatus->setText(tr("正在刷新实时数据…"));
    connection->sendRequest(QStringLiteral("query_order"), {
        {"orderId", activeOrderId}
    });
}

void MainWindow::on_BtnSettleOrder_clicked()
{
    if (activeOrderId < 0) {
        QMessageBox::warning(this, tr("提示"), tr("当前没有进行中的订单"));
        return;
    }

    orderTimer.stop();
    displayTimer.stop();
    m_settlingOrderId = activeOrderId;
    m_pendingAction = QStringLiteral("prepare_settlement");
    connection->sendRequest(QStringLiteral("prepare_settlement"), {
        {"orderId", activeOrderId},
        {"amount", currentAmount},
        {"fee", currentFee}
    });
}

void MainWindow::showPaymentPreview()
{
    const double balance = m_currentUser.value("balance").toDouble();
    PaymentPreview preview(activeOrderId, currentAmount, currentFee, balance, this);
    m_paymentPreview = &preview;

    connect(&preview, &PaymentPreview::paymentConfirmed, this, [this]() {
        m_balanceBeforeSettlement = m_currentUser.value("balance").toDouble();
        const double pendingBalance = qMax(0.0, m_balanceBeforeSettlement - currentFee);
        m_currentUser["balance"] = pendingBalance;
        updateBalanceLabels(pendingBalance);

        m_pendingAction = QStringLiteral("settle_order");
        connection->sendRequest(QStringLiteral("settle_order"), {
            {"orderId", activeOrderId},
            {"amount", currentAmount},
            {"fee", currentFee}
        });
    });
    connect(&preview, &PaymentPreview::rechargeRequested,
            this, &MainWindow::on_BtnRecharge_clicked);
    connect(&preview, &QDialog::accepted, this, [this]() {
        ui->stackedWidget->setCurrentWidget(ui->pageCharge);
        ui->BtnCharge->setChecked(true);
        ui->BtnHome->setChecked(false);
        ui->BtnMine->setChecked(false);
    });
    connect(&preview, &QDialog::finished, this, [this]() {
        m_paymentPreview = nullptr;
    });

    preview.exec();
    m_paymentPreview = nullptr;
}

void MainWindow::on_BtnSearchStations_clicked()
{
    const QString address = ui->editAddress->text().trimmed();
    if (address.isEmpty()) {
        QMessageBox::warning(this, tr("地址搜索"), tr("请输入地址或区域"));
        return;
    }

    connection->sendRequest(QStringLiteral("query_stations"), {
        {"address", address}
    });
}

