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
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QDateEdit>
#include <algorithm>
#include <functional>
#include <QDate>
#include <QHBoxLayout>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpacerItem>
#include <QGridLayout>
#include <QButtonGroup>
#include <QVector>

// ─── Design System Constants ───────────────────────────────────────────────────
// Colors
static const QString cPrimary       = QStringLiteral("#2563eb");
static const QString cPrimaryDark   = QStringLiteral("#1d4ed8");
static const QString cPrimaryLight  = QStringLiteral("#dbe1ff");
static const QString cPrimaryFaint  = QStringLiteral("#e2e7ff");
static const QString cOnPrimary     = QStringLiteral("#ffffff");
static const QString cSurface       = QStringLiteral("#faf8ff");
static const QString cSurfaceCard   = QStringLiteral("#ffffff");
static const QString cOnSurface     = QStringLiteral("#131b2e");
static const QString cOnSurfaceVar  = QStringLiteral("#434655");
static const QString cOutline       = QStringLiteral("#737686");
static const QString cOutlineVar    = QStringLiteral("#c3c6d7");
static const QString cBorder        = QStringLiteral("#eaedff");
static const QString cInputBg       = QStringLiteral("#f2f3ff");
static const QString cSuccess       = QStringLiteral("#006c49");
static const QString cSuccessBg     = QStringLiteral("#e2f6ec");
static const QString cSuccessBorder = QStringLiteral("#a7f3d0");
static const QString cWarning       = QStringLiteral("#b45309");
static const QString cWarningBg     = QStringLiteral("#fff1e6");
static const QString cWarningBorder = QStringLiteral("#fde68a");
static const QString cDanger        = QStringLiteral("#ba1a1a");
static const QString cDangerBg      = QStringLiteral("#ffdad6");
static const QString cDangerBorder  = QStringLiteral("#fecaca");
static const QString cDangerText    = QStringLiteral("#93000a");

// ─── Helpers ───────────────────────────────────────────────────────────────────
static void applyShadow(QWidget *w, int blur = 20, int yOff = 6,
                        const QColor &color = QColor(19, 27, 46, 22))
{
    auto *shadow = new QGraphicsDropShadowEffect(w);
    shadow->setBlurRadius(blur);
    shadow->setOffset(0, yOff);
    shadow->setColor(color);
    w->setGraphicsEffect(shadow);
}

static QFrame *makeDivider(const QString &bg = QStringLiteral("#f2f3ff"))
{
    auto *line = new QFrame;
    line->setFixedHeight(1);
    line->setStyleSheet(QStringLiteral("background:%1; border:none;").arg(bg));
    return line;
}

