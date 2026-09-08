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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
        , connection(new ClientConnection(this))
{
    ui->setupUi(this);

    ui->stackedWidget->setCurrentWidget(ui->pageLogin);
    ui->widgetNavigation->hide();

    QButtonGroup *navGroup = new QButtonGroup(this);
    navGroup -> addButton(ui->BtnHome);
    navGroup -> addButton(ui->BtnMine);
    navGroup -> addButton(ui->BtnCharge);

    navGroup -> setExclusive(true);
    ui->BtnHome ->setChecked(true);

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
        if (action == QStringLiteral("settle_order") && m_paymentPreview) {
            m_paymentPreview->reject();
        }
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
            QString::number(balance, 'f', 2));

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
            ui->labelPhoto->setFixedSize(100,100);
            ui->labelPhoto->setAlignment(Qt::AlignCenter);
            ui->labelPhoto->setPixmap(circularPixmap(avatar,100));
        } else {
            qWarning() << "默认头像加载失败";
        }

        // 登录成功后才进入主界面
        if(action == "login"){
            const int ongoingOrderId = data.value("ongoingOrderId").toInt(-1);
            if (ongoingOrderId > 0) {
                activeOrderId = ongoingOrderId;
                ui->stackedWidget->setCurrentWidget(ui->pageCharge);
                ui->labelOrderStatus->setText(
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
            }
            ui->widgetNavigation->show();

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

        ui->labelMoney_c->setText(QString::number(balance,'f',2));

        ui->editRecharge->clear();
        ui->widgetNavigation->show();
        ui->stackedWidget->setCurrentWidget(ui->pageMine);

        QMessageBox::information(this,
                                 tr("充值成功"),
                                 QString("当前余额: %1 元").arg(balance,0,'f',2));

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

    if (action == QStringLiteral("settle_order") && m_paymentPreview) {
        m_paymentPreview->showPaymentSuccess();
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

    QMessageBox::warning(
        this,
        tr("网络错误"),
        message
        );
}

