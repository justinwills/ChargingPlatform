#include "adminwindow.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QJsonArray>
#include <QJsonObject>

AdminWindow::AdminWindow(QWidget *parent)
    : QMainWindow(parent), connection(new ClientConnection(this)),
      pages(new QStackedWidget(this)), usernameEdit(new QLineEdit(this)),
      passwordEdit(new QLineEdit(this)), loginStatus(new QLabel(this)),
      userFilterEdit(new QLineEdit(this)), usersTable(new QTableWidget(this)),
      stationsTable(new QTableWidget(this)), pilesTable(new QTableWidget(this)),
      statsLabel(new QLabel(this))
{
    setWindowTitle(QStringLiteral("充电平台管理后台"));
    resize(900, 600);
    setCentralWidget(pages);

    auto *loginPage = new QWidget(this);
    auto *loginLayout = new QVBoxLayout(loginPage);
    auto *loginBox = new QGroupBox(QStringLiteral("管理员登录"), loginPage);
    auto *form = new QFormLayout(loginBox);
    usernameEdit->setText(QStringLiteral("admin"));
    passwordEdit->setEchoMode(QLineEdit::Password);
    passwordEdit->setText(QStringLiteral("123456"));
    form->addRow(QStringLiteral("用户名"), usernameEdit);
    form->addRow(QStringLiteral("密码"), passwordEdit);
    auto *loginButton = new QPushButton(QStringLiteral("登录"), loginBox);
    form->addRow(loginButton);
    loginStatus->setStyleSheet(QStringLiteral("color: #b3261e;"));
    loginLayout->addStretch();
    loginLayout->addWidget(loginBox);
    loginLayout->addWidget(loginStatus);
    loginLayout->addStretch();
    pages->addWidget(loginPage);
    connect(loginButton, &QPushButton::clicked, this, &AdminWindow::login);

    auto *dashboard = new QWidget(this);
    auto *dashboardLayout = new QVBoxLayout(dashboard);
    auto *tabs = new QTabWidget(dashboard);
    dashboardLayout->addWidget(tabs);
    pages->addWidget(dashboard);

    auto *usersPage = new QWidget(tabs);
    auto *usersLayout = new QVBoxLayout(usersPage);
    auto *usersControls = new QHBoxLayout;
    userFilterEdit->setPlaceholderText(QStringLiteral("按手机号搜索"));
    auto *usersRefresh = new QPushButton(QStringLiteral("刷新用户"), usersPage);
    auto *userStatus = new QPushButton(QStringLiteral("冻结/恢复选中用户"), usersPage);
    usersControls->addWidget(userFilterEdit);
    usersControls->addWidget(usersRefresh);
    usersControls->addWidget(userStatus);
    usersLayout->addLayout(usersControls);
    usersTable->setColumnCount(6);
    usersTable->setHorizontalHeaderLabels({QStringLiteral("ID"), QStringLiteral("手机号"), QStringLiteral("昵称"), QStringLiteral("余额"), QStringLiteral("状态"), QStringLiteral("注册时间")});
    usersTable->horizontalHeader()->setStretchLastSection(true);
    usersTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    usersTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    usersLayout->addWidget(usersTable);
    tabs->addTab(usersPage, QStringLiteral("用户管理"));
    connect(usersRefresh, &QPushButton::clicked, this, &AdminWindow::refreshUsers);
    connect(userStatus, &QPushButton::clicked, this, &AdminWindow::toggleSelectedUser);

    auto *stationsPage = new QWidget(tabs);
    auto *stationsLayout = new QVBoxLayout(stationsPage);
    auto *stationRefresh = new QPushButton(QStringLiteral("刷新站点和电桩"), stationsPage);
    stationsLayout->addWidget(stationRefresh);
    stationsTable->setColumnCount(6);
    stationsTable->setHorizontalHeaderLabels({QStringLiteral("ID"), QStringLiteral("名称"), QStringLiteral("地址"), QStringLiteral("价格"), QStringLiteral("空闲/总数"), QStringLiteral("在线率")});
    stationsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    stationsLayout->addWidget(stationsTable);
    pilesTable->setColumnCount(7);
    pilesTable->setHorizontalHeaderLabels({QStringLiteral("ID"), QStringLiteral("站点"), QStringLiteral("编号"), QStringLiteral("类型"), QStringLiteral("功率"), QStringLiteral("状态"), QStringLiteral("累计次数")});
    pilesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    stationsLayout->addWidget(pilesTable);
    auto *pileControls = new QHBoxLayout;
    auto *pileId = new QSpinBox(stationsPage);
    pileId->setMinimum(1);
    auto *restart = new QPushButton(QStringLiteral("重启电桩"), stationsPage);
    pileControls->addWidget(new QLabel(QStringLiteral("电桩ID"), stationsPage));
    pileControls->addWidget(pileId);
    pileControls->addWidget(restart);
    pileControls->addStretch();
    stationsLayout->addLayout(pileControls);
    tabs->addTab(stationsPage, QStringLiteral("站点与电桩"));
    connect(stationRefresh, &QPushButton::clicked, this, &AdminWindow::refreshStationsAndPiles);
    connect(restart, &QPushButton::clicked, this, &AdminWindow::restartSelectedPile);

    auto *statsPage = new QWidget(tabs);
    auto *statsLayout = new QVBoxLayout(statsPage);
    auto *statsRefresh = new QPushButton(QStringLiteral("刷新统计"), statsPage);
    statsLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    statsLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    statsLayout->addWidget(statsRefresh);
    statsLayout->addWidget(statsLabel);
    statsLayout->addStretch();
    tabs->addTab(statsPage, QStringLiteral("营收与状态统计"));
    connect(statsRefresh, &QPushButton::clicked, this, &AdminWindow::refreshStats);

    connect(connection, &ClientConnection::responseReceived, this, &AdminWindow::handleResponse);
    connect(connection, &ClientConnection::connectionError, this, &AdminWindow::handleError);
}

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
    refreshUsers();
    refreshStationsAndPiles();
    refreshStats();
}

