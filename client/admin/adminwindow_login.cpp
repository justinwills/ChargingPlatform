#include "adminwindow.h"
#include <QLineEdit>
#include <QLabel>
#include <QStackedWidget>

// Login flow (connects to the server and sends admin_login) and the
// generic send()/requestInitialData() network helpers used once logged in.

// ─── Login ─────────────────────────────────────────────────────────────────────
void AdminWindow::login()
{
    if (usernameEdit->text().trimmed().isEmpty() || passwordEdit->text().isEmpty()) {
        loginStatus->setText(QStringLiteral("请输入用户名和密码"));
        return;
    }

    pendingAction = QStringLiteral("admin_login");

    const auto sendLogin = [this]() {
        connection->sendRequest(QStringLiteral("admin_login"), {
            {"username", usernameEdit->text().trimmed()},
            {"password", passwordEdit->text()}
        });
    };
    if (connection->isConnected()) {
        sendLogin();
    } else {
        connect(connection, &ClientConnection::connected, this, sendLogin, Qt::SingleShotConnection);
        connection->connectToServer(QStringLiteral("127.0.0.1"), 8888);
    }
}

void AdminWindow::logout()
{
    passwordEdit->clear();
    loginStatus->clear();
    pages->setCurrentIndex(0);
    updateNavButtons(0);
    contentStack->setCurrentIndex(0);
}


// ─── Network ───────────────────────────────────────────────────────────────────
void AdminWindow::send(const QString &action, const QJsonObject &params)
{
    pendingAction = action;
    if (!connection->isConnected()) {
        showError(QStringLiteral("尚未连接服务器"));
        return;
    }
    connection->sendRequest(action, params);
}

void AdminWindow::requestInitialData()
{
    refreshStats();
    refreshUsers();
    refreshStationsAndPiles();
    refreshOrders();
}

