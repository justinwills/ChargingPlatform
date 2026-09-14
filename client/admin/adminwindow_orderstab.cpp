#include "adminwindow.h"
#include "admindesign.h"

#include <QComboBox>
#include <QDateEdit>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <QStackedWidget>

// Builds Tab 3 (Order Reports) and adds it to contentStack. Also wires
// the revenuePeriodCombo (Tab 0 widget) change signal, matching the
// order of the original single-constructor implementation. Must run
// after buildDashboardShell() and buildStatsTab().
void AdminWindow::buildOrdersTab()
{
    // ══════════════════════════════════════════════════════════════════════════
    // TAB 3: ORDER REPORTS
    // ══════════════════════════════════════════════════════════════════════════
    auto *ordersPage = new QWidget;
    ordersPage->setStyleSheet(QStringLiteral("background:#f6f3ff;"));
    auto *ordersMainLayout = new QVBoxLayout(ordersPage);
    ordersMainLayout->setContentsMargins(28, 24, 28, 24);
    ordersMainLayout->setSpacing(16);

    // Filter bar card
    auto *orderFilterCard = new QFrame;
    orderFilterCard->setObjectName(QStringLiteral("contentCard"));
    auto *orderFilterLayout = new QVBoxLayout(orderFilterCard);
    orderFilterLayout->setContentsMargins(20, 16, 20, 16);
    orderFilterLayout->setSpacing(12);

    auto *orderFilterRow1 = new QHBoxLayout;
    orderFilterRow1->setSpacing(12);
    orderPhoneFilter->setPlaceholderText(QStringLiteral("🔍  手机号"));
    orderPhoneFilter->setMinimumHeight(40);
    orderFilterRow1->addWidget(orderPhoneFilter, 1);

    auto *orderPhoneSearchBtn = new QPushButton(QStringLiteral("搜索"));
    orderPhoneSearchBtn->setObjectName(QStringLiteral("secondaryBtn"));
    orderPhoneSearchBtn->setMinimumHeight(40);
    orderPhoneSearchBtn->setCursor(Qt::PointingHandCursor);
    orderFilterRow1->addWidget(orderPhoneSearchBtn);

    orderStationFilter->addItem(QStringLiteral("全部站点"), -1);
    orderStationFilter->setMinimumHeight(40);
    orderFilterRow1->addWidget(orderStationFilter);

    orderStatusFilter->addItem(QStringLiteral("全部状态"), QString());
    orderStatusFilter->addItem(QStringLiteral("充电中"), QStringLiteral("充电中"));
    orderStatusFilter->addItem(QStringLiteral("待结算"), QStringLiteral("待结算"));
    orderStatusFilter->addItem(QStringLiteral("已结算"), QStringLiteral("已结算"));
    orderStatusFilter->setMinimumHeight(40);
    orderFilterRow1->addWidget(orderStatusFilter);
    orderFilterLayout->addLayout(orderFilterRow1);

    auto *orderFilterRow2 = new QHBoxLayout;
    orderFilterRow2->setSpacing(12);
    orderFromDate->setCalendarPopup(true);
    orderFromDate->setDate(QDate::currentDate().addDays(-6));
    orderFromDate->setMinimumHeight(40);
    orderFilterRow2->addWidget(orderFromDate);

    auto *dateSep = new QLabel(QStringLiteral("至"));
    dateSep->setStyleSheet(QStringLiteral("color:#737686; font-size:13px; background:transparent; border:none;"));
    orderFilterRow2->addWidget(dateSep);

    orderToDate->setCalendarPopup(true);
    orderToDate->setDate(QDate::currentDate());
    orderToDate->setMinimumHeight(40);
    orderFilterRow2->addWidget(orderToDate);

    orderFilterRow2->addStretch();

    auto *ordersRefreshBtn = new QPushButton(QStringLiteral("刷新订单"));
    ordersRefreshBtn->setObjectName(QStringLiteral("secondaryBtn"));
    ordersRefreshBtn->setMinimumHeight(40);
    ordersRefreshBtn->setCursor(Qt::PointingHandCursor);
    orderFilterRow2->addWidget(ordersRefreshBtn);
    orderFilterLayout->addLayout(orderFilterRow2);

    ordersMainLayout->addWidget(orderFilterCard);

    // Table card
    auto *orderTableCard = new QFrame;
    orderTableCard->setObjectName(QStringLiteral("contentCard"));
    applyShadow(orderTableCard, 16, 4, QColor(19, 27, 46, 12));
    auto *otcLayout = new QVBoxLayout(orderTableCard);
    otcLayout->setContentsMargins(0, 0, 0, 0);

    ordersTable->setColumnCount(10);
    ordersTable->setHorizontalHeaderLabels({
        QStringLiteral("订单ID"), QStringLiteral("用户ID"), QStringLiteral("手机号"),
        QStringLiteral("站点"), QStringLiteral("电桩ID"), QStringLiteral("开始时间"),
        QStringLiteral("结束时间"), QStringLiteral("电量"), QStringLiteral("费用"),
        QStringLiteral("状态")
    });
    ordersTable->horizontalHeader()->setStretchLastSection(true);
    ordersTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ordersTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ordersTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ordersTable->setAlternatingRowColors(true);
    ordersTable->setShowGrid(false);
    ordersTable->verticalHeader()->setVisible(false);
    ordersTable->verticalHeader()->setDefaultSectionSize(48);
    otcLayout->addWidget(ordersTable);
    ordersMainLayout->addWidget(orderTableCard, 1);

    contentStack->addWidget(ordersPage);
    connect(orderPhoneSearchBtn, &QPushButton::clicked, this, &AdminWindow::refreshOrders);
    connect(orderPhoneFilter, &QLineEdit::returnPressed, this, &AdminWindow::refreshOrders);
    connect(orderStationFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AdminWindow::refreshOrders);
    connect(orderStatusFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AdminWindow::refreshOrders);
    connect(orderFromDate, &QDateEdit::dateChanged, this, &AdminWindow::refreshOrders);
    connect(orderToDate, &QDateEdit::dateChanged, this, &AdminWindow::refreshOrders);
    connect(ordersRefreshBtn, &QPushButton::clicked, this, &AdminWindow::refreshOrders);
    connect(revenuePeriodCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { refreshStats(); });

}
