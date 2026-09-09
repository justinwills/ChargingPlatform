#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "admin/adminwindow.h"

#include <QMessageBox>
#include <QJsonArray>
#include <QRegularExpression>
#include <QtGlobal>
#include <QJsonObject>
#include <QFileDialog>
#include <QPixmap>
#include <QPainter>
#include <QPainterPath>
#include <QButtonGroup>
#include <QGraphicsDropShadowEffect>
#include <QFrame>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLayoutItem>
#include <QPushButton>
#include <QLabel>
#include <QIcon>
#include <QToolButton>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
        , connection(new ClientConnection(this))
{
    ui->setupUi(this);

    ui->stackedWidget->setCurrentWidget(ui->pageLogin);
    ui->widgetNavigation->hide();

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
    applyShadow(ui->stationResults);
    applyShadow(ui->labelOrderStationDetails);
    applyShadow(ui->walletCard);

    QButtonGroup *navGroup = new QButtonGroup(this);
    navGroup->addButton(ui->BtnHome);
    navGroup->addButton(ui->BtnCharge);
    navGroup->addButton(ui->BtnMine);

    navGroup->setExclusive(true);
    ui->BtnHome->setChecked(true);

    setupNavIcons();

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

    orderTimer.setInterval(5000);
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
    QMessageBox::information(
        this,
        tr("导航"),
        tr("已为您规划前往「%1」的充电导航路线，稍后将根据实时路况更新。")
            .arg(m_lastStationName));
}

void MainWindow::on_BtnMine_clicked()
{
    ui->stackedWidget->setCurrentWidget(ui->pageMine);
}

void MainWindow::on_BtnAdmin_clicked()
{
    auto *adminWindow = new AdminWindow(this);
    adminWindow->setAttribute(Qt::WA_DeleteOnClose);
    adminWindow->show();
}

// 登录按钮
void MainWindow::on_Btnlogin_clicked()
{
    phoneNumber = ui->editPhone->text();

    QRegularExpression regex("1\\d{10}$");

    if (!regex.match(phoneNumber).hasMatch()) {
        QMessageBox::warning(
            this,
            tr("登录失败"),
            tr("请输入正确的11位手机号")
            );
        return;
    }

    // 防止用户连续点击产生多个登录请求
    ui->Btnlogin->setEnabled(false);
    m_pendingAction = "login";
    if (connection->isConnected()) {
        connection->sendRequest(
            "login",
            {{"phone", phoneNumber}}
            );
        return;
    }

    connect(
        connection,
        &ClientConnection::connected,
        this,
        [this]() {
            connection->sendRequest(
                "login",
                {{"phone", phoneNumber}}
                );
        },
        Qt::SingleShotConnection
        );

    connection->connectToServer("127.0.0.1", 8888);
}

QPixmap circularPixmap(const QPixmap &source,int size){
    QPixmap scaled = source.scaled(
        size,
        size,
        Qt::KeepAspectRatioByExpanding,
        Qt::SmoothTransformation
        );

    int x = (scaled.width() - size)/ 2;
    int y = (scaled.height() - size)/ 2;

    QPixmap result(size,size);
    result.fill(Qt::transparent);

    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing);

    QPainterPath path;
    path.addEllipse(0,0,size,size);
    painter.setClipPath(path);

    painter.drawPixmap(
        QRect(0,0,size,size),
        scaled,
        QRect(x,y,size,size)
        );

    return result;
}

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

void MainWindow::on_BtnStationDetail_clicked()
{
    m_showStationDetailPage = true;
    connection->sendRequest(QStringLiteral("query_station_detail"), {
        {"stationId", ui->spinStationId->value()}
    });
}