// ─── Stylesheet ────────────────────────────────────────────────────────────────
QString AdminWindow::stylesheet()
{
    return QStringLiteral(R"(
        /* ── Base ─────────────────────────────────────────────────── */
        QWidget {
            font-family: "Inter", "Microsoft YaHei";
            color: #434655;
            font-size: 13px;
        }
        QMainWindow, QWidget#centralwidget {
            background: #faf8ff;
        }
        QScrollBar:vertical {
            background: transparent; width: 6px; margin: 0;
        }
        QScrollBar::handle:vertical {
            background: #c3c6d7; border-radius: 3px; min-height: 24px;
        }
        QScrollBar::handle:vertical:hover { background: #a3a6b7; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }

        /* ── Sidebar ──────────────────────────────────────────────── */
        QFrame#sidebar {
            background: #ffffff;
            border-right: 1px solid #eaedff;
        }
        QLabel#sidebarBrand {
            color: #2563eb;
            font-size: 15px;
            font-weight: 700;
            background: transparent;
            border: none;
        }
        QLabel#sidebarSub {
            color: #737686;
            font-size: 11px;
            background: transparent;
            border: none;
        }
        QPushButton#navBtn {
            background: transparent;
            color: #434655;
            border: none;
            border-radius: 12px;
            font-size: 14px;
            font-weight: 500;
            text-align: left;
            padding: 12px 20px;
        }
        QPushButton#navBtn:hover {
            background: #f2f3ff;
            color: #131b2e;
        }
        QPushButton#navBtn0:checked {
            background: #e2e7ff;
            color: #2563eb;
            font-weight: 700;
        }
        QPushButton#navBtn1:checked {
            background: #dff2e8;
            color: #006c49;
            font-weight: 700;
        }
        QPushButton#navBtn2:checked {
            background: #ffe9cc;
            color: #b45309;
            font-weight: 700;
        }
        QPushButton#navBtn3:checked {
            background: #ece4ff;
            color: #6d28d9;
            font-weight: 700;
        }

        /* ── Header bar ───────────────────────────────────────────── */
        QFrame#headerBar {
            background: #ffffff;
            border-bottom: 1px solid #eaedff;
        }
        QLabel#headerTitle {
            color: #131b2e;
            font-size: 20px;
            font-weight: 700;
            background: transparent;
            border: none;
        }
        QLabel#headerSub {
            color: #737686;
            font-size: 12px;
            background: transparent;
            border: none;
        }
        QPushButton#logoutBtn {
            background: #ffdad6;
            color: #93000a;
            border: none;
            border-radius: 10px;
            font-size: 13px;
            font-weight: 600;
            padding: 6px 16px;
        }
        QPushButton#logoutBtn:hover { background: #ffc9c2; }
        QPushButton#logoutBtn:pressed { background: #ffb3ab; }

        /* ── Stat cards ───────────────────────────────────────────── */
        QFrame#statCard {
            background: #ffffff;
            border: 1px solid #eaedff;
            border-radius: 16px;
        }
        QFrame#statCardAccent {
            background: #ffffff;
            border: 1px solid #eaedff;
            border-left: 3px solid #2563eb;
            border-radius: 16px;
        }
        QLabel#statLabel {
            color: #737686;
            font-size: 12px;
            font-weight: 500;
            background: transparent;
            border: none;
        }
        QLabel#statValue {
            color: #131b2e;
            font-family: "Inter", "Consolas", "Microsoft YaHei";
            font-size: 24px;
            font-weight: 700;
            background: transparent;
            border: none;
        }
        QLabel#statValueSm {
            color: #131b2e;
            font-family: "Inter", "Consolas", "Microsoft YaHei";
            font-size: 18px;
            font-weight: 700;
            background: transparent;
            border: none;
        }
        QLabel#statSub {
            color: #737686;
            font-size: 11px;
            background: transparent;
            border: none;
        }

        /* ── Status dot indicators ────────────────────────────────── */
        QLabel#dotIdle {
            background: #006c49;
            border-radius: 5px;
            border: none;
        }
        QLabel#dotBusy {
            background: #d97706;
            border-radius: 5px;
            border: none;
        }
        QLabel#dotFault {
            background: #ba1a1a;
            border-radius: 5px;
            border: none;
        }

        /* ── Content cards ────────────────────────────────────────── */
        QFrame#contentCard {
            background: #ffffff;
            border: 1px solid #eaedff;
            border-radius: 16px;
        }

        /* ── Input fields ─────────────────────────────────────────── */
        QLineEdit {
            background: #f2f3ff;
            border: 1px solid #eaedff;
            border-radius: 12px;
            padding: 10px 14px;
            color: #131b2e;
            font-size: 14px;
            selection-background-color: #dbe1ff;
            selection-color: #003ea8;
        }
        QLineEdit:focus {
            border: 1px solid #2563eb;
            background: #ffffff;
        }
        QLineEdit[placeholderText] {
            color: #9aa0b5;
        }

        QSpinBox, QDoubleSpinBox {
            background: #f2f3ff;
            border: 1px solid #eaedff;
            border-radius: 12px;
            padding: 8px 12px;
            color: #131b2e;
            font-size: 14px;
        }
        QSpinBox:focus, QDoubleSpinBox:focus {
            border: 1px solid #2563eb;
        }
        QComboBox {
            background: #f2f3ff;
            border: 1px solid #eaedff;
            border-radius: 12px;
            padding: 8px 12px;
            color: #131b2e;
            font-size: 14px;
        }
        QComboBox:focus {
            border: 1px solid #2563eb;
        }
        QComboBox::drop-down {
            border: none;
            width: 28px;
        }
        QComboBox QAbstractItemView {
            background: #ffffff;
            border: 1px solid #eaedff;
            border-radius: 12px;
            color: #131b2e;
            selection-background-color: #dbe1ff;
            selection-color: #003ea8;
            padding: 4px;
        }
        QDateEdit {
            background: #f2f3ff;
            border: 1px solid #eaedff;
            border-radius: 12px;
            padding: 8px 12px;
            color: #131b2e;
            font-size: 14px;
        }
        QDateEdit:focus {
            border: 1px solid #2563eb;
        }

        /* ── Primary buttons ──────────────────────────────────────── */
        QPushButton#primaryBtn {
            background: #2563eb;
            color: #ffffff;
            border: none;
            border-radius: 12px;
            font-size: 14px;
            font-weight: 600;
            padding: 10px 20px;
            min-height: 20px;
        }
        QPushButton#primaryBtn:hover { background: #1d4ed8; }
        QPushButton#primaryBtn:pressed { background: #003ea8; }
        QPushButton#primaryBtn:disabled { background: #a5b7e8; color: #e8ecf7; }

        /* ── Secondary buttons ────────────────────────────────────── */
        QPushButton#secondaryBtn {
            background: #ffffff;
            color: #131b2e;
            border: 1px solid #eaedff;
            border-radius: 12px;
            font-size: 14px;
            font-weight: 500;
            padding: 10px 20px;
            min-height: 20px;
        }
        QPushButton#secondaryBtn:hover {
            background: #f2f3ff;
            border: 1px solid #2563eb;
        }
        QPushButton#secondaryBtn:pressed { background: #e2e7ff; }

        /* ── Danger buttons ───────────────────────────────────────── */
        QPushButton#dangerBtn {
            background: #ffdad6;
            color: #93000a;
            border: none;
            border-radius: 12px;
            font-size: 14px;
            font-weight: 600;
            padding: 10px 20px;
            min-height: 20px;
        }
        QPushButton#dangerBtn:hover { background: #ffc9c2; }

        /* ── Warning buttons ──────────────────────────────────────── */
        QPushButton#warningBtn {
            background: #fff1e6;
            color: #b45309;
            border: none;
            border-radius: 12px;
            font-size: 14px;
            font-weight: 600;
            padding: 10px 20px;
            min-height: 20px;
        }
        QPushButton#warningBtn:hover { background: #ffddb8; }

        /* ── Success buttons ──────────────────────────────────────── */
        QPushButton#successBtn {
            background: #e2f6ec;
            color: #006c49;
            border: none;
            border-radius: 12px;
            font-size: 14px;
            font-weight: 600;
            padding: 10px 20px;
            min-height: 20px;
        }
        QPushButton#successBtn:hover { background: #c6eed8; }

        /* ── Tables ───────────────────────────────────────────────── */
        QTableWidget {
            background: #ffffff;
            alternate-background-color: #f4f6ff;
            border: 1px solid #eaedff;
            border-radius: 12px;
            gridline-color: #f2f3ff;
            color: #434655;
            font-size: 13px;
            selection-background-color: #dbe1ff;
            selection-color: #131b2e;
            outline: none;
        }
        QTableWidget::item {
            padding: 8px 10px;
            border-bottom: 1px solid #f2f3ff;
            color: #434655;
        }
        QTableWidget::item:selected {
            background: #e2e7ff;
            color: #131b2e;
        }
        QTableWidget::item:hover:!selected {
            background: #f8f9ff;
        }
        QHeaderView::section {
            background: #f8f9ff;
            color: #737686;
            font-size: 12px;
            font-weight: 600;
            border: none;
            border-bottom: 1px solid #eaedff;
            padding: 10px 12px;
            text-align: left;
        }
        QHeaderView::section:hover {
            background: #f2f3ff;
        }
        QTableCornerButton::section {
            background: #f8f9ff;
            border: none;
            border-bottom: 1px solid #eaedff;
            border-right: 1px solid #eaedff;
        }

        /* ── Scroll areas ─────────────────────────────────────────── */
        QScrollArea {
            border: none;
            background: transparent;
        }
        QScrollArea > QWidget > QWidget {
            background: transparent;
        }

        /* ── Login page ───────────────────────────────────────────── */
        QFrame#loginCard {
            background: #ffffff;
            border: 1px solid #eaedff;
            border-radius: 20px;
        }
        QLabel#loginLogo {
            background: #ffffff;
            border: 1px solid #eaedff;
            border-radius: 36px;
            color: #2563eb;
            font-size: 34px;
            font-weight: 700;
        }
        QLabel#loginTitle {
            color: #131b2e;
            font-size: 24px;
            font-weight: 700;
            background: transparent;
            border: none;
        }
        QLabel#loginTagline {
            color: #737686;
            font-size: 13px;
            background: transparent;
            border: none;
        }
        QLabel#errorLabel {
            color: #ba1a1a;
            font-size: 13px;
            background: transparent;
            border: none;
        }

        /* ── Trend label ──────────────────────────────────────────── */
        QLabel#trendLabel {
            color: #434655;
            font-size: 13px;
            background: transparent;
            border: none;
            line-height: 1.6;
        }
        QLabel#trendDate {
            color: #737686;
            font-size: 12px;
            background: transparent;
            border: none;
        }
        QLabel#trendRevenue {
            color: #131b2e;
            font-family: "Inter", "Consolas", "Microsoft YaHei";
            font-size: 14px;
            font-weight: 600;
            background: transparent;
            border: none;
        }

        /* ── Badge pills ──────────────────────────────────────────── */
        QLabel#badgeActive {
            background: #e2f6ec;
            color: #006c49;
            border-radius: 10px;
            font-size: 11px;
            font-weight: 700;
            padding: 3px 10px;
        }
        QLabel#badgeFrozen {
            background: #ffdad6;
            color: #93000a;
            border-radius: 10px;
            font-size: 11px;
            font-weight: 700;
            padding: 3px 10px;
        }
        QLabel#badgePrimary {
            background: #dbe1ff;
            color: #003ea8;
            border-radius: 10px;
            font-size: 11px;
            font-weight: 700;
            padding: 3px 10px;
        }
        QLabel#badgeIdle {
            background: #e2f6ec;
            color: #006c49;
            border-radius: 10px;
            font-size: 11px;
            font-weight: 700;
            padding: 3px 10px;
        }
        QLabel#badgeBusy {
            background: #fff1e6;
            color: #b45309;
            border-radius: 10px;
            font-size: 11px;
            font-weight: 700;
            padding: 3px 10px;
        }
        QLabel#badgeFault {
            background: #ffdad6;
            color: #93000a;
            border-radius: 10px;
            font-size: 11px;
            font-weight: 700;
            padding: 3px 10px;
        }

        /* ── Pile status card ─────────────────────────────────────── */
        QFrame#pileStatusCard {
            background: #ffffff;
            border: 1px solid #eaedff;
            border-radius: 16px;
        }
        QFrame#pileCardIdle {
            background: #eef9f2;
            border: 1px solid #a7f3d0;
            border-left: 4px solid #006c49;
            border-radius: 16px;
        }
        QFrame#pileCardBusy {
            background: #fff8ef;
            border: 1px solid #fde68a;
            border-left: 4px solid #d97706;
            border-radius: 16px;
        }
        QFrame#pileCardFault {
            background: #fff1f0;
            border: 1px solid #fecaca;
            border-left: 4px solid #ba1a1a;
            border-radius: 16px;
        }
        QLabel#countIdle {
            color: #006c49;
            font-family: "Inter", "Consolas", "Microsoft YaHei";
            font-size: 18px;
            font-weight: 700;
            background: transparent;
            border: none;
        }
        QLabel#countBusy {
            color: #d97706;
            font-family: "Inter", "Consolas", "Microsoft YaHei";
            font-size: 18px;
            font-weight: 700;
            background: transparent;
            border: none;
        }
        QLabel#countFault {
            color: #ba1a1a;
            font-family: "Inter", "Consolas", "Microsoft YaHei";
            font-size: 18px;
            font-weight: 700;
            background: transparent;
            border: none;
        }
    )");
}

