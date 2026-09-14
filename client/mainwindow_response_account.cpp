#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QMessageBox>
#include <QJsonObject>
#include <QPixmap>

// Handles the "account domain" server responses: profile update/login,
// and balance recharge. Called from onServerResponse() (mainwindow.cpp)
// on the success path, after handleChargingResponse(). Returns true if
// `action` was handled.
bool MainWindow::handleAccountResponse(const QString &action, const QJsonObject &data,
                                        const QJsonObject &response)
{
    if(action == "update_user_profile" || action == "login"){

        if( !data.contains("userId") || !data.contains("phone") ||
            !data.contains("nickname")){
            QMessageBox::warning(
                this,
                tr("提示"),
                tr("服务器返回的信息不完整")
                );
            return true;
        }

        // 保存最新用户的完整信息
        m_currentUser = data;
        userId = data.value("userId").toInt(-1);
        phoneNumber = data.value("phone").toString();

        QString nickname = data.value("nickname").toString();
        QString phone = data.value("phone").toString();
        QString avatarPath = data.value("avatarPath").toString();
        double balance = data.value("balance").toDouble();

        // 将服务器返回的信息显示到“我的”页面
        ui->labelNickname->setText(nickname);

        ui->labelPhone->setText(phone);

        ui->labelMoney_c->setText(
            QStringLiteral("¥ %1").arg(balance, 0, 'f', 2));
        updateBalanceLabels(balance);

        QPixmap avatar;

        // 有自定义头像时先尝试加载
        if (!avatarPath.isEmpty()) {
            avatar.load(avatarPath);
        }

        // 没有自定义头像，或自定义头像加载失败时，显示默认头像
        if (avatar.isNull()) {
            avatar.load(":/images/default_avatar.jpeg");
        }

        if (!avatar.isNull()) {
            ui->labelPhoto->setFixedSize(80,80);
            ui->labelPhoto->setAlignment(Qt::AlignCenter);
            ui->labelPhoto->setPixmap(circularPixmap(avatar,80));
        } else {
            qWarning() << "默认头像加载失败";
        }

        // 登录成功后才进入主界面
        if(action == "login"){
            const bool backgroundRefresh = m_backgroundBalanceRefresh;
            m_backgroundBalanceRefresh = false;

            const int ongoingOrderId = data.value("ongoingOrderId").toInt(-1);
            if (ongoingOrderId > 0) {
                activeOrderId = ongoingOrderId;
                ui->stackedWidget->setCurrentWidget(ui->pageChargeMonitor);
                ui->labelMonStatus->setText(
                    tr("正在恢复订单 %1，请稍候...").arg(activeOrderId));
                connection->sendRequest(QStringLiteral("query_order"), {
                    {"orderId", activeOrderId}
                });
            } else {
                activeOrderId = -1;
                currentAmount = 0;
                currentFee = 0;
                activeOrderStartTime = QDateTime();
                orderTimer.stop();
                displayTimer.stop();
                // 后台余额刷新不抢走用户当前页面，仅更新钱包数值。
                if (!backgroundRefresh) {
                    ui->stackedWidget->setCurrentWidget(ui->pageHome);
                    connection->sendRequest(QStringLiteral("query_stations"), {});
                }
            }
            ui->widgetNavigation->show();
            ui->label_title_mine_6->setText(
                tr("欢迎%1").arg(data.value("nickname").toString()));

            if (!backgroundRefresh) {
                QMessageBox::information(
                    this,
                    tr("登陆成功"),
                    tr("欢迎,%1").arg(data.value("nickname").toString())
                    );
            }
            return true;
        }

        if(action == "update_user_profile"){
            m_selectedAvatarPath = avatarPath;

            ui->widgetNavigation->show();
            ui->stackedWidget->setCurrentWidget(ui->pageMine);

            QMessageBox::information(
                this,
                tr("更改成功"),
                tr("数据库中的用户资料已更新")
                );

            return true;
        }
    }

    if(action == "recharge_balance"){
        QJsonObject data = response.value("data").toObject();

        if(!data.contains("userId") || !data.contains("balance")){
            QMessageBox::warning(this,tr("充值失败"),tr("服务器返回信息不完整"));
            return true;
        }

        m_currentUser = data;

        double balance = data.value("balance").toDouble();

        ui->labelMoney_c->setText(QStringLiteral("¥ %1").arg(balance, 0, 'f', 2));
        updateBalanceLabels(balance);

        ui->editRecharge->clear();
        ui->widgetNavigation->show();
        ui->stackedWidget->setCurrentWidget(ui->pageMine);

        QMessageBox::information(this,
                                 tr("充值成功"),
                                 QString("当前余额: %1 元").arg(balance,0,'f',2));

        return true;
    }


    if (m_settlingOrderId > 0) {
        const int responseOrderId = data.value("orderId").toInt(-1);
        const QString responseStatus = data.value("status").toString();
        const bool genuinePrepare =
            responseOrderId > 0
            && responseStatus == QStringLiteral("待结算")
            && !data.contains("startTime");
        if (genuinePrepare) {
            const int settlingId = m_settlingOrderId;
            m_settlingOrderId = -1;
            activeOrderId = settlingId;
            currentAmount = data.value("amount").toDouble(currentAmount);
            currentFee = data.value("fee").toDouble(currentFee);
            m_lastOrder = data;
            orderTimer.stop();
            displayTimer.stop();
            ui->labelMonStatus->setText(
                tr("充电已完成，正在生成账单…"));
            showPaymentPreview();
            return true;
        }
        // 收到的是发起结算前就发出的旧订单状态回包，继续等待真正的
        // prepare_settlement 结果，避免误弹结算窗。
        m_pendingAction = QStringLiteral("prepare_settlement");
        return true;
    }

    if (data.contains("status")) {
        const QString status = data.value("status").toString();
        if (data.contains("orderId")) {
            activeOrderId = data.value("orderId").toInt();
            m_lastOrder = data;
        }
        const int durationMinutes = data.value("durationMinutes").toInt();
        const double estAmount = data.value("estimatedAmount").toDouble();
        const double estFee = data.value("estimatedFee").toDouble();
        if (status == QStringLiteral("充电中")) {
            currentAmount = estAmount;
            currentFee = estFee;
            activeOrderStartTime = QDateTime::fromString(
                data.value("startTime").toString(),
                QStringLiteral("yyyy-MM-dd HH:mm:ss"));
            const int socPercent = data.value("socPercent").toInt(
                qBound(5, 8 + durationMinutes, 99));
            const int remainingSeconds = data.value("remainingSeconds").toInt(60);
            ui->labelMonStatus->setText(tr("正在充电 · 数据每 1 秒刷新"));
            ui->labelMonKwh->setText(QStringLiteral("%1 kWh").arg(estAmount, 0, 'f', 2));
            ui->labelMonFee->setText(QStringLiteral("¥%1").arg(estFee, 0, 'f', 2));
            ui->labelMonSoc->setText(QStringLiteral("%1%").arg(socPercent));
            ui->ringCharge->setValue(socPercent);
            ui->labelMonEta->setText(formatRemainingChargeTime(remainingSeconds));
            if (!ui->labelMonOrderId->text().contains('#')) {
                ui->labelMonOrderId->setText(
                    tr("订单号：#%1 · 电桩 #%2")
                        .arg(activeOrderId)
                        .arg(data.value("pileId").toInt()));
            }
            ui->labelMonStart->setText(
                tr("开始时间：%1").arg(data.value("startTime").toString()));
            displayTimer.start();
            orderTimer.start();
            // 仅在用户当前位于充电相关页面时才切回充电监控页，
            // 避免5秒一轮的后台刷新把用户从其他页面强行拽回充电页。
            if (ui->stackedWidget->currentWidget() == ui->pageChargeMonitor
                || ui->stackedWidget->currentWidget() == ui->pageCharge) {
                ui->stackedWidget->setCurrentWidget(ui->pageChargeMonitor);
                ui->BtnCharge->setChecked(true);
                ui->BtnHome->setChecked(false);
                ui->BtnMine->setChecked(false);
            }
        }
        if (status == QStringLiteral("待结算")) {
            currentAmount = data.value("amount").toDouble(estAmount);
            currentFee = data.value("fee").toDouble(estFee);
            orderTimer.stop();
            displayTimer.stop();
            ui->labelMonStatus->setText(tr("充电已完成，等待结算"));
            ui->labelMonSoc->setText(QStringLiteral("100%"));
            ui->ringCharge->setValue(100);
            ui->labelMonEta->setText(tr("已充满，订单已结束"));
        }
        if (status == QStringLiteral("已结算")) {
            activeOrderStartTime = QDateTime();
            activeOrderId = -1;
            currentAmount = 0;
            currentFee = 0;
            ui->labelElapsedTime->setText(QStringLiteral("00:00:00"));
            ui->labelMonKwh->setText(QStringLiteral("0.0 kWh"));
            ui->labelMonFee->setText(QStringLiteral("¥0.00"));
            orderTimer.stop();
            displayTimer.stop();
            m_backgroundBalanceRefresh = true;
            m_pendingAction = QStringLiteral("login");
            connection->sendRequest(QStringLiteral("login"), {{"phone", phoneNumber}});
        }
        return true;
    }

    if (data.contains("orderId")) {
        activeOrderId = data.value("orderId").toInt();
        activeOrderStartTime = QDateTime::currentDateTime();
        m_lastOrder = data;
        ui->labelMonStation->setText(
            m_lastStationName.isEmpty() ? QStringLiteral("睿光充电站") : m_lastStationName);
        ui->labelMonStatus->setText(tr("订单创建成功，正在开始充电…"));
        ui->labelMonOrderId->setText(tr("订单号：#%1").arg(activeOrderId));
        ui->labelMonStart->setText(
            tr("开始时间：%1")
                .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))));
        ui->labelMonSoc->setText(QStringLiteral("5%"));
        ui->ringCharge->setValue(5);
        ui->labelMonEta->setText(formatRemainingChargeTime(60));
        ui->labelMonKwh->setText(QStringLiteral("0.0 kWh"));
        ui->labelMonFee->setText(QStringLiteral("¥0.00"));
        ui->labelElapsedTime->setText(QStringLiteral("00:00:00"));
        ui->stackedWidget->setCurrentWidget(ui->pageChargeMonitor);
        ui->BtnCharge->setChecked(true);
        ui->BtnHome->setChecked(false);
        ui->BtnMine->setChecked(false);
        displayTimer.start();
        orderTimer.start();
        on_BtnRefreshOrder_clicked();
        return true;
    }

    return false;
}