void MainWindow::onServerResponse(const QJsonObject &response)
{
    QString action = m_pendingAction;
    m_pendingAction.clear();

    ui->Btnlogin->setEnabled(true);
    ui->BtnConfirm_PageEdit->setEnabled(true);
    ui->BtnConfirm_pageRecharge->setEnabled(true);

    const int code = response.value("code").toInt(-1);
    QString message = response.value("msg").toString();

    const QJsonObject data = response.value("data").toObject();

    if (code != 0) {
        if (action == QStringLiteral("prepare_settlement")) {
            m_settlingOrderId = -1;
            QMessageBox::warning(this, tr("结算失败"), response.value("msg").toString());
            return;
        }
        if (action == QStringLiteral("settle_order") && m_paymentPreview) {
            m_paymentPreview->reject();
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
                return;
            }
        }
        ui->stackedWidget->setCurrentWidget(ui->pageCharge);
        ui->BtnCharge->setChecked(true);
        ui->BtnHome->setChecked(false);
        ui->BtnMine->setChecked(false);
        if (connection->isConnected()) {
            on_BtnLoadOrderStation_clicked();
        }
        return;
    }

    if (data.contains("stations")) {
        const QJsonArray stations = data.value("stations").toArray();
        m_lastStations = stations;
        ui->labelNearbyCount->setText(tr("共 %1 个电站").arg(stations.size()));
        if (stations.isEmpty()) {
            ui->stationResults->setPlainText(tr("没有找到匹配的充电站"));
            return;
        }

        QStringList lines;
        for (const QJsonValue &value : stations) {
            const QJsonObject station = value.toObject();
            const QString distance = station.contains("distanceKm")
                ? tr("距离：%1 公里\n").arg(station.value("distanceKm").toDouble(), 0, 'f', 2)
                : QString();
            lines << tr("%1\n地址：%2\n%3空闲电桩：%4/%5\n在线率：%6%\n单价：%7 元/度")
                          .arg(station.value("name").toString())
                          .arg(station.value("address").toString())
                          .arg(distance)
                          .arg(station.value("freePileCount").toInt())
                          .arg(station.value("pileCount").toInt())
                          .arg(station.value("onlineRate").toDouble(), 0, 'f', 1)
                          .arg(station.value("price").toDouble(), 0, 'f', 2);
        }
        ui->stationResults->setPlainText(lines.join(QStringLiteral("\n\n")));
        return;
    }

    if (data.contains("piles") && data.contains("stationId")) {
        m_lastStationLat = data.value("latitude").toDouble();
        m_lastStationLng = data.value("longitude").toDouble();
        m_lastStationName = data.value("name").toString(
            data.value("stationName").toString());
        populateStationDetail(data);

        QStringList orderDetails;
        bool selectedIdlePile = false;
        ui->comboPile->clear();
        const QString stationName = data.value("name").toString(
            data.value("stationName").toString());
        orderDetails << tr("站点：%1\n地址：%2\n价格：%3 元/度")
                            .arg(stationName)
                            .arg(data.value("address").toString(
                                data.value("stationAddress").toString()))
                            .arg(data.value("price").toDouble(), 0, 'f', 2);
        orderDetails << QStringLiteral("电桩：");
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

            if (!selectedIdlePile
                && pile.value("status").toString() == QStringLiteral("闲置")) {
                ui->comboPile->setCurrentIndex(ui->comboPile->count() - 1);
                selectedIdlePile = true;
            }
        }
        if (m_pendingPileId > 0) {
            selectPileInCombo(m_pendingPileId);
            m_pendingPileId = -1;
        }
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
        return;
    }

    if(action == "update_user_profile" || action == "login"){

        if( !data.contains("userId") || !data.contains("phone") ||
            !data.contains("nickname")){
            QMessageBox::warning(
                this,
                tr("提示"),
                tr("服务器返回的信息不完整")
                );
            return;
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
                ui->stackedWidget->setCurrentWidget(ui->pageHome);
                connection->sendRequest(QStringLiteral("query_stations"), {});
            }
            ui->widgetNavigation->show();
            ui->label_title_mine_6->setText(
                tr("欢迎%1").arg(data.value("nickname").toString()));

            QMessageBox::information(
                this,
                tr("登陆成功"),
                tr("欢迎,%1").arg(data.value("nickname").toString())
                );
            return;
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

            return;
        }
    }

    if(action == "recharge_balance"){
        QJsonObject data = response.value("data").toObject();

        if(!data.contains("userId") || !data.contains("balance")){
            QMessageBox::warning(this,tr("充值失败"),tr("服务器返回信息不完整"));
            return;
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

        return;
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
            return;
        }
        // 收到的是发起结算前就发出的旧订单状态回包，继续等待真正的
        // prepare_settlement 结果，避免误弹结算窗。
        m_pendingAction = QStringLiteral("prepare_settlement");
        return;
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
            ui->labelMonStatus->setText(tr("正在充电 · 数据每 5 秒刷新"));
            ui->labelMonKwh->setText(QStringLiteral("%1 kWh").arg(estAmount, 0, 'f', 2));
            ui->labelMonFee->setText(QStringLiteral("¥%1").arg(estFee, 0, 'f', 2));
            ui->labelMonSoc->setText(
                QStringLiteral("%1%").arg(qBound(5, 8 + durationMinutes, 99)));
            ui->ringCharge->setValue(qBound(5, 8 + durationMinutes, 99));
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
            ui->stackedWidget->setCurrentWidget(ui->pageChargeMonitor);
            ui->BtnCharge->setChecked(true);
            ui->BtnHome->setChecked(false);
            ui->BtnMine->setChecked(false);
        }
        if (status == QStringLiteral("待结算")) {
            currentAmount = data.value("amount").toDouble(estAmount);
            currentFee = data.value("fee").toDouble(estFee);
            orderTimer.stop();
            displayTimer.stop();
            ui->labelMonStatus->setText(tr("充电已完成，等待结算"));
            ui->ringCharge->setValue(100);
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
            connection->sendRequest(QStringLiteral("login"), {{"phone", phoneNumber}});
        }
        return;
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
        return;
    }

    if (action == QStringLiteral("settle_order") && m_paymentPreview) {
        ui->stackedWidget->setCurrentWidget(ui->pageCharge);
        ui->BtnCharge->setChecked(true);
        ui->BtnHome->setChecked(false);
        ui->BtnMine->setChecked(false);
        m_paymentPreview->showPaymentSuccess();
    }

    if (activeOrderId >= 0) {
        ui->labelMonStatus->setText(tr("订单已结算，余额已更新"));
        ui->labelElapsedTime->setText(QStringLiteral("00:00:00"));
        activeOrderStartTime = QDateTime();
        orderTimer.stop();
        const int settledOrderId = activeOrderId;
        activeOrderId = -1;
        displayTimer.stop();
        connection->sendRequest(QStringLiteral("query_order"), {
            {"orderId", settledOrderId}
        });
        connection->sendRequest(QStringLiteral("login"), {{"phone", phoneNumber}});
    }
}