// ─── Constructor ───────────────────────────────────────────────────────────────
AdminWindow::AdminWindow(QWidget *parent)
    : QMainWindow(parent), connection(new ClientConnection(this)),
      pages(new QStackedWidget(this)),
      usernameEdit(new QLineEdit), passwordEdit(new QLineEdit),
      loginStatus(new QLabel),
      userFilterEdit(new QLineEdit), usersTable(new QTableWidget),
      stationNameEdit(new QLineEdit), stationAddressEdit(new QLineEdit),
      stationLongitudeEdit(new QDoubleSpinBox), stationLatitudeEdit(new QDoubleSpinBox),
      stationPriceEdit(new QDoubleSpinBox),
      stationsTable(new QTableWidget), pilesTable(new QTableWidget),
      pileIdSpin(new QSpinBox),
      orderPhoneFilter(new QLineEdit), orderStationFilter(new QComboBox),
      orderStatusFilter(new QComboBox), orderFromDate(new QDateEdit),
      orderToDate(new QDateEdit), ordersTable(new QTableWidget),
      statTodayValue(new QLabel), statMonthValue(new QLabel), statTotalValue(new QLabel),
      statPileIdleCount(new QLabel), statPileBusyCount(new QLabel), statPileFaultCount(new QLabel),
      revenueTrendLabel(new QLabel)
{
    setWindowTitle(QStringLiteral("东软电动汽车充电平台 · 管理后台"));
    resize(1120, 700);
    setMinimumSize(960, 600);
    setCentralWidget(pages);

    applyCommonStyle();

    // ══════════════════════════════════════════════════════════════════════════
    // LOGIN PAGE
    // ══════════════════════════════════════════════════════════════════════════
    loginPage = new QWidget;
    auto *loginOuter = new QHBoxLayout(loginPage);
    loginOuter->setContentsMargins(0, 0, 0, 0);

    // Left branding panel
    auto *brandPanel = new QFrame;
    brandPanel->setFixedWidth(400);
    brandPanel->setStyleSheet(QStringLiteral(
        "QFrame { background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        "stop:0 #004ac6, stop:0.5 #2563eb, stop:1 #0053db); "
        "border-radius: 0px; }"));
    auto *brandLayout = new QVBoxLayout(brandPanel);
    brandLayout->setAlignment(Qt::AlignCenter);
    brandLayout->setContentsMargins(48, 48, 48, 48);

    auto *brandIcon = new QLabel(QStringLiteral("⚡"));
    brandIcon->setFixedSize(72, 72);
    brandIcon->setStyleSheet(QStringLiteral(
        "background:rgba(255,255,255,0.15); border-radius:36px; "
        "color:#ffffff; font-size:36px; font-weight:700; border:none;"));
    brandIcon->setAlignment(Qt::AlignCenter);
    brandLayout->addWidget(brandIcon, 0, Qt::AlignHCenter);
    brandLayout->addSpacing(24);

    auto *brandName = new QLabel(QStringLiteral("东软电动汽车充电平台"));
    brandName->setStyleSheet(QStringLiteral(
        "color:#ffffff; font-size:22px; font-weight:700; background:transparent; border:none;"));
    brandName->setAlignment(Qt::AlignHCenter);
    brandLayout->addWidget(brandName);

    auto *brandSub = new QLabel(QStringLiteral("智慧新能源充电服务平台"));
    brandSub->setStyleSheet(QStringLiteral(
        "color:rgba(255,255,255,0.75); font-size:14px; background:transparent; border:none;"));
    brandSub->setAlignment(Qt::AlignHCenter);
    brandLayout->addWidget(brandSub);

    brandLayout->addSpacing(40);

    auto *brandTagline = new QLabel(QStringLiteral("管理后台 · 运营数据一目了然"));
    brandTagline->setStyleSheet(QStringLiteral(
        "color:rgba(255,255,255,0.6); font-size:12px; background:transparent; border:none;"));
    brandTagline->setAlignment(Qt::AlignHCenter);
    brandLayout->addWidget(brandTagline);

    brandLayout->addStretch();
    loginOuter->addWidget(brandPanel);

    // Right login form panel
    auto *formPanel = new QWidget;
    formPanel->setStyleSheet(QStringLiteral("background:#faf8ff;"));
    auto *formPanelLayout = new QVBoxLayout(formPanel);
    formPanelLayout->setAlignment(Qt::AlignCenter);
    formPanelLayout->setContentsMargins(60, 60, 60, 60);

    auto *formCard = new QFrame;
    formCard->setObjectName(QStringLiteral("loginCard"));
    formCard->setMinimumWidth(360);
    formCard->setMaximumWidth(420);
    applyShadow(formCard, 24, 8, QColor(19, 27, 46, 18));

    auto *cardLayout = new QVBoxLayout(formCard);
    cardLayout->setContentsMargins(36, 40, 36, 36);
    cardLayout->setSpacing(0);

    auto *logoLabel = new QLabel(QStringLiteral("⚡"));
    logoLabel->setObjectName(QStringLiteral("loginLogo"));
    logoLabel->setFixedSize(64, 64);
    logoLabel->setAlignment(Qt::AlignCenter);
    cardLayout->addWidget(logoLabel, 0, Qt::AlignHCenter);
    cardLayout->addSpacing(20);

    auto *titleLabel = new QLabel(QStringLiteral("管理员登录"));
    titleLabel->setObjectName(QStringLiteral("loginTitle"));
    titleLabel->setAlignment(Qt::AlignHCenter);
    cardLayout->addWidget(titleLabel);
    cardLayout->addSpacing(6);

    auto *taglineLabel = new QLabel(QStringLiteral("登录以访问平台管理功能"));
    taglineLabel->setObjectName(QStringLiteral("loginTagline"));
    taglineLabel->setAlignment(Qt::AlignHCenter);
    cardLayout->addWidget(taglineLabel);
    cardLayout->addSpacing(32);

    // Username
    auto *userLabel = new QLabel(QStringLiteral("用户名"));
    userLabel->setStyleSheet(QStringLiteral("color:#434655; font-size:13px; font-weight:600; background:transparent; border:none;"));
    cardLayout->addWidget(userLabel);
    cardLayout->addSpacing(6);
    usernameEdit->setPlaceholderText(QStringLiteral("请输入管理员用户名"));
    usernameEdit->setText(QStringLiteral("admin"));
    usernameEdit->setMinimumHeight(44);
    usernameEdit->setStyleSheet(QStringLiteral(
        "QLineEdit { background:#f2f3ff; border:1px solid #eaedff; border-radius:12px; "
        "padding:10px 14px; color:#131b2e; font-size:14px; }"
        "QLineEdit:focus { border:1px solid #2563eb; background:#ffffff; }"));
    cardLayout->addWidget(usernameEdit);
    cardLayout->addSpacing(16);

    // Password
    auto *passLabel = new QLabel(QStringLiteral("密码"));
    passLabel->setStyleSheet(QStringLiteral("color:#434655; font-size:13px; font-weight:600; background:transparent; border:none;"));
    cardLayout->addWidget(passLabel);
    cardLayout->addSpacing(6);
    passwordEdit->setEchoMode(QLineEdit::Password);
    passwordEdit->setPlaceholderText(QStringLiteral("请输入密码"));
    passwordEdit->setText(QStringLiteral("123456"));
    passwordEdit->setMinimumHeight(44);
    passwordEdit->setStyleSheet(QStringLiteral(
        "QLineEdit { background:#f2f3ff; border:1px solid #eaedff; border-radius:12px; "
        "padding:10px 14px; color:#131b2e; font-size:14px; }"
        "QLineEdit:focus { border:1px solid #2563eb; background:#ffffff; }"));
    cardLayout->addWidget(passwordEdit);
    cardLayout->addSpacing(24);

    // Login button
    auto *loginBtn = new QPushButton(QStringLiteral("登 录"));
    loginBtn->setObjectName(QStringLiteral("primaryBtn"));
    loginBtn->setMinimumHeight(48);
    loginBtn->setCursor(Qt::PointingHandCursor);
    loginBtn->setAutoDefault(false);
    loginBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background:#2563eb; color:#ffffff; border:none; border-radius:12px; "
        "font-size:14px; font-weight:600; padding:10px 20px; }"
        "QPushButton:hover { background:#1d4ed8; }"
        "QPushButton:pressed { background:#003ea8; }"));
    cardLayout->addWidget(loginBtn);
    cardLayout->addSpacing(12);

    // Error label
    loginStatus->setObjectName(QStringLiteral("errorLabel"));
    loginStatus->setAlignment(Qt::AlignCenter);
    loginStatus->setWordWrap(true);
    cardLayout->addWidget(loginStatus);

    formPanelLayout->addWidget(formCard, 0, Qt::AlignCenter);
    loginOuter->addWidget(formPanel, 1);

    pages->addWidget(loginPage);
    connect(loginBtn, &QPushButton::clicked, this, &AdminWindow::login);

    // ══════════════════════════════════════════════════════════════════════════
    // DASHBOARD PAGE
    // ══════════════════════════════════════════════════════════════════════════
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

    // ══════════════════════════════════════════════════════════════════════════
    // TAB 0: REVENUE & STATS
    // ══════════════════════════════════════════════════════════════════════════
    auto *statsPage = new QWidget;
    statsPage->setStyleSheet(QStringLiteral("background:#f2f6ff;"));
    auto *statsScroll = new QScrollArea;
    statsScroll->setWidgetResizable(true);
    statsScroll->setFrameShape(QFrame::NoFrame);
    statsScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *statsInner = new QWidget;
    auto *statsMainLayout = new QVBoxLayout(statsInner);
    statsMainLayout->setContentsMargins(28, 24, 28, 24);
    statsMainLayout->setSpacing(20);

    // ── Revenue summary cards row ────────────────────────────────────────────
    auto *revenueRow = new QHBoxLayout;
    revenueRow->setSpacing(16);

    struct StatCardInfo {
        QLabel *&value;
        const char *label;
        const char *sub;
    };
    StatCardInfo revenueCards[] = {
        {statTodayValue, "今日营收（元）", "自动每日零点重置"},
        {statMonthValue, "本月营收（元）", "自然月累计"},
        {statTotalValue, "累计营收（元）", "平台历史总营收"},
    };
    for (int i = 0; i < 3; ++i) {
        auto *card = new QFrame;
        card->setObjectName(i == 0 ? QStringLiteral("statCardAccent") : QStringLiteral("statCard"));
        card->setMinimumHeight(120);
        applyShadow(card, 16, 4, QColor(19, 27, 46, 12));
        auto *cl = new QVBoxLayout(card);
        cl->setContentsMargins(20, 18, 20, 18);
        cl->setSpacing(4);
        auto *lbl = new QLabel(QString::fromUtf8(revenueCards[i].label));
        lbl->setObjectName(QStringLiteral("statLabel"));
        cl->addWidget(lbl);
        cl->addSpacing(4);
        revenueCards[i].value->setObjectName(QStringLiteral("statValue"));
        revenueCards[i].value->setText(QStringLiteral("¥0.00"));
        cl->addWidget(revenueCards[i].value);
        cl->addSpacing(2);
        auto *sub = new QLabel(QString::fromUtf8(revenueCards[i].sub));
        sub->setObjectName(QStringLiteral("statSub"));
        cl->addWidget(sub);
        cl->addStretch();
        revenueRow->addWidget(card, 1);
    }
    statsMainLayout->addLayout(revenueRow);

    // ── Pile status cards row ────────────────────────────────────────────────
    auto *pileRow = new QHBoxLayout;
    pileRow->setSpacing(16);

    struct PileCardInfo {
        QLabel *&count;
        const char *label;
        const char *dotObj;
        const char *cardObj;
        const char *countObj;
    };
    PileCardInfo pileCards[] = {
        {statPileIdleCount, "空闲电桩", "dotIdle", "pileCardIdle", "countIdle"},
        {statPileBusyCount, "充电中", "dotBusy", "pileCardBusy", "countBusy"},
        {statPileFaultCount, "故障", "dotFault", "pileCardFault", "countFault"},
    };
    for (int i = 0; i < 3; ++i) {
        auto *card = new QFrame;
        card->setObjectName(QString::fromUtf8(pileCards[i].cardObj));
        card->setMinimumHeight(72);
        applyShadow(card, 16, 4, QColor(19, 27, 46, 12));
        auto *cl = new QHBoxLayout(card);
        cl->setContentsMargins(18, 14, 18, 14);
        cl->setSpacing(14);

        auto *dot = new QLabel;
        dot->setObjectName(QString::fromUtf8(pileCards[i].dotObj));
        dot->setFixedSize(10, 10);
        cl->addWidget(dot, 0, Qt::AlignTop);

        auto *textCol = new QVBoxLayout;
        textCol->setSpacing(2);
        auto *lbl = new QLabel(QString::fromUtf8(pileCards[i].label));
        lbl->setObjectName(QStringLiteral("statLabel"));
        textCol->addWidget(lbl);
        pileCards[i].count->setObjectName(QString::fromUtf8(pileCards[i].countObj));
        pileCards[i].count->setText(QStringLiteral("0"));
        textCol->addWidget(pileCards[i].count);
        textCol->addStretch();
        cl->addLayout(textCol, 1);
        pileRow->addWidget(card, 1);
    }
    statsMainLayout->addLayout(pileRow);

    // ── 7-Day Revenue Trend ──────────────────────────────────────────────────
    auto *trendCard = new QFrame;
    trendCard->setObjectName(QStringLiteral("contentCard"));
    applyShadow(trendCard, 16, 4, QColor(19, 27, 46, 12));
    auto *trendCardLayout = new QVBoxLayout(trendCard);
    trendCardLayout->setContentsMargins(24, 20, 24, 24);
    trendCardLayout->setSpacing(12);

    auto *trendTitle = new QLabel(QStringLiteral("近 7 日营收趋势"));
    trendTitle->setStyleSheet(QStringLiteral(
        "color:#131b2e; font-size:16px; font-weight:700; background:transparent; border:none;"));
    trendCardLayout->addWidget(trendTitle);
    trendCardLayout->addWidget(makeDivider());
    trendCardLayout->addSpacing(4);

    revenueTrendLabel->setObjectName(QStringLiteral("trendLabel"));
    revenueTrendLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    revenueTrendLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    revenueTrendLabel->setWordWrap(true);
    revenueTrendLabel->setText(QStringLiteral("暂无已结算订单数据"));
    trendCardLayout->addWidget(revenueTrendLabel);
    trendCardLayout->addStretch();

    statsMainLayout->addWidget(trendCard);
    statsMainLayout->addStretch();

    statsScroll->setWidget(statsInner);
    auto *statsOuterLayout = new QVBoxLayout(statsPage);
    statsOuterLayout->setContentsMargins(0, 0, 0, 0);
    statsOuterLayout->addWidget(statsScroll);
    contentStack->addWidget(statsPage);

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
    tableCardLayout->addWidget(usersTable);
    usersMainLayout->addWidget(tableCard, 1);

    contentStack->addWidget(usersPage);
    connect(refreshUsersBtn, &QPushButton::clicked, this, &AdminWindow::refreshUsers);
    connect(toggleUserBtn, &QPushButton::clicked, this, &AdminWindow::toggleSelectedUser);

    // ══════════════════════════════════════════════════════════════════════════
    // TAB 2: STATIONS & PILES
    // ══════════════════════════════════════════════════════════════════════════
    auto *stationsPage = new QWidget;
    stationsPage->setStyleSheet(QStringLiteral("background:#fff8ef;"));
    auto *stationsScroll = new QScrollArea;
    stationsScroll->setWidgetResizable(true);
    stationsScroll->setFrameShape(QFrame::NoFrame);
    stationsScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *stationsInner = new QWidget;
    auto *stationsMainLayout = new QVBoxLayout(stationsInner);
    stationsMainLayout->setContentsMargins(28, 24, 28, 24);
    stationsMainLayout->setSpacing(16);

    // ── Add Station Form Card ────────────────────────────────────────────────
    auto *addCard = new QFrame;
    addCard->setObjectName(QStringLiteral("contentCard"));
    auto *addCardLayout = new QVBoxLayout(addCard);
    addCardLayout->setContentsMargins(24, 20, 24, 20);
    addCardLayout->setSpacing(16);

    auto *addTitle = new QLabel(QStringLiteral("新增充电站"));
    addTitle->setStyleSheet(QStringLiteral(
        "color:#131b2e; font-size:16px; font-weight:700; background:transparent; border:none;"));
    addCardLayout->addWidget(addTitle);
    addCardLayout->addWidget(makeDivider());

    auto *formGrid = new QGridLayout;
    formGrid->setSpacing(12);
    formGrid->setContentsMargins(0, 8, 0, 0);

    auto addField = [&](int row, int col, const QString &label, QWidget *widget) {
        auto *lbl = new QLabel(label);
        lbl->setStyleSheet(QStringLiteral(
            "color:#737686; font-size:12px; font-weight:600; background:transparent; border:none;"));
        formGrid->addWidget(lbl, row * 2, col);
        widget->setMinimumHeight(40);
        formGrid->addWidget(widget, row * 2 + 1, col);
    };

    stationNameEdit->setPlaceholderText(QStringLiteral("充电站名称"));
    stationAddressEdit->setPlaceholderText(QStringLiteral("详细地址"));
    stationLongitudeEdit->setRange(-180, 180);
    stationLatitudeEdit->setRange(-90, 90);
    stationPriceEdit->setRange(0, 100000);
    stationPriceEdit->setDecimals(2);
    stationPriceEdit->setPrefix(QStringLiteral("¥ "));
    stationPriceEdit->setSuffix(QStringLiteral(" /kWh"));

    addField(0, 0, QStringLiteral("站点名称"), stationNameEdit);
    addField(0, 1, QStringLiteral("地址"), stationAddressEdit);
    addField(1, 0, QStringLiteral("经度"), stationLongitudeEdit);
    addField(1, 1, QStringLiteral("纬度"), stationLatitudeEdit);
    addField(2, 0, QStringLiteral("充电单价"), stationPriceEdit);

    addCardLayout->addLayout(formGrid);

    auto *addStationBtn = new QPushButton(QStringLiteral("＋  新增充电站"));
    addStationBtn->setObjectName(QStringLiteral("primaryBtn"));
    addStationBtn->setMinimumHeight(44);
    addStationBtn->setCursor(Qt::PointingHandCursor);
    addCardLayout->addWidget(addStationBtn, 0, Qt::AlignRight);
    stationsMainLayout->addWidget(addCard);

    // ── Refresh button ───────────────────────────────────────────────────────
    auto *stationRefreshBtn = new QPushButton(QStringLiteral("🔄  刷新站点与电桩数据"));
    stationRefreshBtn->setObjectName(QStringLiteral("secondaryBtn"));
    stationRefreshBtn->setMinimumHeight(40);
    stationRefreshBtn->setCursor(Qt::PointingHandCursor);
    stationsMainLayout->addWidget(stationRefreshBtn);

    // ── Stations Table Card ──────────────────────────────────────────────────
    auto *stationTableCard = new QFrame;
    stationTableCard->setObjectName(QStringLiteral("contentCard"));
    applyShadow(stationTableCard, 16, 4, QColor(19, 27, 46, 12));
    auto *stcLayout = new QVBoxLayout(stationTableCard);
    stcLayout->setContentsMargins(0, 0, 0, 0);

    stationsTable->setColumnCount(6);
    stationsTable->setHorizontalHeaderLabels({
        QStringLiteral("ID"), QStringLiteral("名称"), QStringLiteral("地址"),
        QStringLiteral("价格"), QStringLiteral("空闲/总数"), QStringLiteral("在线率")
    });
    stationsTable->horizontalHeader()->setStretchLastSection(true);
    stationsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    stationsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    stationsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    stationsTable->setAlternatingRowColors(true);
    stationsTable->setShowGrid(false);
    stationsTable->verticalHeader()->setVisible(false);
    stationsTable->verticalHeader()->setDefaultSectionSize(48);
    stcLayout->addWidget(stationsTable);
    stationsMainLayout->addWidget(stationTableCard);

    // ── Piles Table Card ─────────────────────────────────────────────────────
    auto *pileTableCard = new QFrame;
    pileTableCard->setObjectName(QStringLiteral("contentCard"));
    applyShadow(pileTableCard, 16, 4, QColor(19, 27, 46, 12));
    auto *ptcLayout = new QVBoxLayout(pileTableCard);
    ptcLayout->setContentsMargins(0, 0, 0, 0);

    pilesTable->setColumnCount(7);
    pilesTable->setHorizontalHeaderLabels({
        QStringLiteral("ID"), QStringLiteral("站点"), QStringLiteral("编号"),
        QStringLiteral("类型"), QStringLiteral("功率"), QStringLiteral("状态"),
        QStringLiteral("累计次数")
    });
    pilesTable->horizontalHeader()->setStretchLastSection(true);
    pilesTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    pilesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    pilesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    pilesTable->setAlternatingRowColors(true);
    pilesTable->setShowGrid(false);
    pilesTable->verticalHeader()->setVisible(false);
    pilesTable->verticalHeader()->setDefaultSectionSize(48);
    ptcLayout->addWidget(pilesTable);

    // Pile controls row
    auto *pileControlRow = new QHBoxLayout;
    pileControlRow->setContentsMargins(20, 12, 20, 16);
    pileControlRow->setSpacing(12);
    auto *pileIdLabel = new QLabel(QStringLiteral("电桩 ID"));
    pileIdLabel->setStyleSheet(QStringLiteral(
        "color:#737686; font-size:13px; font-weight:500; background:transparent; border:none;"));
    pileControlRow->addWidget(pileIdLabel);
    pileIdSpin->setMinimum(1);
    pileIdSpin->setMinimumHeight(36);
    pileIdSpin->setFixedWidth(100);
    pileControlRow->addWidget(pileIdSpin);
    auto *restartBtn = new QPushButton(QStringLiteral("重启电桩"));
    restartBtn->setObjectName(QStringLiteral("warningBtn"));
    restartBtn->setMinimumHeight(36);
    restartBtn->setCursor(Qt::PointingHandCursor);
    pileControlRow->addWidget(restartBtn);
    pileControlRow->addStretch();
    ptcLayout->addLayout(pileControlRow);

    stationsMainLayout->addWidget(pileTableCard);
    stationsMainLayout->addStretch();

    stationsScroll->setWidget(stationsInner);
    auto *stationsOuterLayout = new QVBoxLayout(stationsPage);
    stationsOuterLayout->setContentsMargins(0, 0, 0, 0);
    stationsOuterLayout->addWidget(stationsScroll);
    contentStack->addWidget(stationsPage);

    connect(addStationBtn, &QPushButton::clicked, this, &AdminWindow::addStation);
    connect(stationRefreshBtn, &QPushButton::clicked, this, &AdminWindow::refreshStationsAndPiles);
    connect(restartBtn, &QPushButton::clicked, this, &AdminWindow::restartSelectedPile);

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

    orderStationFilter->addItem(QStringLiteral("全部站点"), -1);
    orderStationFilter->setMinimumHeight(40);
    orderFilterRow1->addWidget(orderStationFilter);

    orderStatusFilter->addItem(QStringLiteral("全部状态"), QString());
    orderStatusFilter->addItem(QStringLiteral("充电中"), QStringLiteral("充电中"));
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
    connect(ordersRefreshBtn, &QPushButton::clicked, this, &AdminWindow::refreshOrders);

    // ══════════════════════════════════════════════════════════════════════════
    // Connection signals
    // ══════════════════════════════════════════════════════════════════════════
    connect(connection, &ClientConnection::responseReceived, this, &AdminWindow::handleResponse);
    connect(connection, &ClientConnection::connectionError, this, &AdminWindow::handleError);
}

