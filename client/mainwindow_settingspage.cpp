#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QPixmap>
#include <QPainter>
#include <QPainterPath>

// Profile editing: entering the edit page, choosing an avatar photo,
// and confirming/cancelling the edit.

void MainWindow::on_BtnSetting_clicked()
{
    ui->lineEditNickname->setText(m_currentUser.value("nickname").toString());

    m_selectedAvatarPath = m_currentUser.value("avatarPath").toString();
    ui->labelPhotoEdit->setPixmap(ui->labelPhoto->pixmap());

    ui->widgetNavigation->hide();
    ui->stackedWidget->setCurrentWidget(ui->pageEditMine);

}

static QPixmap circularPixmap(const QPixmap &pixmap, int size)
{
    if (pixmap.isNull()) {
        return QPixmap();
    }

    QPixmap scaled = pixmap.scaled(
        size,
        size,
        Qt::KeepAspectRatioByExpanding,
        Qt::SmoothTransformation
        );

    QPixmap result(size, size);
    result.fill(Qt::transparent);

    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    QPainterPath path;
    path.addEllipse(0, 0, size, size);
    painter.setClipPath(path);

    painter.drawPixmap(
        (size - scaled.width()) / 2,
        (size - scaled.height()) / 2,
        scaled
        );

    return result;
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