// 进入修改用户信息界面
void MainWindow::on_BtnSetting_clicked()
{
    ui->lineEditNickname->setText(m_currentUser.value("nickname").toString());

    m_selectedAvatarPath = m_currentUser.value("avatarPath").toString();
    ui->labelPhotoEdit->setPixmap(ui->labelPhoto->pixmap());

    ui->widgetNavigation->hide();
    ui->stackedWidget->setCurrentWidget(ui->pageEditMine);

}

// 选择更改头像按钮
void MainWindow::on_BtnChoosePhoto_clicked()
{
    QString fileName = QFileDialog::getOpenFileName(
        this,
        "选择头像",
        "",
        "图片文件 (*.png *.jpg *.jpeg)"
        );

    if (fileName.isEmpty())
        return;

    QPixmap avatar(fileName);

    if (avatar.isNull()) {
        QMessageBox::warning(this, tr("提示"), tr("无法读取该图片"));
        return;
    }

    m_selectedAvatarPath = fileName;

    // 先在修改页面预览
    ui->labelPhotoEdit->setPixmap(circularPixmap(avatar,100));
}

// 确认更改用户信息
void MainWindow::on_BtnConfirm_PageEdit_clicked()
{
    QString nickname = ui->lineEditNickname->text().trimmed();

    if (nickname.isEmpty()) {
        QMessageBox::warning(this,tr("提示"), tr("昵称不能为空"));
        return;
    }

    if(!connection->isConnected()){
        QMessageBox::warning(this,tr("提示"),tr("尚未连接服务器"));
        return;
    }

    int userId = m_currentUser.value("userId").toInt(-1);

    if(userId <= 0){
        QMessageBox::warning(this,tr("提示"),tr("当前用户信息无效"));
        return;
    }

    QJsonObject params;
    params["userId"] = userId;
    params["nickname"] = nickname;
    params["avatarPath"] = m_selectedAvatarPath;

    m_pendingAction = "update_user_profile";

    ui->BtnConfirm_PageEdit->setEnabled(false);

    connection->sendRequest(
        "update_user_profile",
        params
        );
}

// 取消更改用户信息按钮
void MainWindow::on_BtnCancel_clicked()
{
    ui->widgetNavigation->show();
    ui->stackedWidget->setCurrentWidget(ui->pageMine);
}

void MainWindow::on_BtnRecharge_clicked()
{
    ui->widgetNavigation->hide();
    ui->stackedWidget->setCurrentWidget(ui->pageRecharge);
}

void MainWindow::on_Btn_20_clicked()
{
    ui->editRecharge->setText(QStringLiteral("20"));
}

void MainWindow::on_Btn_50_clicked()
{
    ui->editRecharge->setText(QStringLiteral("50"));
}

void MainWindow::on_Btn_100_clicked()
{
    ui->editRecharge->setText(QStringLiteral("100"));
}

void MainWindow::on_Btn_200_clicked()
{
    ui->editRecharge->setText(QStringLiteral("200"));
}

void MainWindow::on_BtnConfirm_pageRecharge_clicked()
{
    QString moneyText = ui->editRecharge->text().trimmed();
    if (moneyText.isEmpty()) {
        QMessageBox::warning(this, tr("提示"), tr("金额不能为空"));
        return;
    }

    bool ok;
    int money = moneyText.toInt(&ok);

    if (!ok){
        QMessageBox::warning(this,tr("提示"), tr("金额必须是整数"));
        return;
    }
    if (money<=0){
        QMessageBox::warning(this,tr("提示"), tr("金额必须大于零"));
        return;
    }

    int userId = m_currentUser.value("userId").toInt(-1);

    if(userId <= 0){
        QMessageBox::warning(this,tr("充值失败"), tr("当前用户信息失效"));
        return;
    }

    QJsonObject params;
    params["userId"] = userId;
    params["amount"] = money;

    m_pendingAction = "recharge_balance";
    ui->BtnConfirm_pageRecharge->setEnabled(false);

    connection->sendRequest("recharge_balance",params);
}