// ─── Style Application ─────────────────────────────────────────────────────────
void AdminWindow::applyCommonStyle()
{
    setStyleSheet(stylesheet());
}

void AdminWindow::applyLoginStyle() {}
void AdminWindow::applyDashboardStyle() {}

// ─── Navigation ────────────────────────────────────────────────────────────────
void AdminWindow::switchTab(int index)
{
    contentStack->setCurrentIndex(index);
    updateNavButtons(index);
}

void AdminWindow::updateNavButtons(int activeIndex)
{
    for (int i = 0; i < navButtons.size(); ++i) {
        navButtons[i]->setChecked(i == activeIndex);
    }
}

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

// ─── Data Refresh ──────────────────────────────────────────────────────────────
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

void AdminWindow::addStation()
{
    send(QStringLiteral("admin_add_station"), {
        {"name", stationNameEdit->text()},
        {"address", stationAddressEdit->text()},
        {"longitude", stationLongitudeEdit->value()},
        {"latitude", stationLatitudeEdit->value()},
        {"price", stationPriceEdit->value()}
    });
}

void AdminWindow::restartSelectedPile()
{
    send(QStringLiteral("admin_restart_pile"), {{"pileId", pileIdSpin->value()}});
}

void AdminWindow::refreshStats()
{
    send(QStringLiteral("admin_stats"));
}

