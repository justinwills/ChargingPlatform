#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QMessageBox>
#include <QRegularExpression>
#include <QtGlobal>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
        , connection(new ClientConnection(this))
{
    ui->setupUi(this);

    ui->stackedWidget->setCurrentWidget(ui->pageLogin);
    ui->widgetNavigation->hide();

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
}


void MainWindow::on_BtnMine_clicked()
{
    ui->stackedWidget->setCurrentWidget(ui->pageMine);
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

    connection->sendRequest(QStringLiteral("start_charging"), {
        {"userId", userId},
        {"pileId", ui->spinPileId->value()}
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
        {"amount", ui->spinAmount->value()},
        {"fee", ui->spinFee->value()}
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

    if (data.contains("userId") && data.contains("nickname")) {
        userId = data.value("userId").toInt();
        ui->editUserName->setText(data.value("nickname").toString());
        ui->editPhoneNumber->setText(data.value("phone").toString());
        ui->editMoney->setText(QString::number(data.value("balance").toDouble()));
        ui->stackedWidget->setCurrentWidget(ui->pageHome);
        ui->widgetNavigation->show();

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
            ui->spinAmount->setValue(data.value("estimatedAmount").toDouble());
            ui->spinFee->setValue(data.value("estimatedFee").toDouble());
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

void MainWindow::onConnectionError(const QString &message)
{
    QMessageBox::warning(this, tr("连接失败"), message);
}