void MainWindow::on_BtnCancel_pageRecharge_clicked()
{
    ui->widgetNavigation->show();
    ui->stackedWidget->setCurrentWidget(ui->pageMine);
}

void MainWindow::on_BtnLeave_clicked()
{
    // 清除当前用户信息
    m_currentUser = QJsonObject{};
    phoneNumber.clear();
    m_selectedAvatarPath.clear();
    m_pendingAction.clear();
    m_chargePagePending = false;

    // 清除界面中的旧用户数据
    ui->editPhone->clear();
    ui->editRecharge->clear();
    ui->labelNickname->clear();
    ui->labelPhone->clear();
    ui->labelMoney_c->clear();

    // 隐藏导航栏并返回登录页
    ui->widgetNavigation->hide();
    ui->stackedWidget->setCurrentWidget(ui->pageLogin);
}

// 网络错误处理
void MainWindow::onConnectionError(const QString &message){
    ui->Btnlogin->setEnabled(true);
    ui->BtnConfirm_pageRecharge->setEnabled(true);
    ui->BtnConfirm_PageEdit->setEnabled(true);

    m_pendingAction.clear();
    m_chargePagePending = false;
    m_settlingOrderId = -1;

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
    ui->labelRechargeBalance->setText(QStringLiteral("¥ %1").arg(balance, 0, 'f', 2));
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

void MainWindow::populateStationDetail(const QJsonObject &data)
{
    const QString stationName = data.value("name").toString(
        data.value("stationName").toString());
    const QString address = data.value("address").toString(
        data.value("stationAddress").toString());

    ui->labelSDName->setText(stationName);
    ui->labelSDAddress->setText(address);
    ui->labelSDFree->setText(QStringLiteral("%1/%2")
                                  .arg(data.value("freePileCount").toInt())
                                  .arg(data.value("pileCount").toInt()));
    ui->labelSDOnline->setText(QStringLiteral("%1%")
                                   .arg(data.value("onlineRate").toDouble(), 0, 'f', 0));
    ui->labelSDPrice->setText(QStringLiteral("¥%1/度")
                                  .arg(data.value("price").toDouble(), 0, 'f', 2));

    while (QLayoutItem *child = ui->pileLayout->takeAt(0)) {
        if (QWidget *w = child->widget()) {
            w->deleteLater();
        }
        delete child;
    }

    const QJsonArray piles = data.value("piles").toArray();
    for (const QJsonValue &value : piles) {
        const QJsonObject pile = value.toObject();
        const QString status = pile.value("status").toString();
        const bool idle = (status == QStringLiteral("闲置"));

        auto *card = new QFrame(ui->pileHost);
        card->setObjectName(QStringLiteral("pileCard"));
        auto *row = new QHBoxLayout(card);
        row->setContentsMargins(14, 10, 12, 10);
        row->setSpacing(10);

        auto *info = new QLabel(card);
        info->setTextFormat(Qt::RichText);
        info->setText(
            QStringLiteral("<span style=\"font-size:15px;font-weight:700;color:#131b2e;\">%1</span>"
                           "<span style=\"color:#737686;font-size:12px;\">&nbsp;%2 kW</span>"
                           "<br><span style=\"color:#434655;font-size:12px;\">%3 · 累计使用 %4 次</span>")
                .arg(pile.value("code").toString())
                .arg(pile.value("power").toDouble(), 0, 'f', 1)
                .arg(pile.value("type").toString())
                .arg(pile.value("totalSessions").toInt()));
        row->addWidget(info, 1);

        if (idle) {
            auto *btn = new QPushButton(tr("开始充电"), card);
            btn->setObjectName(QStringLiteral("pileStartBtn"));
            const int pileId = pile.value("pileId").toInt();
            const int stationId = data.value("stationId").toInt();
            connect(btn, &QPushButton::clicked, this, [this, pileId, stationId]() {
                m_pendingPileId = pileId;
                if (stationId > 0) {
                    ui->spinOrderStationId->setValue(stationId);
                }
                openChargePage();
            });
            row->addWidget(btn);
        } else {
            auto *badge = new QLabel(status, card);
            badge->setObjectName(status == QStringLiteral("充电中")
                                     ? QStringLiteral("badgeCharging")
                                     : QStringLiteral("badgeFault"));
            row->addWidget(badge);
        }
        ui->pileLayout->addWidget(card);
    }
    ui->pileLayout->addStretch();
}

