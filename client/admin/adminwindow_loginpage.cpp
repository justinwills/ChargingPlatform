#include "adminwindow.h"
#include "admindesign.h"

#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>
#include <QStackedWidget>

// Builds the login page (pages->widget 0): branding panel + credentials
// form. Extracted from the AdminWindow constructor.
void AdminWindow::buildLoginPage()
{
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

}
