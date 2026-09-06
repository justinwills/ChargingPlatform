#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QMessageBox>
#include <QRegularExpression>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

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