void AdminWindow::refreshOrders()
{
    send(QStringLiteral("admin_orders"), {
        {"phoneKeyword", orderPhoneFilter->text().trimmed()},
        {"stationId", orderStationFilter->currentData().toInt()},
        {"status", orderStatusFilter->currentData().toString()},
        {"fromDate", orderFromDate->date().toString(QStringLiteral("yyyy-MM-dd"))},
        {"toDate", orderToDate->date().toString(QStringLiteral("yyyy-MM-dd"))}
    });
}

// ─── Stats Cards Population ────────────────────────────────────────────────────
void AdminWindow::populateStatsCards(const QJsonObject &data)
{
    statTodayValue->setText(QStringLiteral("¥%1").arg(
        data.value("revenueToday").toDouble(), 0, 'f', 2));
    statMonthValue->setText(QStringLiteral("¥%1").arg(
        data.value("revenueThisMonth").toDouble(), 0, 'f', 2));
    statTotalValue->setText(QStringLiteral("¥%1").arg(
        data.value("revenueTotal").toDouble(), 0, 'f', 2));

    const QJsonObject pileStatus = data.value("pileStatus").toObject();
    statPileIdleCount->setText(QString::number(pileStatus.value("闲置").toInt()));
    statPileBusyCount->setText(QString::number(pileStatus.value("在用").toInt()));
    statPileFaultCount->setText(QString::number(pileStatus.value("故障").toInt()));
}

