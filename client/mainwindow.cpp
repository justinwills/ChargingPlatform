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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
        , connection(new ClientConnection(this))
{
    ui->setupUi(this);

    ui->stackedWidget->setCurrentWidget(ui->pageLogin);
        connect(connection, &ClientConnection::responseReceived,
            this, &MainWindow::onServerResponse);
        connect(connection, &ClientConnection::connectionError,
            this, &MainWindow::onConnectionError);

            orderTimer.setInterval(5000);
            connect(&orderTimer, &QTimer::timeout,
                this, &MainWindow::on_BtnRefreshOrder_clicked);

            displayTimer.setInterval(1000);
            connect(&displayTimer, &QTimer::timeout, this, [this]() {
                if (!activeOrderStartTime.isValid() || activeOrderId < 0) {
                    return;
                }

                const qint64 elapsedSeconds = qMax<qint64>(
                    0, activeOrderStartTime.secsTo(QDateTime::currentDateTime()));
                const int hours = static_cast<int>(elapsedSeconds / 3600);
                const int minutes = static_cast<int>((elapsedSeconds % 3600) / 60);
                const int seconds = static_cast<int>(elapsedSeconds % 60);
                ui->labelElapsedTime->setText(
                    tr("已充电时间：%1:%2:%3")
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
}

void MainWindow::on_BtnCharge_clicked()
{
    ui->stackedWidget->setCurrentWidget(ui->pageCharge);
    if (connection->isConnected()) {
        on_BtnLoadOrderStation_clicked();
    }
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

    if (!regex.match(phoneNumber).hasMatch()){
        QMessageBox::warning(this,tr("警告"),tr("手机号格式错误！"),QMessageBox::Yes);
        return;
    }
    connection->connectToServer(QStringLiteral("127.0.0.1"), 8888);
    connect(connection, &ClientConnection::connected, this, [this]() {
        connection->sendRequest(QStringLiteral("login"), {{"phone", phoneNumber}});
    }, Qt::SingleShotConnection);
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
        ui->labelOrderStatus->setText(tr("当前没有可刷新的订单，请先开始充电"));
        return;
    }

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

    connection->sendRequest(QStringLiteral("settle_order"), {
        {"orderId", activeOrderId},
        {"amount", currentAmount},
        {"fee", currentFee}
    });
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
    connection->sendRequest(QStringLiteral("query_station_detail"), {
        {"stationId", ui->spinStationId->value()}
    });
}

void MainWindow::onServerResponse(const QJsonObject &response)
{
    const int code = response.value("code").toInt(-1);
    const QJsonObject data = response.value("data").toObject();

    if (code != 0) {
        QMessageBox::warning(this, tr("请求失败"), response.value("msg").toString());
        return;
    }

    if (data.contains("stations")) {
        const QJsonArray stations = data.value("stations").toArray();
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

        QStringList lines;
        const QString stationName = data.value("name").toString(
            data.value("stationName").toString());
        lines << tr("充电站：%1\n地址：%2\n价格：%3 元/度")
                      .arg(stationName)
                      .arg(data.value("address").toString(
                          data.value("stationAddress").toString()))
                      .arg(data.value("price").toDouble(), 0, 'f', 2);
        lines << QStringLiteral("\n电桩列表：");
        for (const QJsonValue &value : data.value("piles").toArray()) {
            const QJsonObject pile = value.toObject();
            lines << tr("%1 | %2 | %3 kW | 状态：%4")
                          .arg(pile.value("code").toString())
                          .arg(pile.value("type").toString())
                          .arg(pile.value("power").toDouble(), 0, 'f', 1)
                          .arg(pile.value("status").toString());
        }
                ui->stationResults->setPlainText(lines.join(QStringLiteral("\n")));
        QStringList orderDetails;
        bool selectedIdlePile = false;
        ui->comboPile->clear();
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
        return;
    }

    if (data.contains("userId") && data.contains("nickname")) {
        userId = data.value("userId").toInt();
        ui->editUserName->setText(data.value("nickname").toString());
        ui->editPhoneNumber->setText(data.value("phone").toString());
        ui->editMoney->setText(QString::number(data.value("balance").toDouble()));
        const QString avatarPath = data.value("avatarPath").toString();
        QPixmap avatar(avatarPath);
        if (avatar.isNull()) {
            avatar.load(QStringLiteral(":/images/default_avatar.jpeg"));
        }
        if (!avatar.isNull()) {
            ui->labelPhoto->setPixmap(avatar.scaled(
                ui->labelPhoto->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
        if (pendingAction == QStringLiteral("recharge_balance")) {
            pendingAction.clear();
            QMessageBox::information(this, tr("充值成功"), tr("余额已更新"));
        }
        ui->stackedWidget->setCurrentWidget(ui->pageHome);

        if (data.contains("ongoingOrderId")) {
            activeOrderId = data.value("ongoingOrderId").toInt();
            ui->stackedWidget->setCurrentWidget(ui->pageCharge);
            on_BtnRefreshOrder_clicked();
            orderTimer.start();
        }
        return;
    }

    if (data.contains("status")) {
        const QString status = data.value("status").toString();
        activeOrderId = data.value("orderId").toInt();
        ui->labelOrderStatus->setText(
            tr("订单 %1：%2\n充电时长：%3 分钟，电量：%4，预计费用：%5")
                .arg(data.value("orderId").toInt())
                .arg(status)
                .arg(data.value("durationMinutes").toInt())
                .arg(data.value("estimatedAmount").toDouble())
                .arg(data.value("estimatedFee").toDouble()));
        if (status == QStringLiteral("充电中")) {
            activeOrderStartTime = QDateTime::fromString(
                data.value("startTime").toString(),
                QStringLiteral("yyyy-MM-dd HH:mm:ss"));
            currentAmount = data.value("estimatedAmount").toDouble();
            currentFee = data.value("estimatedFee").toDouble();
            displayTimer.start();
        }
        if (status == QStringLiteral("已结算")) {
            activeOrderId = -1;
            orderTimer.stop();
            displayTimer.stop();
            connection->sendRequest(QStringLiteral("login"), {{"phone", phoneNumber}});
        }
        return;
    }

    if (data.contains("orderId")) {
        activeOrderId = data.value("orderId").toInt();
        activeOrderStartTime = QDateTime::currentDateTime();
        ui->labelOrderStatus->setText(
            tr("充电已开始，订单号：%1").arg(data.value("orderId").toInt()));
        ui->labelElapsedTime->setText(tr("已充电时间：00:00:00"));
        ui->stackedWidget->setCurrentWidget(ui->pageCharge);
        displayTimer.start();
        orderTimer.start();
        on_BtnRefreshOrder_clicked();
        return;
    }

    if (activeOrderId >= 0) {
        ui->labelOrderStatus->setText(tr("订单已结算，余额已更新"));
        orderTimer.stop();
        const int settledOrderId = activeOrderId;
        activeOrderId = -1;
        connection->sendRequest(QStringLiteral("query_order"), {
            {"orderId", settledOrderId}
        });
        connection->sendRequest(QStringLiteral("login"), {{"phone", phoneNumber}});
    }
}

void MainWindow::on_BtnSetting_clicked()
{
    ui->lineEditNickname->setText(ui->editUserName->text());
    ui->labelPhotoEdit->setPixmap(ui->labelPhoto->pixmap());
    ui->stackedWidget->setCurrentWidget(ui->pageEditMine);
}

void MainWindow::on_BtnChoosePhoto_clicked()
{
    const QString fileName = QFileDialog::getOpenFileName(
        this, tr("选择头像"), QString(), tr("图片 (*.png *.jpg *.jpeg)"));
    if (fileName.isEmpty()) {
        return;
    }
    const QPixmap avatar(fileName);
    if (avatar.isNull()) {
        QMessageBox::warning(this, tr("提示"), tr("无法读取该图片"));
        return;
    }
    selectedAvatarPath = fileName;
    ui->labelPhotoEdit->setPixmap(avatar.scaled(
        ui->labelPhotoEdit->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void MainWindow::on_BtnConfirm_PageEdit_clicked()
{
    const QString nickname = ui->lineEditNickname->text().trimmed();
    if (nickname.isEmpty() || userId <= 0) {
        QMessageBox::warning(this, tr("提示"), tr("用户信息无效"));
        return;
    }
    pendingAction = QStringLiteral("update_user_profile");
    connection->sendRequest(QStringLiteral("update_user_profile"), {
        {"userId", userId}, {"nickname", nickname},
        {"avatarPath", selectedAvatarPath}
    });
}

void MainWindow::on_BtnCancel_clicked()
{
    ui->stackedWidget->setCurrentWidget(ui->pageMine);
}

void MainWindow::on_BtnRecharge_clicked()
{
    ui->stackedWidget->setCurrentWidget(ui->pageRecharge);
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
    bool ok = false;
    const double amount = ui->editRecharge->text().toDouble(&ok);
    if (!ok || amount <= 0 || userId <= 0) {
        QMessageBox::warning(this, tr("提示"), tr("请输入有效的充值金额"));
        return;
    }
    pendingAction = QStringLiteral("recharge_balance");
    connection->sendRequest(QStringLiteral("recharge_balance"), {
        {"userId", userId}, {"amount", amount}
    });
}

void MainWindow::on_BtnCancel_pageRecharge_clicked()
{
    ui->stackedWidget->setCurrentWidget(ui->pageMine);
}

void MainWindow::on_BtnLeave_clicked()
{
    userId = -1;
    phoneNumber.clear();
    selectedAvatarPath.clear();
    pendingAction.clear();
    ui->stackedWidget->setCurrentWidget(ui->pageLogin);
}

void MainWindow::onConnectionError(const QString &message)
{
    QMessageBox::warning(this, tr("连接失败"), message);
}

