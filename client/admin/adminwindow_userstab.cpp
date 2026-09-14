#include "adminwindow.h"
#include "admindesign.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <QStackedWidget>

// Builds Tab 1 (User Management) and adds it to contentStack. Must run
// after buildDashboardShell().
void AdminWindow::buildUsersTab()
{
    // ══════════════════════════════════════════════════════════════════════════
    // TAB 1: USER MANAGEMENT
    // ══════════════════════════════════════════════════════════════════════════
    auto *usersPage = new QWidget;
    usersPage->setStyleSheet(QStringLiteral("background:#f2f8f5;"));
    auto *usersMainLayout = new QVBoxLayout(usersPage);
    usersMainLayout->setContentsMargins(28, 24, 28, 24);
    usersMainLayout->setSpacing(16);

    // Filter bar card
    auto *filterCard = new QFrame;
    filterCard->setObjectName(QStringLiteral("contentCard"));
    auto *filterLayout = new QHBoxLayout(filterCard);
    filterLayout->setContentsMargins(20, 16, 20, 16);
    filterLayout->setSpacing(12);

    userFilterEdit->setPlaceholderText(QStringLiteral("🔍  按手机号搜索用户..."));
    userFilterEdit->setMinimumHeight(40);
    filterLayout->addWidget(userFilterEdit, 1);

    auto *searchUsersBtn = new QPushButton(QStringLiteral("搜索"));
    searchUsersBtn->setObjectName(QStringLiteral("secondaryBtn"));
    searchUsersBtn->setMinimumHeight(40);
    searchUsersBtn->setCursor(Qt::PointingHandCursor);
    filterLayout->addWidget(searchUsersBtn);

    auto *refreshUsersBtn = new QPushButton(QStringLiteral("刷新"));
    refreshUsersBtn->setObjectName(QStringLiteral("secondaryBtn"));
    refreshUsersBtn->setMinimumHeight(40);
    refreshUsersBtn->setCursor(Qt::PointingHandCursor);
    filterLayout->addWidget(refreshUsersBtn);

    auto *toggleUserBtn = new QPushButton(QStringLiteral("冻结/恢复"));
    toggleUserBtn->setObjectName(QStringLiteral("warningBtn"));
    toggleUserBtn->setMinimumHeight(40);
    toggleUserBtn->setCursor(Qt::PointingHandCursor);
    filterLayout->addWidget(toggleUserBtn);

    usersMainLayout->addWidget(filterCard);

    // Table card
    auto *tableCard = new QFrame;
    tableCard->setObjectName(QStringLiteral("contentCard"));
    applyShadow(tableCard, 16, 4, QColor(19, 27, 46, 12));
    auto *tableCardLayout = new QVBoxLayout(tableCard);
    tableCardLayout->setContentsMargins(0, 0, 0, 0);

    usersTable->setColumnCount(6);
    usersTable->setHorizontalHeaderLabels({
        QStringLiteral("ID"), QStringLiteral("手机号"), QStringLiteral("昵称"),
        QStringLiteral("余额"), QStringLiteral("状态"), QStringLiteral("注册时间")
    });
    usersTable->horizontalHeader()->setStretchLastSection(true);
    usersTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    usersTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    usersTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    usersTable->setAlternatingRowColors(true);
    usersTable->setShowGrid(false);
    usersTable->verticalHeader()->setVisible(false);
    usersTable->verticalHeader()->setDefaultSectionSize(48);
    usersTable->setMinimumHeight(220);
    usersTable->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    usersTable->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    tableCardLayout->addWidget(usersTable);
    usersMainLayout->addWidget(tableCard, 1);

    contentStack->addWidget(usersPage);
    connect(searchUsersBtn, &QPushButton::clicked, this, &AdminWindow::refreshUsers);
    connect(refreshUsersBtn, &QPushButton::clicked, this, &AdminWindow::refreshUsers);
    connect(toggleUserBtn, &QPushButton::clicked, this, &AdminWindow::toggleSelectedUser);

}