void AdminWindow::populateRevenueTrend(const QJsonArray &trend)
{
    if (trend.isEmpty()) {
        revenueTrendLabel->setText(QStringLiteral("暂无已结算订单数据"));
        return;
    }

    QString html;
    html += QStringLiteral("<table style='width:100%; border-collapse:collapse;'>");
    html += QStringLiteral("<tr>"
        "<td style='color:#737686; font-size:12px; font-weight:600; padding:6px 12px; border-bottom:1px solid #eaedff;'>日期</td>"
        "<td style='color:#737686; font-size:12px; font-weight:600; padding:6px 12px; border-bottom:1px solid #eaedff;'>营收（元）</td>"
        "</tr>");

    QStringList dates;
    QMap<QString, double> revenueByDate;
    for (const QJsonValue &val : trend) {
        const QJsonObject pt = val.toObject();
        const QString date = pt.value("date").toString();
        dates.append(date);
        revenueByDate.insert(date, pt.value("revenue").toDouble());
    }
    dates.sort(Qt::CaseSensitive);
    std::sort(dates.begin(), dates.end(), std::greater<QString>());

    for (const QString &date : dates) {
        const double rev = revenueByDate.value(date);
        html += QStringLiteral("<tr>"
            "<td style='color:#434655; font-size:13px; padding:8px 12px; border-bottom:1px solid #f2f3ff;'>%1</td>"
            "<td style='color:#131b2e; font-family:Inter,Consolas; font-size:14px; font-weight:600; padding:8px 12px; border-bottom:1px solid #f2f3ff;'>¥%2</td>"
            "</tr>").arg(date).arg(rev, 0, 'f', 2);
    }
    html += QStringLiteral("</table>");
    revenueTrendLabel->setText(html);
}

