#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "navigationpage.h"

#include <QMessageBox>
#include <QJsonObject>

// Entry point for server responses. Handles shared setup and the
// generic error path (code != 0), then dispatches successful responses
// by domain to handleChargingResponse() (mainwindow_response_charging.cpp)
// and handleAccountResponse() (mainwindow_response_account.cpp).

void MainWindow::onServerResponse(const QJsonObject &response)
{
    const QString action = response.value(QStringLiteral("_requestAction"))
                               .toString(m_pendingAction);
    m_pendingAction.clear();

    ui->Btnlogin->setEnabled(true);
    ui->BtnConfirm_PageEdit->setEnabled(true);
    ui->BtnConfirm_pageRecharge->setEnabled(true);

    const int code = response.value("code").toInt(-1);
    QString message = response.value("msg").toString();

    const QJsonObject data = response.value("data").toObject();

    if (code != 0) {
        if (action == QStringLiteral("start_navigation")
            || action == QStringLiteral("switch_navigation_mode")
            || action == QStringLiteral("end_navigation")) {
            m_navigationPage->showError(response.value("msg").toString());
            if (action == QStringLiteral("end_navigation")) {
                leaveNavigationPage();
            }
            return;
        }
        if (action == QStringLiteral("prepare_settlement")) {
            m_settlingOrderId = -1;
            QMessageBox::warning(this, tr("结算失败"), response.value("msg").toString());
            return;
        }
        if (action == QStringLiteral("settle_order")) {
            if (m_balanceBeforeSettlement >= 0.0) {
                m_currentUser["balance"] = m_balanceBeforeSettlement;
                updateBalanceLabels(m_balanceBeforeSettlement);
                m_balanceBeforeSettlement = -1.0;
            }
            if (m_paymentPreview) {
                m_paymentPreview->reject();
            }
        }
        if (action == QStringLiteral("query_user_ongoing_order")) {
            m_chargePagePending = false;
            ui->stackedWidget->setCurrentWidget(ui->pageCharge);
            ui->BtnCharge->setChecked(true);
            ui->BtnHome->setChecked(false);
            ui->BtnMine->setChecked(false);
            if (connection->isConnected()) {
                on_BtnLoadOrderStation_clicked();
            }
            m_pendingPileId = -1;
        }
        QMessageBox::warning(this, tr("请求失败"), response.value("msg").toString());
        return;
    }

    if (handleChargingResponse(action, data, response)) {
        return;
    }
    if (handleAccountResponse(action, data, response)) {
        return;
    }
}
