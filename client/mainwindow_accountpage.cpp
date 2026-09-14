#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "admin/adminwindow.h"

#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>

// Mine tab entry, opening the admin window, and the login flow.

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

QPixmap MainWindow::circularPixmap(const QPixmap &source, int size){
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