void AdminWindow::refreshUsers()
{
    send(QStringLiteral("query_users"), {{"phoneKeyword", userFilterEdit->text().trimmed()}});
}

void AdminWindow::toggleSelectedUser()
{
    const int row = usersTable->currentRow();
    if (row < 0) {
        showError(QStringLiteral("请先选择用户"));
        return;
    }
    const int userId = usersTable->item(row, 0)->text().toInt();
    const QString currentStatus = usersTable->item(row, 4)->text();
    send(QStringLiteral("set_user_status"), {
        {"userId", userId},
        {"status", currentStatus == QStringLiteral("冻结") ? QStringLiteral("正常") : QStringLiteral("冻结")}
    });
}

void AdminWindow::refreshStationsAndPiles()
{
    send(QStringLiteral("admin_query_stations"));
}

void AdminWindow::restartSelectedPile()
{
    auto *spinBox = findChild<QSpinBox *>();
    send(QStringLiteral("admin_restart_pile"), {{"pileId", spinBox ? spinBox->value() : 1}});
}

void AdminWindow::refreshStats()
{
    send(QStringLiteral("admin_stats"));
}

void AdminWindow::handleResponse(const QJsonObject &response)
{
    const int code = response.value("code").toInt(-1);
    if (code != 0) {
        showError(response.value("msg").toString());
        return;
    }

    const QJsonObject data = response.value("data").toObject();
    if (pendingAction == QStringLiteral("admin_login")) {
        pages->setCurrentIndex(1);
        requestInitialData();
    } else if (data.contains("users")) {
        const QJsonArray users = data.value("users").toArray();
        usersTable->setRowCount(users.size());
        for (int row = 0; row < users.size(); ++row) {
            const QJsonObject user = users.at(row).toObject();
            usersTable->setItem(row, 0, new QTableWidgetItem(QString::number(user.value("userId").toInt())));
            usersTable->setItem(row, 1, new QTableWidgetItem(user.value("phone").toString()));
            usersTable->setItem(row, 2, new QTableWidgetItem(user.value("nickname").toString()));
            usersTable->setItem(row, 3, new QTableWidgetItem(QString::number(user.value("balance").toDouble(), 'f', 2)));
            usersTable->setItem(row, 4, new QTableWidgetItem(user.value("status").toString()));
            usersTable->setItem(row, 5, new QTableWidgetItem(user.value("createdAt").toString()));
        }
    } else if (data.contains("stations")) {
        const QJsonArray stations = data.value("stations").toArray();
        stationsTable->setRowCount(stations.size());
        for (int row = 0; row < stations.size(); ++row) {
            const QJsonObject station = stations.at(row).toObject();
            stationsTable->setItem(row, 0, new QTableWidgetItem(QString::number(station.value("stationId").toInt())));
            stationsTable->setItem(row, 1, new QTableWidgetItem(station.value("name").toString()));
            stationsTable->setItem(row, 2, new QTableWidgetItem(station.value("address").toString()));
            stationsTable->setItem(row, 3, new QTableWidgetItem(QString::number(station.value("price").toDouble(), 'f', 2)));
            stationsTable->setItem(row, 4, new QTableWidgetItem(QStringLiteral("%1/%2").arg(station.value("freePileCount").toInt()).arg(station.value("pileCount").toInt())));
            stationsTable->setItem(row, 5, new QTableWidgetItem(QStringLiteral("%1%").arg(station.value("onlineRate").toDouble(), 0, 'f', 1)));
        }
        send(QStringLiteral("admin_query_piles"));
    } else if (data.contains("piles")) {
        const QJsonArray piles = data.value("piles").toArray();
        pilesTable->setRowCount(piles.size());
        for (int row = 0; row < piles.size(); ++row) {
            const QJsonObject pile = piles.at(row).toObject();
            pilesTable->setItem(row, 0, new QTableWidgetItem(QString::number(pile.value("pileId").toInt())));
            pilesTable->setItem(row, 1, new QTableWidgetItem(pile.value("stationName").toString()));
            pilesTable->setItem(row, 2, new QTableWidgetItem(pile.value("code").toString()));
            pilesTable->setItem(row, 3, new QTableWidgetItem(pile.value("type").toString()));
            pilesTable->setItem(row, 4, new QTableWidgetItem(QString::number(pile.value("power").toDouble(), 'f', 1)));
            pilesTable->setItem(row, 5, new QTableWidgetItem(pile.value("status").toString()));
            pilesTable->setItem(row, 6, new QTableWidgetItem(QString::number(pile.value("totalSessions").toInt())));
        }
    } else if (data.contains("revenueToday") && data.contains("pileStatus")) {
        const QJsonObject pileStatus = data.value("pileStatus").toObject();
        statsLabel->setText(QStringLiteral("今日营收：%1 元\n本月营收：%2 元\n累计营收：%3 元\n\n电桩状态：闲置 %4，在用 %5，故障 %6")
            .arg(data.value("revenueToday").toDouble(), 0, 'f', 2)
            .arg(data.value("revenueThisMonth").toDouble(), 0, 'f', 2)
            .arg(data.value("revenueTotal").toDouble(), 0, 'f', 2)
            .arg(pileStatus.value("闲置").toInt())
            .arg(pileStatus.value("在用").toInt())
            .arg(pileStatus.value("故障").toInt()));
    } else if (pendingAction == QStringLiteral("set_user_status")) {
        refreshUsers();
    } else if (pendingAction == QStringLiteral("admin_restart_pile")) {
        refreshStationsAndPiles();
    }
}

void AdminWindow::handleError(const QString &message)
{
    showError(message);
}

void AdminWindow::showError(const QString &message)
{
    if (pages->currentIndex() == 0) {
        loginStatus->setText(message);
    } else {
        QMessageBox::warning(this, QStringLiteral("管理员操作失败"), message);
    }
}