// ─── Response Handler ──────────────────────────────────────────────────────────
void AdminWindow::handleResponse(const QJsonObject &response)
{
    const QString action = response.value(QStringLiteral("_requestAction"))
                               .toString(pendingAction);
    const int code = response.value("code").toInt(-1);
    if (code != 0) {
        showError(response.value("msg").toString());
        return;
    }

    const QJsonObject data = response.value("data").toObject();
    if (action == QStringLiteral("admin_login")) {
        pages->setCurrentIndex(1);
        contentStack->setCurrentIndex(0);
        requestInitialData();
    } else if (data.contains("users")) {
        const QJsonArray users = data.value("users").toArray();
        usersTable->setRowCount(users.size());
        for (int row = 0; row < users.size(); ++row) {
            const QJsonObject user = users.at(row).toObject();
            usersTable->setItem(row, 0, new QTableWidgetItem(QString::number(user.value("userId").toInt())));
            usersTable->setItem(row, 1, new QTableWidgetItem(user.value("phone").toString()));
            usersTable->setItem(row, 2, new QTableWidgetItem(user.value("nickname").toString()));
            usersTable->setItem(row, 3, new QTableWidgetItem(
                QStringLiteral("¥%1").arg(user.value("balance").toDouble(), 0, 'f', 2)));

            const QString status = user.value("status").toString();
            auto *statusItem = new QTableWidgetItem(status);
            if (status == QStringLiteral("冻结")) {
                statusItem->setForeground(QColor(cDangerText));
                statusItem->setBackground(QColor(cDangerBg));
            } else {
                statusItem->setForeground(QColor(cSuccess));
                statusItem->setBackground(QColor(cSuccessBg));
            }
            usersTable->setItem(row, 4, statusItem);
            usersTable->setItem(row, 5, new QTableWidgetItem(user.value("createdAt").toString()));
        }
    } else if (data.contains("stations")) {
        const QJsonArray stations = data.value("stations").toArray();
        const QVariant selectedStation = orderStationFilter->currentData();
        orderStationFilter->clear();
        orderStationFilter->addItem(QStringLiteral("全部站点"), -1);
        stationsTable->setRowCount(stations.size());
        for (int row = 0; row < stations.size(); ++row) {
            const QJsonObject station = stations.at(row).toObject();
            orderStationFilter->addItem(station.value("name").toString(), station.value("stationId").toInt());
            stationsTable->setItem(row, 0, new QTableWidgetItem(QString::number(station.value("stationId").toInt())));
            stationsTable->setItem(row, 1, new QTableWidgetItem(station.value("name").toString()));
            stationsTable->setItem(row, 2, new QTableWidgetItem(station.value("address").toString()));
            stationsTable->setItem(row, 3, new QTableWidgetItem(
                QStringLiteral("¥%1").arg(station.value("price").toDouble(), 0, 'f', 2)));

            const int free = station.value("freePileCount").toInt();
            const int total = station.value("pileCount").toInt();
            auto *availItem = new QTableWidgetItem(QStringLiteral("%1 / %2").arg(free).arg(total));
            availItem->setForeground(free > 0 ? QColor(cSuccess) : QColor(cDanger));
            stationsTable->setItem(row, 4, availItem);

            stationsTable->setItem(row, 5, new QTableWidgetItem(
                QStringLiteral("%1%").arg(station.value("onlineRate").toDouble(), 0, 'f', 1)));
        }
        const int selectedIndex = orderStationFilter->findData(selectedStation);
        if (selectedIndex >= 0) orderStationFilter->setCurrentIndex(selectedIndex);
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
            pilesTable->setItem(row, 4, new QTableWidgetItem(
                QStringLiteral("%1 kW").arg(pile.value("power").toDouble(), 0, 'f', 1)));

            const QString pileStatus = pile.value("status").toString();
            auto *statusItem = new QTableWidgetItem(pileStatus);
            if (pileStatus == QStringLiteral("空闲")) {
                statusItem->setForeground(QColor(cSuccess));
                statusItem->setBackground(QColor(cSuccessBg));
            } else if (pileStatus == QStringLiteral("充电中") || pileStatus == QStringLiteral("在用")) {
                statusItem->setForeground(QColor(cWarning));
                statusItem->setBackground(QColor(cWarningBg));
            } else {
                statusItem->setForeground(QColor(cDanger));
                statusItem->setBackground(QColor(cDangerBg));
            }
            pilesTable->setItem(row, 5, statusItem);
            pilesTable->setItem(row, 6, new QTableWidgetItem(QString::number(pile.value("totalSessions").toInt())));
        }
    } else if (data.contains("revenueToday") && data.contains("pileStatus")) {
        populateStatsCards(data);
        populateRevenueTrend(data.value("revenueTrend").toArray());
    } else if (data.contains("orders")) {
        const QJsonArray orders = data.value("orders").toArray();
        ordersTable->setRowCount(orders.size());
        for (int row = 0; row < orders.size(); ++row) {
            const QJsonObject order = orders.at(row).toObject();
            ordersTable->setItem(row, 0, new QTableWidgetItem(QString::number(order.value("orderId").toInt())));
            ordersTable->setItem(row, 1, new QTableWidgetItem(QString::number(order.value("userId").toInt())));
            ordersTable->setItem(row, 2, new QTableWidgetItem(order.value("phone").toString()));
            ordersTable->setItem(row, 3, new QTableWidgetItem(order.value("stationName").toString()));
            ordersTable->setItem(row, 4, new QTableWidgetItem(QString::number(order.value("pileId").toInt())));
            ordersTable->setItem(row, 5, new QTableWidgetItem(order.value("startTime").toString()));
            ordersTable->setItem(row, 6, new QTableWidgetItem(order.value("endTime").toString()));
            ordersTable->setItem(row, 7, new QTableWidgetItem(
                QStringLiteral("%1 kWh").arg(order.value("amount").toDouble(), 0, 'f', 2)));
            ordersTable->setItem(row, 8, new QTableWidgetItem(
                QStringLiteral("¥%1").arg(order.value("fee").toDouble(), 0, 'f', 2)));

            const QString oStatus = order.value("status").toString();
            auto *sItem = new QTableWidgetItem(oStatus);
            if (oStatus == QStringLiteral("已结算")) {
                sItem->setForeground(QColor(cSuccess));
                sItem->setBackground(QColor(cSuccessBg));
            } else if (oStatus == QStringLiteral("充电中")) {
                sItem->setForeground(QColor(cWarning));
                sItem->setBackground(QColor(cWarningBg));
            } else {
                sItem->setForeground(QColor(cDanger));
                sItem->setBackground(QColor(cDangerBg));
            }
            ordersTable->setItem(row, 9, sItem);
        }
        return;
    } else if (action == QStringLiteral("admin_add_station")) {
        stationNameEdit->clear();
        stationAddressEdit->clear();
        stationLongitudeEdit->setValue(0);
        stationLatitudeEdit->setValue(0);
        stationPriceEdit->setValue(0);
        refreshStationsAndPiles();
    } else if (action == QStringLiteral("set_user_status")) {
        refreshUsers();
    } else if (action == QStringLiteral("admin_restart_pile")) {
        refreshStationsAndPiles();
    }
}

// ─── Error Handling ────────────────────────────────────────────────────────────
void AdminWindow::handleError(const QString &message)
{
    showError(message);
}

void AdminWindow::showError(const QString &message)
{
    if (pages->currentIndex() == 0) {
        loginStatus->setText(QStringLiteral("⚠ %1").arg(message));
    } else {
        QMessageBox msgBox(this);
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setWindowTitle(QStringLiteral("操作失败"));
        msgBox.setText(message);
        msgBox.setStyleSheet(QStringLiteral(
            "QMessageBox { background: #ffffff; }"
            "QLabel { color: #131b2e; font-size: 14px; }"
            "QPushButton { background: #2563eb; color: #ffffff; border: none; "
            "border-radius: 10px; padding: 8px 24px; font-weight: 600; min-width: 60px; }"
            "QPushButton:hover { background: #1d4ed8; }"
        ));
        msgBox.exec();
    }
}
