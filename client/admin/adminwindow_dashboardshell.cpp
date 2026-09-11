#include "adminwindow.h"
#include "admindesign.h"

#include <QButtonGroup>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWidget>

// Builds the dashboard shell (pages->widget 1): sidebar nav, header bar,
// and the empty contentStack that the tab builders populate. Must run
// before buildStatsTab/buildUsersTab/buildStationsTab/buildOrdersTab.
void AdminWindow::buildDashboardShell()
{
    dashboardPage = new QWidget;
    auto *dashLayout = new QHBoxLayout(dashboardPage);
    dashLayout->setContentsMargins(0, 0, 0, 0);
    dashLayout->setSpacing(0);

    // ── Sidebar ──────────────────────────────────────────────────────────────
    auto *sidebar = new QFrame;
    sidebar->setObjectName(QStringLiteral("sidebar"));
    sidebar->setFixedWidth(220);
    auto *sideLayout = new QVBoxLayout(sidebar);
    sideLayout->setContentsMargins(16, 20, 16, 20);
    sideLayout->setSpacing(4);

    // Brand area
    auto *brandRow = new QHBoxLayout;
    brandRow->setSpacing(10);
    auto *sBrandIcon = new QLabel(QStringLiteral("⚡"));
    sBrandIcon->setFixedSize(36, 36);
    sBrandIcon->setStyleSheet(QStringLiteral(
        "background:#2563eb; border-radius:10px; color:#ffffff; "
        "font-size:18px; font-weight:700; border:none;"));
    sBrandIcon->setAlignment(Qt::AlignCenter);
    brandRow->addWidget(sBrandIcon);

    auto *sBrandText = new QVBoxLayout;
    sBrandText->setSpacing(0);
    auto *sBrandName = new QLabel(QStringLiteral("东软电动汽车充电平台"));
    sBrandName->setObjectName(QStringLiteral("sidebarBrand"));
    sBrandText->addWidget(sBrandName);
    auto *sBrandSub = new QLabel(QStringLiteral("管理后台"));
    sBrandSub->setObjectName(QStringLiteral("sidebarSub"));
    sBrandText->addWidget(sBrandSub);
    brandRow->addLayout(sBrandText);
    brandRow->addStretch();
    sideLayout->addLayout(brandRow);
    sideLayout->addSpacing(24);

    // Nav section label
    auto *navSectionLabel = new QLabel(QStringLiteral("导航"));
    navSectionLabel->setStyleSheet(QStringLiteral(
        "color:#c3c6d7; font-size:11px; font-weight:700; letter-spacing:0.08em; "
        "text-transform:uppercase; background:transparent; border:none; padding-left:12px;"));
    sideLayout->addWidget(navSectionLabel);
    sideLayout->addSpacing(8);

    // Nav buttons
    struct NavInfo { QString icon; QString label; };
    QVector<NavInfo> navItems = {
        {QStringLiteral("📊"), QStringLiteral("营收统计")},
        {QStringLiteral("👥"), QStringLiteral("用户管理")},
        {QStringLiteral("⚡"), QStringLiteral("站点与电桩")},
        {QStringLiteral("📋"), QStringLiteral("订单报表")},
    };
    auto *navGroup = new QButtonGroup(this);
    navGroup->setExclusive(true);
    for (int i = 0; i < navItems.size(); ++i) {
        auto *btn = new QPushButton(QStringLiteral("%1  %2").arg(navItems[i].icon, navItems[i].label));
        btn->setObjectName(QStringLiteral("navBtn%1").arg(i));
        btn->setCheckable(true);
        btn->setMinimumHeight(44);
        btn->setCursor(Qt::PointingHandCursor);
        navGroup->addButton(btn, i);
        sideLayout->addWidget(btn);
        navButtons.append(btn);
    }
    navButtons[0]->setChecked(true);
    sideLayout->addStretch();

    // Sidebar footer
    sideLayout->addWidget(makeDivider(QStringLiteral("#eaedff")));
    sideLayout->addSpacing(8);
    auto *footerLabel = new QLabel(QStringLiteral("东软电动汽车充电平台\nv1.0.0"));
    footerLabel->setStyleSheet(QStringLiteral(
        "color:#c3c6d7; font-size:11px; background:transparent; border:none;"));
    footerLabel->setAlignment(Qt::AlignCenter);
    sideLayout->addWidget(footerLabel);

    dashLayout->addWidget(sidebar);

    // ── Main content area ────────────────────────────────────────────────────
    auto *mainArea = new QWidget;
    mainArea->setStyleSheet(QStringLiteral("background:#faf8ff;"));
    auto *mainLayout = new QVBoxLayout(mainArea);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Header bar
    auto *headerBar = new QFrame;
    headerBar->setObjectName(QStringLiteral("headerBar"));
    headerBar->setFixedHeight(64);
    auto *headerLayout = new QHBoxLayout(headerBar);
    headerLayout->setContentsMargins(28, 0, 28, 0);
    auto *headerTitle = new QLabel(QStringLiteral("营收统计"));
    headerTitle->setObjectName(QStringLiteral("headerTitle"));
    headerLayout->addWidget(headerTitle);
    headerLayout->addStretch();
    auto *headerSub = new QLabel(QStringLiteral("管理员"));
    headerSub->setObjectName(QStringLiteral("headerSub"));
    headerLayout->addWidget(headerSub);
    headerLayout->addSpacing(12);
    auto *logoutBtn = new QPushButton(QStringLiteral("退出登录"));
    logoutBtn->setObjectName(QStringLiteral("logoutBtn"));
    logoutBtn->setCursor(Qt::PointingHandCursor);
    logoutBtn->setFixedHeight(30);
    headerLayout->addWidget(logoutBtn);
    mainLayout->addWidget(headerBar);

    // Content stack
    contentStack = new QStackedWidget;
    contentStack->setStyleSheet(QStringLiteral("background:#faf8ff;"));
    mainLayout->addWidget(contentStack, 1);

    dashLayout->addWidget(mainArea, 1);
    pages->addWidget(dashboardPage);
    connect(logoutBtn, &QPushButton::clicked, this, &AdminWindow::logout);

    // Connect nav
    connect(navGroup, QOverload<int>::of(&QButtonGroup::idClicked),
            this, &AdminWindow::switchTab);

    connect(navGroup, QOverload<int>::of(&QButtonGroup::idClicked), this,
            [headerTitle](int idx) {
        const QStringList titles = {
            QStringLiteral("营收统计"),
            QStringLiteral("用户管理"),
            QStringLiteral("站点与电桩"),
            QStringLiteral("订单报表"),
        };
        if (idx >= 0 && idx < titles.size())
            headerTitle->setText(titles[idx]);
    });

}
