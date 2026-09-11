#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QMessageBox>

// Recharge page: preset amount buttons, confirm/cancel, and logging out
// (leave) back to the login page.

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
    m_balanceBeforeSettlement = -1.0;
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
