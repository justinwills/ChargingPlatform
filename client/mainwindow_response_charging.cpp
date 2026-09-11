#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "navigationpage.h"
#include "paymentpreview.h"

#include <QMessageBox>
#include <QJsonArray>
#include <QJsonObject>

// Handles the "charging domain" server responses: navigation, order
// settlement, and the ongoing-order check that opens the charge page.
// Called from onServerResponse() (mainwindow.cpp) on the success path,
// before handleAccountResponse(). Returns true if `action` was handled.
bool MainWindow::handleChargingResponse(const QString &action, const QJsonObject &data,
                                         const QJsonObject &response)
{
    Q_UNUSED(response);
    if (action == QStringLiteral("start_navigation")
        || action == QStringLiteral("switch_navigation_mode")) {
        if (ui->stackedWidget->currentWidget() != m_navigationPage) {
            m_navigationStarted = false;
            if (action == QStringLiteral("start_navigation") && connection->isConnected()) {
                connection->sendRequest(QStringLiteral("end_navigation"), {});
            }
            return true;
        }
        m_navigationStarted = true;
        m_navigationPage->showRoute(data);
        return true;
    }

    if (action == QStringLiteral("end_navigation")) {
        m_navigationStarted = false;
        return true;
    }

    if (action == QStringLiteral("settle_order")) {
        if (!data.contains("balance")) {
            if (m_balanceBeforeSettlement >= 0.0) {
                m_currentUser["balance"] = m_balanceBeforeSettlement;
                updateBalanceLabels(m_balanceBeforeSettlement);
                m_balanceBeforeSettlement = -1.0;
            }
            if (m_paymentPreview) {
                m_paymentPreview->reject();
            }
            QMessageBox::warning(this, tr("结算失败"), tr("服务器未返回最新余额"));
            return true;
        }

        const double newBalance = data.value("balance").toDouble();
        m_currentUser["balance"] = newBalance;
        updateBalanceLabels(newBalance);
        m_balanceBeforeSettlement = -1.0;

        ui->labelMonStatus->setText(tr("订单已结算，余额已更新"));
        ui->labelElapsedTime->setText(QStringLiteral("00:00:00"));
        activeOrderStartTime = QDateTime();
        activeOrderId = -1;
        currentAmount = 0;
        currentFee = 0;
        orderTimer.stop();
        displayTimer.stop();

        ui->stackedWidget->setCurrentWidget(ui->pageCharge);
        ui->BtnCharge->setChecked(true);
        ui->BtnHome->setChecked(false);
        ui->BtnMine->setChecked(false);
        return true;
    }

    if (action == QStringLiteral("query_user_ongoing_order")) {
        m_chargePagePending = false;
        const QString status = data.value("status").toString();
        if (data.value("hasOngoing").toBool()
            && (status == QStringLiteral("充电中")
                || status == QStringLiteral("待结算"))) {
            const int orderId = data.value("orderId").toInt(-1);
            if (orderId > 0) {
                activeOrderId = orderId;
                currentAmount = data.value("estimatedAmount").toDouble(
                    data.value("amount").toDouble(currentAmount));
                currentFee = data.value("estimatedFee").toDouble(
                    data.value("fee").toDouble(currentFee));
                m_lastOrder = data;
                m_pendingPileId = -1;

                // 订单仍在充电中：只恢复充电监控页，绝不自动结算或弹结算窗。
                if (status == QStringLiteral("充电中")) {
                    const int socPercent = data.value("socPercent").toInt(5);
                    const int remainingSeconds = data.value("remainingSeconds").toInt(60);
                    ui->labelMonStatus->setText(tr("正在充电 · 数据每 1 秒刷新"));
                    ui->labelMonStation->setText(
                        m_lastStationName.isEmpty()
                            ? QStringLiteral("睿光充电站") : m_lastStationName);
                    ui->labelMonOrderId->setText(
                        tr("订单号：#%1").arg(activeOrderId));
                    ui->labelMonStart->setText(
                        tr("开始时间：%1")
                            .arg(data.value("startTime").toString()));
                    activeOrderStartTime = QDateTime::fromString(
                        data.value("startTime").toString(),
                        QStringLiteral("yyyy-MM-dd HH:mm:ss"));
                    ui->labelMonKwh->setText(
                        QStringLiteral("%1 kWh")
                            .arg(data.value("estimatedAmount").toDouble(
                                currentAmount), 0, 'f', 2));
                    ui->labelMonFee->setText(
                        QStringLiteral("¥%1").arg(data.value("estimatedFee")
                            .toDouble(currentFee), 0, 'f', 2));
                    ui->labelMonSoc->setText(QStringLiteral("%1%").arg(socPercent));
                    ui->ringCharge->setValue(socPercent);
                    ui->labelMonEta->setText(formatRemainingChargeTime(remainingSeconds));
                    ui->stackedWidget->setCurrentWidget(ui->pageChargeMonitor);
                    ui->BtnCharge->setChecked(true);
                    ui->BtnHome->setChecked(false);
                    ui->BtnMine->setChecked(false);
                    displayTimer.start();
                    orderTimer.start();
                    return true;
                }

                // 订单已是待结算状态：进入结算流程。
                QMessageBox::information(
                    this, tr("提示"), tr("您有未完成的充电订单，请先结算"));
                ui->stackedWidget->setCurrentWidget(ui->pageChargeMonitor);
                ui->BtnCharge->setChecked(true);
                ui->BtnHome->setChecked(false);
                ui->BtnMine->setChecked(false);

                m_pendingAction = QStringLiteral("prepare_settlement");
                connection->sendRequest(QStringLiteral("prepare_settlement"), {
                    {"orderId", activeOrderId},
                    {"amount", currentAmount},
                    {"fee", currentFee}
                });
                orderTimer.stop();
                displayTimer.stop();
                m_settlingOrderId = activeOrderId;
                return true;
            }
        }
        ui->stackedWidget->setCurrentWidget(ui->pageCharge);
        ui->BtnCharge->setChecked(true);
        ui->BtnHome->setChecked(false);
        ui->BtnMine->setChecked(false);
        if (connection->isConnected()) {
            on_BtnLoadOrderStation_clicked();
        }
        return true;
    }

    if (data.contains("stations")) {
        const QJsonArray stations = data.value("stations").toArray();
        m_lastStations = stations;
        ui->labelNearbyCount->setText(tr("共 %1 个电站").arg(stations.size()));
        rebuildNearbyCards(stations);
        if (!m_chargeStation.isEmpty()) {
            updateChargeStationCard(m_chargeStation);
        }
        return true;
    }

    if (data.contains("piles") && data.contains("stationId")) {
        m_lastStationId = data.value("stationId").toInt(-1);
        if (data.contains("latitude")) {
            m_lastStationLat = data.value("latitude").toDouble();
        }
        if (data.contains("longitude")) {
            m_lastStationLng = data.value("longitude").toDouble();
        }
        m_lastStationName = data.value("name").toString(
            data.value("stationName").toString());
        m_lastStationAddress = data.value("address").toString(
            data.value("stationAddress").toString());
        populateStationDetail(data);

        QStringList orderDetails;
        m_chargeStation = data;
        m_chargePiles = data.value(QStringLiteral("piles")).toArray();
        m_chargeStationPrice = data.value(QStringLiteral("price")).toDouble();
        if (!m_chargeStation.contains(QStringLiteral("freePileCount"))) {
            int freeCount = 0;
            for (const QJsonValue &value : m_chargePiles) {
                if (isPileAvailable(value.toObject())) {
                    ++freeCount;
                }
            }
            m_chargeStation[QStringLiteral("freePileCount")] = freeCount;
        }
        updateChargeStationCard(m_chargeStation);

        const int requestedPileId = m_pendingPileId > 0
            ? m_pendingPileId : data.value(QStringLiteral("pileId")).toInt(-1);
        const QString stationName = data.value("name").toString(
            data.value("stationName").toString());
        orderDetails << tr("站点：%1\n地址：%2\n价格：%3 元/度")
                            .arg(stationName)
                            .arg(data.value("address").toString(
                                data.value("stationAddress").toString()))
                            .arg(data.value("price").toDouble(), 0, 'f', 2);
        orderDetails << QStringLiteral("电桩：");
        {
            const QSignalBlocker blocker(ui->comboPile);
            ui->comboPile->clear();
            ui->comboPile->setCurrentIndex(-1);
            for (const QJsonValue &value : data.value("piles").toArray()) {
                const QJsonObject pile = value.toObject();
                orderDetails << tr("%1(%2) %3")
                                    .arg(pile.value("code").toString())
                                    .arg(pile.value("type").toString())
                                    .arg(pile.value("status").toString());

                const QString pileDescription = tr("%1 | %2 | %3 kW | %4")
                    .arg(pile.value("code").toString())
                    .arg(pile.value("type").toString())
                    .arg(pile.value("power").toDouble(), 0, 'f', 1)
                    .arg(pile.value("status").toString());
                ui->comboPile->addItem(pileDescription, pile.value("pileId"));
            }
            ui->comboPile->setCurrentIndex(-1);
            if (requestedPileId > 0) {
                for (int index = 0; index < m_chargePiles.size(); ++index) {
                    const QJsonObject pile = m_chargePiles.at(index).toObject();
                    if (pile.value(QStringLiteral("pileId")).toInt() == requestedPileId
                        && isPileAvailable(pile)) {
                        selectPileInCombo(requestedPileId);
                        break;
                    }
                }
            }
        }
        m_pendingPileId = -1;
        updateChargePileCard();
        ui->labelOrderStationDetails->setPlainText(orderDetails.join(QStringLiteral("\n")));
        if (data.contains("pileId") && data.value("pileId").toInt() > 0) {
            ui->labelPileStation->setText(
                tr("当前选择：%1（站点ID：%2，电桩ID：%3）")
                    .arg(stationName)
                    .arg(data.value("stationId").toInt())
                    .arg(data.value("pileId").toInt()));
        } else {
            ui->labelPileStation->setText(
                tr("已选择站点：%1（站点ID：%2）")
                    .arg(stationName)
                    .arg(data.value("stationId").toInt()));
        }
        if (m_showStationDetailPage) {
            m_showStationDetailPage = false;
            ui->stackedWidget->setCurrentWidget(ui->pageStationDetail);
            ui->BtnHome->setChecked(true);
        }
        return true;
    }


    return false;
}
