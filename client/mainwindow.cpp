#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QMessageBox>
#include <QRegularExpression>
#include <QFileDialog>
#include <QPixmap>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // 设置默认头像 （TODO：后面要从数据库那照片）
    QPixmap avatar(":/images/default_avatar.jpeg");

    ui->labelPhoto->setPixmap(
        avatar.scaled(
            ui->labelPhoto->size(),
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation
            )
        );
    ui->labelPhoto->setAlignment(Qt::AlignCenter);

    ui->labelPhotoEdit->setPixmap(
        avatar.scaled(
            ui->labelPhotoEdit->size(),
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation
            )
        );
    ui->labelPhotoEdit->setAlignment(Qt::AlignCenter);


    ui->stackedWidget->setCurrentWidget(ui->pageLogin);
    ui->widgetNavigation->hide();

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
    // TODO: 验证用户
    ui->stackedWidget->setCurrentWidget(ui->pageHome);
    ui->widgetNavigation->show();
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

