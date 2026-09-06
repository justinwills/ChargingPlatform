#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "clientconnection.h"

#include <QMessageBox>
#include <QRegularExpression>
#include <QFileDialog>
#include <QPixmap>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    ,m_connection(new ClientConnection(this))
{
    ui->setupUi(this);

    ui->stackedWidget->setCurrentWidget(ui->pageLogin);
    ui->widgetNavigation->hide();

    connect(m_connection,
            &ClientConnection::responseReceived,
            this,
            &MainWindow::onServerResponse);

    connect(m_connection,
            &ClientConnection::connectionError,
            this,
            &MainWindow::onConnectionError);

    connect(m_connection,
            &ClientConnection::connected,
            this,
            []() {
                qDebug() << "已经连接到充电平台服务器";
            });

    connect(m_connection,
            &ClientConnection::disconnected,
            this,
            []() {
                qDebug() << "与服务器断开连接";
            });

    m_connection->connectToServer("127.0.0.1", 8888);

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
    phoneNumber = ui->editPhone->text().trimmed();

    static const QRegularExpression phoneRegex("^1\\d{10}$");

    if (!phoneRegex.match(phoneNumber).hasMatch()) {
        QMessageBox::warning(
            this,
            tr("登录失败"),
            tr("请输入正确的11位手机号")
            );
        return;
    }

    if (!m_connection->isConnected()) {
        QMessageBox::warning(
            this,
            tr("连接失败"),
            tr("尚未连接服务器，请确认服务端已经启动")
            );

        m_connection->connectToServer("127.0.0.1", 8888);
        return;
    }

    // 防止用户连续点击产生多个登录请求
    ui->Btnlogin->setEnabled(false);

    QJsonObject params;
    params["phone"] = phoneNumber;

    m_connection->sendRequest("login", params);
}

void MainWindow::onServerResponse(const QJsonObject &response){
    ui->Btnlogin->setEnabled(true);

    int code = response.value("code").toInt(-1);
    QString message = response.value("msg").toString();

    if(code!=0){
        QMessageBox::warning(this,tr("登录失败"),
                             message.isEmpty() ? tr("服务器返回未知错误") : message);
        return;
    }

    QJsonObject data = response.value("data").toObject();

    if( !data.contains("userId") || !data.contains("phone")){
        QMessageBox::warning(
            this,
            tr("登录失败"),
            tr("服务器返回的用户信息不完整")
            );
        return;
    }

    // 保存当前用户的完整信息
    m_currentUser = data;
    phoneNumber = data.value("phone").toString();

    // 将服务器返回的信息显示到“我的”页面
    ui->lineEditNickname->setText(data.value("nickname").toString());

    ui->labelPhone->setText(data.value("phone").toString());

    ui->labelMoney_c->setText(
        QString::number(data.value("balance").toDouble(), 'f', 2)
        );

    QString avatarPath = data.value("avatarPath").toString();
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
        ui->labelPhoto->setAlignment(Qt::AlignCenter);
        ui->labelPhoto->setPixmap(
            avatar.scaled(
                ui->labelPhoto->size(),
                Qt::KeepAspectRatio,
                Qt::SmoothTransformation
                )
            );
    } else {
        qWarning() << "默认头像加载失败";
    }
    // 登录成功后才进入主界面
    ui->stackedWidget->setCurrentWidget(ui->pageHome);
    ui->widgetNavigation->show();

    QMessageBox::information(
        this,
        tr("登陆成功"),
        tr("欢迎,%1").arg(data.value("nickname").toString())
        );
}

// 网络错误处理
void MainWindow::onConnectionError(const QString &message){
    ui->Btnlogin->setEnabled(true);

    QMessageBox::warning(
        this,
        tr("网络错误"),
        message
        );
}

// 进入修改用户信息界面
void MainWindow::on_BtnSetting_clicked()
{
    ui->lineEditNickname->setText(ui->labelNickname->text());
    ui->labelPhotoEdit->setPixmap(ui->labelPhoto->pixmap());

    ui->widgetNavigation->hide();
    ui->stackedWidget->setCurrentWidget(ui->pageEditMine);

}

// 确认更改用户信息
void MainWindow::on_BtnConfirm_PageEdit_clicked()
{
    QString nickname = ui->lineEditNickname->text().trimmed();

    if (nickname.isEmpty()) {
        QMessageBox::warning(this, "提示", "昵称不能为空");
        return;
    }

    // 这里向后台发送修改请求
    // 修改成功后再更新查看页面
    ui->labelNickname->setText(nickname);
    ui->labelPhoto->setPixmap(ui->labelPhotoEdit->pixmap());

    ui->widgetNavigation->show();
    ui->stackedWidget->setCurrentWidget(ui->pageMine);
}

// 取消更改用户信息按钮
void MainWindow::on_BtnCancel_clicked()
{
    ui->widgetNavigation->show();
    ui->stackedWidget->setCurrentWidget(ui->pageMine);
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
        QMessageBox::warning(this, "提示", "无法读取该图片");
        return;
    }

    // 先在修改页面预览
    ui->labelPhotoEdit->setPixmap(
        avatar.scaled(
            ui->labelPhotoEdit->size(),
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation
            )
        );
}

void MainWindow::on_Btn_50_clicked()
{
    ui->editRecharge->setText("50");
}


void MainWindow::on_Btn_100_clicked()
{
    ui->editRecharge->setText("100");
}


void MainWindow::on_Btn_200_clicked()
{
    ui->editRecharge->setText("200");
}


void MainWindow::on_BtnConfirm_pageRecharge_clicked()
{
    QString moneyText = ui->editRecharge->text().trimmed();
    if (moneyText.isEmpty()) {
        QMessageBox::warning(this, "提示", "金额不能为空");
        return;
    }

    bool ok;
    int money = moneyText.toInt(&ok);

    if (!ok){
        QMessageBox::warning(this,"提示", "金额必须是整数");
        return;
    }
    if (money<=0){
        QMessageBox::warning(this,"提示", "金额必须大于零");
        return;
    }
    // 这里向后台发送修改请求(money要改成int)
    // 修改成功后再更新查看页面
    int newMoney = money + 0; // (要加原本的，从BD拿)
    ui->labelMoney_c->setText(QString::number(newMoney));

    ui->widgetNavigation->show();
    ui->stackedWidget->setCurrentWidget(ui->pageMine);

}


void MainWindow::on_BtnRecharge_clicked()
{
    ui->widgetNavigation->hide();
    ui->stackedWidget->setCurrentWidget(ui->pageRecharge);
}


void MainWindow::on_BtnCancel_pageRecharge_clicked()
{
    ui->widgetNavigation->show();
    ui->stackedWidget->setCurrentWidget(ui->pageMine);
}

