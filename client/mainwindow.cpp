#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "admin/adminwindow.h"
#include "flowlayout.h"
#include "navigationpage.h"

#include <QMessageBox>
#include <QInputDialog>
#include <QJsonArray>
#include <QRegularExpression>
#include <QtGlobal>
#include <QJsonObject>
#include <QFileDialog>
#include <QPixmap>
#include <QPainter>
#include <QPainterPath>
#include <QButtonGroup>
#include <QDialog>
#include <QGraphicsDropShadowEffect>
#include <QFrame>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLayoutItem>
#include <QPushButton>
#include <QLabel>
#include <QIcon>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QToolButton>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
        , connection(new ClientConnection(this))
{
    ui->setupUi(this);

    m_navigationPage = new NavigationPage(ui->stackedWidget);
    ui->stackedWidget->addWidget(m_navigationPage);

    connect(m_navigationPage, &NavigationPage::backRequested, this, [this]() {
        if (m_navigationStarted && connection->isConnected()) {
            connection->sendRequest(QStringLiteral("end_navigation"), {});
        }
        leaveNavigationPage();
    });
    connect(m_navigationPage, &NavigationPage::endRequested, this, [this]() {
        if (m_navigationStarted && connection->isConnected()) {
            connection->sendRequest(QStringLiteral("end_navigation"), {});
        }
        leaveNavigationPage();
    });
    connect(m_navigationPage, &NavigationPage::planRequested, this,
            [this](const QString &originAddress, const QString &mode) {
        if (!originAddress.isEmpty()) {
            m_hasNavigationOrigin = false;
            requestNavigation(mode, originAddress);
        } else if (m_hasNavigationOrigin) {
            requestNavigation(mode);
        } else {
            m_navigationPage->requestCurrentLocation();
        }
    });
    connect(m_navigationPage, &NavigationPage::modeChanged, this,
            [this](const QString &mode) {
        if (m_navigationStarted && connection->isConnected()) {
            m_navigationPage->setBusy(true);
            connection->sendRequest(QStringLiteral("switch_navigation_mode"), {
                {QStringLiteral("mode"), mode}
            });
        } else if (m_hasNavigationOrigin) {
            requestNavigation(mode);
        } else {
            m_navigationPage->requestCurrentLocation();
        }
    });
    connect(m_navigationPage, &NavigationPage::currentLocationResolved, this,
            [this](double latitude, double longitude) {
        if (ui->stackedWidget->currentWidget() != m_navigationPage) {
            return;
        }
        m_navigationOriginLat = latitude;
        m_navigationOriginLng = longitude;
        m_hasNavigationOrigin = true;
        requestNavigation(m_navigationPage->mode());
    });

    ui->stackedWidget->setCurrentWidget(ui->pageLogin);
    ui->widgetNavigation->hide();
    ui->labelMonBadge->hide();
    ui->labelMonStatus->hide();

    auto *navShadow = new QGraphicsDropShadowEffect(ui->widgetNavigation);
    navShadow->setBlurRadius(24);
    navShadow->setOffset(0, -6);
    navShadow->setColor(QColor(15, 23, 42, 40));
    ui->widgetNavigation->setGraphicsEffect(navShadow);

    applyShadow(ui->balanceFrame);
    applyShadow(ui->loginCard);
    applyShadow(ui->profileCard);
    applyShadow(ui->menuCard);
    applyShadow(ui->startStationCard);
    applyShadow(ui->guideCard);
    applyShadow(ui->monitorCard);
    applyShadow(ui->sessionCard);
    applyShadow(ui->sdStatsCard);
    applyShadow(ui->labelOrderStationDetails);
    applyShadow(ui->walletCard);

    QButtonGroup *navGroup = new QButtonGroup(this);
    navGroup->addButton(ui->BtnHome);
    navGroup->addButton(ui->BtnCharge);
    navGroup->addButton(ui->BtnMine);

    navGroup->setExclusive(true);
    ui->BtnHome->setChecked(true);

    setupNavIcons();
    setupChargePageUi();

    connect(ui->comboPile, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int) { updateChargePileCard(); });

    connect(connection, &ClientConnection::responseReceived,
        this, &MainWindow::onServerResponse);

    connect(connection, &ClientConnection::connectionError,
            this, &MainWindow::onConnectionError);

    connect(connection,
            &ClientConnection::connected,
            this,
            []() {
                qDebug() << "已经连接到充电平台服务器";
            });

    connect(connection,
            &ClientConnection::disconnected,
            this,
            []() {
                qDebug() << "与服务器断开连接";
            });

    orderTimer.setInterval(1000);
    connect(&orderTimer, &QTimer::timeout,
        this, &MainWindow::on_BtnRefreshOrder_clicked);

    displayTimer.setInterval(1000);
    connect(&displayTimer, &QTimer::timeout, this, [this]() {
        if (!activeOrderStartTime.isValid() || activeOrderId < 0) {
            ui->labelElapsedTime->setText(tr("00:00:00"));
            return;
        }

        const qint64 elapsedSeconds = qMax<qint64>(
            0, activeOrderStartTime.secsTo(QDateTime::currentDateTime()));
        const int hours = static_cast<int>(elapsedSeconds / 3600);
        const int minutes = static_cast<int>((elapsedSeconds % 3600) / 60);
        const int seconds = static_cast<int>(elapsedSeconds % 60);
        ui->labelElapsedTime->setText(
            QStringLiteral("%1:%2:%3")
                .arg(hours, 2, 10, QLatin1Char('0'))
                .arg(minutes, 2, 10, QLatin1Char('0'))
                .arg(seconds, 2, 10, QLatin1Char('0')));
    });

}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupChargePageUi()
{
    // Keep the original input widgets as the backing state for the existing
    // request flow, but replace the form-like presentation on this page.
    const auto oldWidgets = ui->pageCharge->findChildren<QWidget *>(
        QString(), Qt::FindDirectChildrenOnly);
    for (QWidget *widget : oldWidgets) {
        widget->hide();
    }

    auto *pageLayout = new QVBoxLayout(ui->pageCharge);
    pageLayout->setContentsMargins(16, 10, 16, 8);
    pageLayout->setSpacing(7);

    ui->label_title_mine_4->setText(tr("启动充电"));
    ui->label_title_mine_4->setMinimumHeight(34);
    ui->label_title_mine_4->setMaximumHeight(34);
    ui->label_title_mine_4->show();
    pageLayout->addWidget(ui->label_title_mine_4);

    auto *stepIndicator = new QWidget(ui->pageCharge);
    stepIndicator->setObjectName(QStringLiteral("chargeStepIndicator"));
    stepIndicator->setFixedHeight(46);
    auto *stepLayout = new QHBoxLayout(stepIndicator);
    stepLayout->setContentsMargins(2, 0, 2, 0);
    stepLayout->setSpacing(4);

    auto addStep = [stepIndicator, stepLayout](const QString &number,
                                               const QString &text,
                                               bool active) {
        auto *step = new QWidget(stepIndicator);
        auto *column = new QVBoxLayout(step);
        column->setContentsMargins(0, 0, 0, 0);
        column->setSpacing(2);

        auto *circle = new QLabel(number, step);
        circle->setFixedSize(25, 25);
        circle->setAlignment(Qt::AlignCenter);
        circle->setStyleSheet(active
            ? QStringLiteral("background:#2563eb;color:white;border-radius:12px;"
                             "font-size:12px;font-weight:700;")
            : QStringLiteral("background:#e8eaf2;color:#8b90a0;border-radius:12px;"
                             "font-size:12px;font-weight:700;"));
        column->addWidget(circle, 0, Qt::AlignHCenter);

        auto *caption = new QLabel(text, step);
        caption->setAlignment(Qt::AlignCenter);
        caption->setStyleSheet(active
            ? QStringLiteral("color:#2563eb;font-size:10px;font-weight:700;background:transparent;")
            : QStringLiteral("color:#9599a8;font-size:10px;background:transparent;"));
        column->addWidget(caption);
        stepLayout->addWidget(step, 1);
    };

    auto addConnector = [stepIndicator, stepLayout]() {
        auto *line = new QFrame(stepIndicator);
        line->setFixedHeight(2);
        line->setStyleSheet(QStringLiteral("background:#dfe2eb;border:none;"));
        stepLayout->addWidget(line, 1, Qt::AlignVCenter);
    };

    addStep(QStringLiteral("1"), tr("选择电桩"), true);
    addConnector();
    addStep(QStringLiteral("2"), tr("连接充电枪"), false);
    addConnector();
    addStep(QStringLiteral("3"), tr("开始充电"), false);
    pageLayout->addWidget(stepIndicator);

    auto *stationCard = new QFrame(ui->pageCharge);
    stationCard->setObjectName(QStringLiteral("chargeStationCard"));
    stationCard->setStyleSheet(QStringLiteral(
        "QFrame#chargeStationCard{background:#ffffff;border:1px solid #edf0fa;"
        "border-radius:18px;}"));
    auto *stationCardLayout = new QVBoxLayout(stationCard);
    stationCardLayout->setContentsMargins(16, 11, 12, 11);
    stationCardLayout->setSpacing(5);

    auto *stationHeading = new QLabel(tr("选择充电站"), stationCard);
    stationHeading->setStyleSheet(QStringLiteral(
        "color:#131b2e;font-size:15px;font-weight:700;background:transparent;"));
    stationCardLayout->addWidget(stationHeading);

    m_chargeStationSelectorButton = new QPushButton(stationCard);
    m_chargeStationSelectorButton->setObjectName(QStringLiteral("chargeStationSelector"));
    m_chargeStationSelectorButton->setCursor(Qt::PointingHandCursor);
    m_chargeStationSelectorButton->setMinimumHeight(60);
    m_chargeStationSelectorButton->setStyleSheet(QStringLiteral(
        "QPushButton#chargeStationSelector{background:#f8f9ff;border:none;border-radius:12px;"
        "padding:7px 10px;text-align:left;}"
        "QPushButton#chargeStationSelector:hover{background:#f0f3ff;}"
        "QPushButton#chargeStationSelector:pressed{background:#e8edff;}"));
    auto *stationRow = new QHBoxLayout(m_chargeStationSelectorButton);
    stationRow->setContentsMargins(10, 5, 7, 5);
    stationRow->setSpacing(8);
    auto *stationInfo = new QVBoxLayout;
    stationInfo->setSpacing(1);
    m_chargeStationNameLabel = new QLabel(tr("正在加载充电站…"), m_chargeStationSelectorButton);
    m_chargeStationNameLabel->setStyleSheet(QStringLiteral(
        "color:#131b2e;font-size:14px;font-weight:700;background:transparent;"));
    m_chargeStationMetaLabel = new QLabel(tr("请稍候"), m_chargeStationSelectorButton);
    m_chargeStationMetaLabel->setStyleSheet(QStringLiteral(
        "color:#737686;font-size:11px;background:transparent;"));
    m_chargeStationAvailabilityLabel = new QLabel(tr("暂无可用信息"), m_chargeStationSelectorButton);
    m_chargeStationAvailabilityLabel->setStyleSheet(QStringLiteral(
        "color:#0d7a4e;font-size:11px;font-weight:600;background:transparent;"));
    stationInfo->addWidget(m_chargeStationNameLabel);
    stationInfo->addWidget(m_chargeStationMetaLabel);
    stationInfo->addWidget(m_chargeStationAvailabilityLabel);
    stationRow->addLayout(stationInfo, 1);
    auto *stationAction = new QLabel(tr("选择  ›"), m_chargeStationSelectorButton);
    stationAction->setStyleSheet(QStringLiteral(
        "color:#2563eb;font-size:12px;font-weight:700;background:transparent;"));
    stationAction->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    stationRow->addWidget(stationAction);
    for (QLabel *label : {m_chargeStationNameLabel, m_chargeStationMetaLabel,
                          m_chargeStationAvailabilityLabel, stationAction}) {
        label->setAttribute(Qt::WA_TransparentForMouseEvents);
    }
    stationCardLayout->addWidget(m_chargeStationSelectorButton);
    pageLayout->addWidget(stationCard);
    applyShadow(stationCard);

    auto *pileCard = new QFrame(ui->pageCharge);
    pileCard->setObjectName(QStringLiteral("chargePileCard"));
    pileCard->setStyleSheet(QStringLiteral(
        "QFrame#chargePileCard{background:#ffffff;border:1px solid #edf0fa;"
        "border-radius:18px;}"));
    auto *pileCardLayout = new QVBoxLayout(pileCard);
    pileCardLayout->setContentsMargins(16, 11, 12, 11);
    pileCardLayout->setSpacing(5);
    auto *pileHeading = new QLabel(tr("选择充电桩"), pileCard);
    pileHeading->setStyleSheet(QStringLiteral(
        "color:#131b2e;font-size:15px;font-weight:700;background:transparent;"));
    pileCardLayout->addWidget(pileHeading);

    m_chargePileSelectorButton = new QPushButton(pileCard);
    m_chargePileSelectorButton->setObjectName(QStringLiteral("chargePileSelector"));
    m_chargePileSelectorButton->setCursor(Qt::PointingHandCursor);
    m_chargePileSelectorButton->setMinimumHeight(67);
    m_chargePileSelectorButton->setStyleSheet(QStringLiteral(
        "QPushButton#chargePileSelector{background:#f8f9ff;border:none;border-radius:12px;"
        "padding:7px 10px;text-align:left;}"
        "QPushButton#chargePileSelector:hover{background:#f0f3ff;}"
        "QPushButton#chargePileSelector:pressed{background:#e8edff;}"));
    auto *pileRow = new QHBoxLayout(m_chargePileSelectorButton);
    pileRow->setContentsMargins(10, 5, 5, 5);
    pileRow->setSpacing(8);
    auto *pileLeft = new QVBoxLayout;
    pileLeft->setSpacing(3);
    auto *pileCodeRow = new QHBoxLayout;
    pileCodeRow->setSpacing(6);
    m_chargePileCodeLabel = new QLabel(tr("请选择可用电桩"), m_chargePileSelectorButton);
    m_chargePileCodeLabel->setStyleSheet(QStringLiteral(
        "color:#131b2e;font-size:16px;font-weight:800;background:transparent;"));
    m_chargePileTypeLabel = new QLabel(m_chargePileSelectorButton);
    m_chargePileTypeLabel->setStyleSheet(QStringLiteral(
        "background:#e8f0ff;color:#1554d1;border-radius:9px;padding:2px 8px;"
        "font-size:10px;font-weight:700;"));
    m_chargePileTypeLabel->hide();
    pileCodeRow->addWidget(m_chargePileCodeLabel);
    pileCodeRow->addWidget(m_chargePileTypeLabel);
    pileCodeRow->addStretch();
    m_chargePilePowerLabel = new QLabel(tr("选择后即可开始充电"), m_chargePileSelectorButton);
    m_chargePilePowerLabel->setStyleSheet(QStringLiteral(
        "color:#737686;font-size:11px;background:transparent;"));
    pileLeft->addLayout(pileCodeRow);
    pileLeft->addWidget(m_chargePilePowerLabel);
    pileRow->addLayout(pileLeft, 1);

    auto *pileRight = new QVBoxLayout;
    pileRight->setSpacing(3);
    m_chargePilePriceLabel = new QLabel(QStringLiteral("—"), m_chargePileSelectorButton);
    m_chargePilePriceLabel->setAlignment(Qt::AlignRight);
    m_chargePilePriceLabel->setStyleSheet(QStringLiteral(
        "color:#2563eb;font-size:15px;font-weight:800;background:transparent;"));
    auto *pileStatusRow = new QHBoxLayout;
    pileStatusRow->setSpacing(5);
    m_chargePileStatusLabel = new QLabel(tr("未选择"), m_chargePileSelectorButton);
    m_chargePileStatusLabel->setStyleSheet(QStringLiteral(
        "background:#eceef4;color:#777c8c;border-radius:9px;padding:2px 7px;"
        "font-size:10px;font-weight:700;"));
    auto *pileChevron = new QLabel(QStringLiteral("›"), m_chargePileSelectorButton);
    pileChevron->setStyleSheet(QStringLiteral(
        "color:#9aa0b5;font-size:20px;background:transparent;"));
    pileStatusRow->addStretch();
    pileStatusRow->addWidget(m_chargePileStatusLabel);
    pileStatusRow->addWidget(pileChevron);
    pileRight->addWidget(m_chargePilePriceLabel);
    pileRight->addLayout(pileStatusRow);
    pileRow->addLayout(pileRight);
    for (QLabel *label : {m_chargePileCodeLabel, m_chargePileTypeLabel,
                          m_chargePilePowerLabel, m_chargePilePriceLabel,
                          m_chargePileStatusLabel, pileChevron}) {
        label->setAttribute(Qt::WA_TransparentForMouseEvents);
    }
    pileCardLayout->addWidget(m_chargePileSelectorButton);
    pageLayout->addWidget(pileCard);
    applyShadow(pileCard);

    auto *guideCard = new QFrame(ui->pageCharge);
    guideCard->setObjectName(QStringLiteral("chargeGuideCard"));
    guideCard->setStyleSheet(QStringLiteral(
        "QFrame#chargeGuideCard{background:#ffffff;border:1px solid #edf0fa;"
        "border-radius:16px;}"));
    auto *guideLayout = new QVBoxLayout(guideCard);
    guideLayout->setContentsMargins(15, 9, 15, 9);
    guideLayout->setSpacing(4);
    auto *guideTitle = new QLabel(tr("充电指引"), guideCard);
    guideTitle->setStyleSheet(QStringLiteral(
        "color:#131b2e;font-size:14px;font-weight:700;background:transparent;"));
    guideLayout->addWidget(guideTitle);
    auto *guideSteps = new QHBoxLayout;
    guideSteps->setSpacing(4);
    const QStringList steps{tr("1  选择充电桩"), tr("2  连接充电枪"), tr("3  开始充电")};
    for (int i = 0; i < steps.size(); ++i) {
        auto *step = new QLabel(steps.at(i), guideCard);
        step->setStyleSheet(i == 0
            ? QStringLiteral("color:#2563eb;font-size:10px;font-weight:700;background:transparent;")
            : QStringLiteral("color:#555a69;font-size:10px;font-weight:600;background:transparent;"));
        guideSteps->addWidget(step, 1);
    }
    guideLayout->addLayout(guideSteps);
    auto *guideNote = new QLabel(tr("充电完成后自动停止 · 实时费用透明可见"), guideCard);
    guideNote->setStyleSheet(QStringLiteral(
        "color:#7b8090;font-size:10px;background:transparent;"));
    guideLayout->addWidget(guideNote);
    pageLayout->addWidget(guideCard);
    applyShadow(guideCard);

    ui->BtnStartCharging->setText(tr("开始充电"));
    ui->BtnStartCharging->setMinimumHeight(52);
    ui->BtnStartCharging->setMaximumHeight(52);
    ui->BtnStartCharging->setEnabled(false);
    ui->BtnStartCharging->show();
    pageLayout->addWidget(ui->BtnStartCharging);

    ui->labelOrderStatus->setText(tr("请选择充电桩后开始充电"));
    ui->labelOrderStatus->setStyleSheet(QStringLiteral(
        "color:#7b8090;font-size:11px;background:transparent;"));
    ui->labelOrderStatus->setMinimumHeight(18);
    ui->labelOrderStatus->setMaximumHeight(20);
    ui->labelOrderStatus->show();
    pageLayout->addWidget(ui->labelOrderStatus);

    connect(m_chargeStationSelectorButton, &QPushButton::clicked,
            this, &MainWindow::showChargeStationSelector);
    connect(m_chargePileSelectorButton, &QPushButton::clicked,
            this, &MainWindow::showChargePileSelector);
}

bool MainWindow::isPileAvailable(const QJsonObject &pile) const
{
    return pile.value(QStringLiteral("status")).toString() == QStringLiteral("闲置");
}

QString MainWindow::pileStatusText(const QString &status) const
{
    if (status == QStringLiteral("闲置")) {
        return tr("空闲");
    }
    if (status == QStringLiteral("在用") || status == QStringLiteral("充电中")) {
        return tr("使用中");
    }
    if (status == QStringLiteral("故障")) {
        return tr("故障");
    }
    return status.isEmpty() ? tr("不可用") : status;
}

void MainWindow::updateChargeStationCard(const QJsonObject &station)
{
    if (station.isEmpty()) {
        m_chargeStationNameLabel->setText(tr("请选择充电站"));
        m_chargeStationMetaLabel->setText(tr("查看附近可用站点"));
        m_chargeStationAvailabilityLabel->setText(tr("暂无可用信息"));
        return;
    }

    const int stationId = station.value(QStringLiteral("stationId")).toInt();
    QString address = station.value(QStringLiteral("address")).toString(
        station.value(QStringLiteral("stationAddress")).toString());
    QString distance;
    for (const QJsonValue &value : m_lastStations) {
        const QJsonObject cached = value.toObject();
        if (cached.value(QStringLiteral("stationId")).toInt() != stationId) {
            continue;
        }
        if (address.isEmpty()) {
            address = cached.value(QStringLiteral("address")).toString();
        }
        if (cached.contains(QStringLiteral("distanceKm"))) {
            distance = tr("%1 km")
                           .arg(cached.value(QStringLiteral("distanceKm")).toDouble(), 0, 'f', 1);
        }
        break;
    }

    const QString stationName = station.value(QStringLiteral("name")).toString(
        station.value(QStringLiteral("stationName")).toString());
    m_chargeStationNameLabel->setText(stationName.isEmpty() ? tr("未命名充电站") : stationName);
    m_chargeStationMetaLabel->setText(
        distance.isEmpty() ? address : tr("%1 · %2").arg(distance, address));
    m_chargeStationAvailabilityLabel->setText(
        tr("●  %1 个空闲充电桩")
            .arg(station.value(QStringLiteral("freePileCount")).toInt()));
}

void MainWindow::updateChargePileCard()
{
    QJsonObject selectedPile;
    if (ui->comboPile->currentIndex() >= 0) {
        const int selectedId = ui->comboPile->currentData().toInt();
        for (const QJsonValue &value : m_chargePiles) {
            const QJsonObject pile = value.toObject();
            if (pile.value(QStringLiteral("pileId")).toInt() == selectedId) {
                selectedPile = pile;
                break;
            }
        }
    }

    const bool available = !selectedPile.isEmpty() && isPileAvailable(selectedPile);
    ui->BtnStartCharging->setEnabled(available);

    if (selectedPile.isEmpty()) {
        m_chargePileCodeLabel->setText(tr("请选择可用电桩"));
        m_chargePileTypeLabel->hide();
        m_chargePilePowerLabel->setText(tr("选择后即可开始充电"));
        m_chargePilePriceLabel->setText(QStringLiteral("—"));
        m_chargePileStatusLabel->setText(tr("未选择"));
        m_chargePileStatusLabel->setStyleSheet(QStringLiteral(
            "background:#eceef4;color:#777c8c;border-radius:9px;padding:2px 7px;"
            "font-size:10px;font-weight:700;"));
        ui->labelOrderStatus->setText(tr("请选择充电桩后开始充电"));
        ui->labelOrderStatus->setStyleSheet(QStringLiteral(
            "color:#7b8090;font-size:11px;background:transparent;"));
        return;
    }

    const QString type = selectedPile.value(QStringLiteral("type")).toString();
    const QString status = selectedPile.value(QStringLiteral("status")).toString();
    const QString code = selectedPile.value(QStringLiteral("code")).toString();
    m_chargePileCodeLabel->setText(code);
    m_chargePileTypeLabel->setText(type);
    m_chargePileTypeLabel->show();
    m_chargePilePowerLabel->setText(
        tr("%1 kW").arg(selectedPile.value(QStringLiteral("power")).toDouble(), 0, 'f', 0));
    m_chargePilePriceLabel->setText(
        tr("¥%1/kWh").arg(m_chargeStationPrice, 0, 'f', 2));
    m_chargePileStatusLabel->setText(pileStatusText(status));

    if (available) {
        m_chargePileStatusLabel->setStyleSheet(QStringLiteral(
            "background:#e2f6ec;color:#0d7a4e;border-radius:9px;padding:2px 7px;"
            "font-size:10px;font-weight:700;"));
        ui->labelOrderStatus->setText(tr("已选择 %1，可以开始充电").arg(code));
        ui->labelOrderStatus->setStyleSheet(QStringLiteral(
            "color:#0d7a4e;font-size:11px;font-weight:600;background:transparent;"));
    } else {
        m_chargePileStatusLabel->setStyleSheet(QStringLiteral(
            "background:#f1ecec;color:#9a6060;border-radius:9px;padding:2px 7px;"
            "font-size:10px;font-weight:700;"));
        ui->labelOrderStatus->setText(tr("该电桩当前不可用，请选择其他电桩"));
        ui->labelOrderStatus->setStyleSheet(QStringLiteral(
            "color:#8a6262;font-size:11px;background:transparent;"));
    }
}

void MainWindow::showChargeStationSelector()
{
    QJsonArray stations = m_lastStations;
    const int currentStationId = m_chargeStation.value(QStringLiteral("stationId")).toInt();
    bool includesCurrent = false;
    for (const QJsonValue &value : stations) {
        if (value.toObject().value(QStringLiteral("stationId")).toInt() == currentStationId) {
            includesCurrent = true;
            break;
        }
    }
    if (!m_chargeStation.isEmpty() && !includesCurrent) {
        stations.prepend(m_chargeStation);
    }

    if (stations.isEmpty()) {
        QMessageBox::information(this, tr("选择充电站"), tr("充电站列表正在加载，请稍后再试"));
        if (connection->isConnected()) {
            connection->sendRequest(QStringLiteral("query_stations"), {});
        }
        return;
    }

    QDialog dialog(this);
    dialog.setModal(true);
    dialog.setWindowTitle(tr("选择充电站"));
    dialog.setObjectName(QStringLiteral("chargeSelectorDialog"));
    dialog.setStyleSheet(QStringLiteral(
        "QDialog#chargeSelectorDialog{background:#f7f7fc;border:1px solid #e4e7f2;"
        "border-radius:20px;}"
        "QScrollArea{background:transparent;border:none;}"
        "QScrollArea>QWidget>QWidget{background:transparent;}"));
    const int dialogWidth = qMax(320, qMin(width() - 24, 406));
    const int dialogHeight = qMin(410, 94 + stations.size() * 74);
    dialog.setFixedSize(dialogWidth, qMax(230, dialogHeight));

    auto *dialogLayout = new QVBoxLayout(&dialog);
    dialogLayout->setContentsMargins(14, 14, 14, 14);
    dialogLayout->setSpacing(10);
    auto *header = new QHBoxLayout;
    auto *title = new QLabel(tr("选择充电站"), &dialog);
    title->setStyleSheet(QStringLiteral(
        "color:#131b2e;font-size:18px;font-weight:800;background:transparent;"));
    auto *close = new QPushButton(QStringLiteral("×"), &dialog);
    close->setFixedSize(32, 32);
    close->setCursor(Qt::PointingHandCursor);
    close->setStyleSheet(QStringLiteral(
        "QPushButton{background:#e9ebf3;color:#626777;border:none;border-radius:16px;"
        "font-size:18px;} QPushButton:hover{background:#dfe2eb;}"));
    connect(close, &QPushButton::clicked, &dialog, &QDialog::reject);
    header->addWidget(title);
    header->addStretch();
    header->addWidget(close);
    dialogLayout->addLayout(header);

    auto *scrollArea = new QScrollArea(&dialog);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *host = new QWidget(scrollArea);
    auto *list = new QVBoxLayout(host);
    list->setContentsMargins(0, 0, 0, 0);
    list->setSpacing(8);

    for (const QJsonValue &value : stations) {
        const QJsonObject station = value.toObject();
        const int stationId = station.value(QStringLiteral("stationId")).toInt();
        auto *row = new QPushButton(host);
        row->setCursor(Qt::PointingHandCursor);
        row->setMinimumHeight(66);
        const bool selected = stationId == currentStationId;
        row->setStyleSheet(selected
            ? QStringLiteral("QPushButton{background:#eef3ff;border:1px solid #2563eb;"
                             "border-radius:14px;text-align:left;padding:8px 12px;}"
                             "QPushButton:hover{background:#e6edff;}")
            : QStringLiteral("QPushButton{background:#ffffff;border:1px solid #e7eaf3;"
                             "border-radius:14px;text-align:left;padding:8px 12px;}"
                             "QPushButton:hover{background:#f1f4ff;border-color:#cbd6ff;}"));
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(12, 7, 10, 7);
        rowLayout->setSpacing(8);
        auto *info = new QVBoxLayout;
        info->setSpacing(2);
        auto *name = new QLabel(station.value(QStringLiteral("name")).toString(
                                    station.value(QStringLiteral("stationName")).toString()), row);
        name->setStyleSheet(QStringLiteral(
            "color:#131b2e;font-size:14px;font-weight:700;background:transparent;"));
        QString meta = station.value(QStringLiteral("address")).toString(
            station.value(QStringLiteral("stationAddress")).toString());
        if (station.contains(QStringLiteral("distanceKm"))) {
            meta = tr("%1 km · %2")
                       .arg(station.value(QStringLiteral("distanceKm")).toDouble(), 0, 'f', 1)
                       .arg(meta);
        }
        auto *details = new QLabel(meta, row);
        details->setStyleSheet(QStringLiteral(
            "color:#777c8c;font-size:10px;background:transparent;"));
        info->addWidget(name);
        info->addWidget(details);
        rowLayout->addLayout(info, 1);
        auto *free = new QLabel(tr("%1 个空闲")
                                    .arg(station.value(QStringLiteral("freePileCount")).toInt()), row);
        free->setStyleSheet(QStringLiteral(
            "background:#e2f6ec;color:#0d7a4e;border-radius:9px;padding:3px 7px;"
            "font-size:10px;font-weight:700;"));
        auto *chevron = new QLabel(QStringLiteral("›"), row);
        chevron->setStyleSheet(QStringLiteral(
            "color:#9aa0b5;font-size:20px;background:transparent;"));
        rowLayout->addWidget(free);
        rowLayout->addWidget(chevron);
        for (QLabel *label : {name, details, free, chevron}) {
            label->setAttribute(Qt::WA_TransparentForMouseEvents);
        }
        connect(row, &QPushButton::clicked, this, [this, stationId, &dialog]() {
            ui->spinOrderStationId->setValue(stationId);
            {
                const QSignalBlocker blocker(ui->comboPile);
                ui->comboPile->clear();
                ui->comboPile->setCurrentIndex(-1);
            }
            m_chargePiles = QJsonArray();
            updateChargePileCard();
            connection->sendRequest(QStringLiteral("query_station_detail"), {
                {QStringLiteral("stationId"), stationId}
            });
            dialog.accept();
        });
        list->addWidget(row);
    }
    list->addStretch();
    scrollArea->setWidget(host);
    dialogLayout->addWidget(scrollArea, 1);

    const QPoint position = mapToGlobal(
        QPoint((width() - dialog.width()) / 2, height() - dialog.height() - 14));
    dialog.move(position);
    dialog.exec();
}

void MainWindow::showChargePileSelector()
{
    if (m_chargePiles.isEmpty()) {
        QMessageBox::information(this, tr("选择充电桩"), tr("当前站点暂无充电桩信息"));
        return;
    }

    QDialog dialog(this);
    dialog.setModal(true);
    dialog.setWindowTitle(tr("选择充电桩"));
    dialog.setObjectName(QStringLiteral("chargeSelectorDialog"));
    dialog.setStyleSheet(QStringLiteral(
        "QDialog#chargeSelectorDialog{background:#f7f7fc;border:1px solid #e4e7f2;"
        "border-radius:20px;}"
        "QScrollArea{background:transparent;border:none;}"
        "QScrollArea>QWidget>QWidget{background:transparent;}"));
    const int dialogWidth = qMax(320, qMin(width() - 24, 406));
    const int dialogHeight = qMin(430, 94 + m_chargePiles.size() * 70);
    dialog.setFixedSize(dialogWidth, qMax(235, dialogHeight));

    auto *dialogLayout = new QVBoxLayout(&dialog);
    dialogLayout->setContentsMargins(14, 14, 14, 14);
    dialogLayout->setSpacing(10);
    auto *header = new QHBoxLayout;
    auto *title = new QLabel(tr("选择充电桩"), &dialog);
    title->setStyleSheet(QStringLiteral(
        "color:#131b2e;font-size:18px;font-weight:800;background:transparent;"));
    auto *hint = new QLabel(tr("仅空闲电桩可选"), &dialog);
    hint->setStyleSheet(QStringLiteral(
        "color:#7b8090;font-size:10px;background:transparent;"));
    auto *close = new QPushButton(QStringLiteral("×"), &dialog);
    close->setFixedSize(32, 32);
    close->setCursor(Qt::PointingHandCursor);
    close->setStyleSheet(QStringLiteral(
        "QPushButton{background:#e9ebf3;color:#626777;border:none;border-radius:16px;"
        "font-size:18px;} QPushButton:hover{background:#dfe2eb;}"));
    connect(close, &QPushButton::clicked, &dialog, &QDialog::reject);
    header->addWidget(title);
    header->addWidget(hint);
    header->addStretch();
    header->addWidget(close);
    dialogLayout->addLayout(header);

    auto *scrollArea = new QScrollArea(&dialog);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *host = new QWidget(scrollArea);
    auto *list = new QVBoxLayout(host);
    list->setContentsMargins(0, 0, 0, 0);
    list->setSpacing(8);
    const int selectedPileId = ui->comboPile->currentIndex() >= 0
        ? ui->comboPile->currentData().toInt() : -1;

    for (const QJsonValue &value : m_chargePiles) {
        const QJsonObject pile = value.toObject();
        const int pileId = pile.value(QStringLiteral("pileId")).toInt();
        const bool available = isPileAvailable(pile);
        const bool selected = pileId == selectedPileId;
        auto *row = new QPushButton(host);
        row->setMinimumHeight(62);
        row->setCursor(available ? Qt::PointingHandCursor : Qt::ForbiddenCursor);
        row->setEnabled(available);
        if (!available) {
            row->setStyleSheet(QStringLiteral(
                "QPushButton{background:#f0f1f5;border:1px solid #e5e6eb;"
                "border-radius:14px;text-align:left;padding:7px 10px;}"));
        } else if (selected) {
            row->setStyleSheet(QStringLiteral(
                "QPushButton{background:#eef3ff;border:1px solid #2563eb;"
                "border-radius:14px;text-align:left;padding:7px 10px;}"
                "QPushButton:hover{background:#e6edff;}"));
        } else {
            row->setStyleSheet(QStringLiteral(
                "QPushButton{background:#ffffff;border:1px solid #e7eaf3;"
                "border-radius:14px;text-align:left;padding:7px 10px;}"
                "QPushButton:hover{background:#f1f4ff;border-color:#cbd6ff;}"));
        }

        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(10, 6, 8, 6);
        rowLayout->setSpacing(7);
        auto *identity = new QVBoxLayout;
        identity->setSpacing(1);
        auto *code = new QLabel(pile.value(QStringLiteral("code")).toString(), row);
        code->setMinimumWidth(52);
        code->setStyleSheet(available
            ? QStringLiteral("color:#131b2e;font-size:15px;font-weight:800;background:transparent;")
            : QStringLiteral("color:#9296a3;font-size:15px;font-weight:800;background:transparent;"));
        auto *type = new QLabel(pile.value(QStringLiteral("type")).toString(), row);
        type->setStyleSheet(available
            ? QStringLiteral("background:#e8f0ff;color:#1554d1;border-radius:9px;padding:3px 7px;"
                             "font-size:10px;font-weight:700;")
            : QStringLiteral("background:#e2e3e8;color:#9296a3;border-radius:9px;padding:3px 7px;"
                             "font-size:10px;font-weight:700;"));
        auto *power = new QLabel(
            tr("%1 kW").arg(pile.value(QStringLiteral("power")).toDouble(), 0, 'f', 0), row);
        power->setStyleSheet(available
            ? QStringLiteral("color:#686d7c;font-size:11px;background:transparent;")
            : QStringLiteral("color:#a0a3ad;font-size:11px;background:transparent;"));
        identity->addWidget(code);
        identity->addWidget(power);
        auto *price = new QLabel(tr("¥%1/kWh").arg(m_chargeStationPrice, 0, 'f', 2), row);
        price->setStyleSheet(available
            ? QStringLiteral("color:#2563eb;font-size:12px;font-weight:800;background:transparent;")
            : QStringLiteral("color:#a0a3ad;font-size:12px;font-weight:700;background:transparent;"));
        auto *status = new QLabel(pileStatusText(
                                      pile.value(QStringLiteral("status")).toString()), row);
        status->setStyleSheet(available
            ? QStringLiteral("background:#e2f6ec;color:#0d7a4e;border-radius:9px;padding:3px 7px;"
                             "font-size:10px;font-weight:700;")
            : QStringLiteral("background:#e5e5e8;color:#8b6b6b;border-radius:9px;padding:3px 7px;"
                             "font-size:10px;font-weight:700;"));
        auto *priceAndStatus = new QVBoxLayout;
        priceAndStatus->setSpacing(2);
        priceAndStatus->addWidget(price, 0, Qt::AlignRight);
        priceAndStatus->addWidget(status, 0, Qt::AlignRight);
        auto *endMark = new QLabel(available ? QStringLiteral("›") : QStringLiteral("🔒"), row);
        endMark->setStyleSheet(QStringLiteral(
            "color:#9aa0b5;font-size:17px;background:transparent;"));
        rowLayout->addLayout(identity);
        rowLayout->addWidget(type);
        rowLayout->addStretch();
        rowLayout->addLayout(priceAndStatus);
        rowLayout->addWidget(endMark);
        for (QLabel *label : {code, type, power, price, status, endMark}) {
            label->setAttribute(Qt::WA_TransparentForMouseEvents);
        }
        if (available) {
            connect(row, &QPushButton::clicked, this, [this, pileId, &dialog]() {
                selectPileInCombo(pileId);
                dialog.accept();
            });
        }
        list->addWidget(row);
    }
    list->addStretch();
    scrollArea->setWidget(host);
    dialogLayout->addWidget(scrollArea, 1);

    const QPoint position = mapToGlobal(
        QPoint((width() - dialog.width()) / 2, height() - dialog.height() - 14));
    dialog.move(position);
    dialog.exec();
}

void MainWindow::on_BtnHome_clicked()
{
    ui->stackedWidget->setCurrentWidget(ui->pageHome);
    if (userId > 0 && connection->isConnected()) {
        connection->sendRequest(QStringLiteral("query_stations"), {});
    }
}

void MainWindow::on_BtnCharge_clicked()
{
    openChargePage();
}

void MainWindow::openChargePage()
{
    if (m_chargePagePending) {
        return;
    }
    if (userId <= 0 || !connection->isConnected()) {
        ui->stackedWidget->setCurrentWidget(ui->pageCharge);
        if (connection->isConnected()) {
            on_BtnLoadOrderStation_clicked();
        }
        return;
    }

    // 以服务器返回的最新结果为准判断是否还有未完成的充电订单，
    // 避免客户端缓存状态过期导致已结算后仍提示"请先结算"。
    m_chargePagePending = true;
    m_pendingAction = QStringLiteral("query_user_ongoing_order");
    connection->sendRequest(QStringLiteral("query_user_ongoing_order"), {
        {"userId", userId}
    });
}

void MainWindow::on_BtnStationBack_clicked()
{
    ui->stackedWidget->setGeometry(0, 0, 360, 540);
    ui->widgetNavigation->show();
    ui->stackedWidget->setCurrentWidget(ui->pageHome);
    ui->BtnHome->setChecked(true);
    if (userId > 0 && connection->isConnected()) {
        connection->sendRequest(QStringLiteral("query_stations"), {});
    }
}

void MainWindow::on_BtnStartChargeHere_clicked()
{
    openChargePage();
}

void MainWindow::on_BtnNavigateHere_clicked()
{
    if (m_lastStationId <= 0 || m_lastStationName.isEmpty()) {
        QMessageBox::warning(this, tr("导航"), tr("当前充电站信息不完整，请重新打开详情页"));
        return;
    }

    m_navigationStarted = false;
    m_hasNavigationOrigin = false;
    m_navigationPage->setDestination(
        m_lastStationId, m_lastStationName, m_lastStationAddress,
        m_lastStationLat, m_lastStationLng);

    const QString searchedAddress = ui->editAddress->text().trimmed();
    m_navigationPage->setOriginHint(searchedAddress);
    showNavigationPage();

    if (searchedAddress.isEmpty()) {
        m_navigationPage->requestCurrentLocation();
    } else {
        requestNavigation(m_navigationPage->mode(), searchedAddress);
    }
}

void MainWindow::showNavigationPage()
{
    ui->widgetNavigation->hide();
    ui->stackedWidget->setGeometry(0, 0, 360, 612);
    ui->stackedWidget->setCurrentWidget(m_navigationPage);
}

void MainWindow::leaveNavigationPage()
{
    m_navigationStarted = false;
    m_hasNavigationOrigin = false;
    m_navigationPage->resetNavigation();
    ui->stackedWidget->setGeometry(0, 0, 360, 540);
    ui->stackedWidget->setCurrentWidget(ui->pageStationDetail);
    ui->widgetNavigation->show();
    ui->BtnHome->setChecked(true);
}

void MainWindow::requestNavigation(const QString &mode, const QString &originAddress)
{
    if (!connection->isConnected()) {
        m_navigationPage->showError(tr("尚未连接服务器，无法规划路线"));
        return;
    }

    QJsonObject params{
        {QStringLiteral("stationId"), m_lastStationId},
        {QStringLiteral("mode"), mode}
    };
    if (!originAddress.trimmed().isEmpty()) {
        params[QStringLiteral("originAddress")] = originAddress.trimmed();
    } else if (m_hasNavigationOrigin) {
        params[QStringLiteral("originLatitude")] = m_navigationOriginLat;
        params[QStringLiteral("originLongitude")] = m_navigationOriginLng;
    } else {
        m_navigationPage->showError(tr("请允许获取当前位置，或手动输入详细起点"));
        return;
    }

    m_navigationPage->setBusy(true);
    connection->sendRequest(QStringLiteral("start_navigation"), params);
}

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

QPixmap circularPixmap(const QPixmap &source,int size){
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

void MainWindow::on_BtnStartCharging_clicked()
{
    if (userId < 0) {
        QMessageBox::warning(this, tr("提示"), tr("请先登录"));
        return;
    }
    if (ui->comboPile->currentIndex() < 0) {
        QMessageBox::warning(this, tr("提示"), tr("请先选择充电站和电桩"));
        return;
    }

    connection->sendRequest(QStringLiteral("start_charging"), {
        {"userId", userId},
        {"pileId", ui->comboPile->currentData().toInt()}
    });
}

void MainWindow::on_BtnPileDetail_clicked()
{
    if (ui->comboPile->currentIndex() < 0) {
        return;
    }
    connection->sendRequest(QStringLiteral("query_pile_detail"), {
        {"pileId", ui->comboPile->currentData().toInt()}
    });
}

void MainWindow::on_BtnLoadOrderStation_clicked()
{
    connection->sendRequest(QStringLiteral("query_station_detail"), {
        {"stationId", ui->spinOrderStationId->value()}
    });
}

void MainWindow::on_BtnRefreshOrder_clicked()
{
    if (activeOrderId < 0) {
        ui->labelMonStatus->setText(tr("当前没有进行中的充电订单"));
        return;
    }

    ui->labelMonStatus->setText(tr("正在刷新实时数据…"));
    connection->sendRequest(QStringLiteral("query_order"), {
        {"orderId", activeOrderId}
    });
}

void MainWindow::on_BtnSettleOrder_clicked()
{
    if (activeOrderId < 0) {
        QMessageBox::warning(this, tr("提示"), tr("当前没有进行中的订单"));
        return;
    }

    orderTimer.stop();
    displayTimer.stop();
    m_settlingOrderId = activeOrderId;
    m_pendingAction = QStringLiteral("prepare_settlement");
    connection->sendRequest(QStringLiteral("prepare_settlement"), {
        {"orderId", activeOrderId},
        {"amount", currentAmount},
        {"fee", currentFee}
    });
}

void MainWindow::showPaymentPreview()
{
    const double balance = m_currentUser.value("balance").toDouble();
    PaymentPreview preview(activeOrderId, currentAmount, currentFee, balance, this);
    m_paymentPreview = &preview;

    connect(&preview, &PaymentPreview::paymentConfirmed, this, [this]() {
        m_balanceBeforeSettlement = m_currentUser.value("balance").toDouble();
        const double pendingBalance = qMax(0.0, m_balanceBeforeSettlement - currentFee);
        m_currentUser["balance"] = pendingBalance;
        updateBalanceLabels(pendingBalance);

        m_pendingAction = QStringLiteral("settle_order");
        connection->sendRequest(QStringLiteral("settle_order"), {
            {"orderId", activeOrderId},
            {"amount", currentAmount},
            {"fee", currentFee}
        });
    });
    connect(&preview, &PaymentPreview::rechargeRequested,
            this, &MainWindow::on_BtnRecharge_clicked);
    connect(&preview, &QDialog::accepted, this, [this]() {
        ui->stackedWidget->setCurrentWidget(ui->pageCharge);
        ui->BtnCharge->setChecked(true);
        ui->BtnHome->setChecked(false);
        ui->BtnMine->setChecked(false);
    });
    connect(&preview, &QDialog::finished, this, [this]() {
        m_paymentPreview = nullptr;
    });

    preview.exec();
    m_paymentPreview = nullptr;
}

void MainWindow::on_BtnSearchStations_clicked()
{
    const QString address = ui->editAddress->text().trimmed();
    if (address.isEmpty()) {
        QMessageBox::warning(this, tr("地址搜索"), tr("请输入地址或区域"));
        return;
    }

    connection->sendRequest(QStringLiteral("query_stations"), {
        {"address", address}
    });
}

void MainWindow::onServerResponse(const QJsonObject &response)
{
    const QString action = response.value(QStringLiteral("_requestAction"))
                               .toString(m_pendingAction);
    m_pendingAction.clear();

    ui->Btnlogin->setEnabled(true);
    ui->BtnConfirm_PageEdit->setEnabled(true);
    ui->BtnConfirm_pageRecharge->setEnabled(true);

    const int code = response.value("code").toInt(-1);
    QString message = response.value("msg").toString();

    const QJsonObject data = response.value("data").toObject();

    if (code != 0) {
        if (action == QStringLiteral("start_navigation")
            || action == QStringLiteral("switch_navigation_mode")
            || action == QStringLiteral("end_navigation")) {
            m_navigationPage->showError(response.value("msg").toString());
            if (action == QStringLiteral("end_navigation")) {
                leaveNavigationPage();
            }
            return;
        }
        if (action == QStringLiteral("prepare_settlement")) {
            m_settlingOrderId = -1;
            QMessageBox::warning(this, tr("结算失败"), response.value("msg").toString());
            return;
        }
        if (action == QStringLiteral("settle_order")) {
            if (m_balanceBeforeSettlement >= 0.0) {
                m_currentUser["balance"] = m_balanceBeforeSettlement;
                updateBalanceLabels(m_balanceBeforeSettlement);
                m_balanceBeforeSettlement = -1.0;
            }
            if (m_paymentPreview) {
                m_paymentPreview->reject();
            }
        }
        if (action == QStringLiteral("query_user_ongoing_order")) {
            m_chargePagePending = false;
            ui->stackedWidget->setCurrentWidget(ui->pageCharge);
            ui->BtnCharge->setChecked(true);
            ui->BtnHome->setChecked(false);
            ui->BtnMine->setChecked(false);
            if (connection->isConnected()) {
                on_BtnLoadOrderStation_clicked();
            }
            m_pendingPileId = -1;
        }
        QMessageBox::warning(this, tr("请求失败"), response.value("msg").toString());
        return;
    }

    if (action == QStringLiteral("start_navigation")
        || action == QStringLiteral("switch_navigation_mode")) {
        if (ui->stackedWidget->currentWidget() != m_navigationPage) {
            m_navigationStarted = false;
            if (action == QStringLiteral("start_navigation") && connection->isConnected()) {
                connection->sendRequest(QStringLiteral("end_navigation"), {});
            }
            return;
        }
        m_navigationStarted = true;
        m_navigationPage->showRoute(data);
        return;
    }

    if (action == QStringLiteral("end_navigation")) {
        m_navigationStarted = false;
        return;
    }

    if (action == QStringLiteral("settle_order")) {
        if (!data.contains("balance")) {
            if (m_balanceBeforeSettlement >= 0.0) {
                m_currentUser["balance"] = m_balanceBeforeSettlement;
                updateBalanceLabels(m_balanceBeforeSettlement);
                m_balanceBeforeSettlement = -1.0;
            }
            if (m_paymentPreview) {
                m_paymentPreview->reject();
            }
            QMessageBox::warning(this, tr("结算失败"), tr("服务器未返回最新余额"));
            return;
        }

        const double newBalance = data.value("balance").toDouble();
        m_currentUser["balance"] = newBalance;
        updateBalanceLabels(newBalance);
        m_balanceBeforeSettlement = -1.0;

        ui->labelMonStatus->setText(tr("订单已结算，余额已更新"));
        ui->labelElapsedTime->setText(QStringLiteral("00:00:00"));
        activeOrderStartTime = QDateTime();
        activeOrderId = -1;
        currentAmount = 0;
        currentFee = 0;
        orderTimer.stop();
        displayTimer.stop();

        ui->stackedWidget->setCurrentWidget(ui->pageCharge);
        ui->BtnCharge->setChecked(true);
        ui->BtnHome->setChecked(false);
        ui->BtnMine->setChecked(false);
        return;
    }

    if (action == QStringLiteral("query_user_ongoing_order")) {
        m_chargePagePending = false;
        const QString status = data.value("status").toString();
        if (data.value("hasOngoing").toBool()
            && (status == QStringLiteral("充电中")
                || status == QStringLiteral("待结算"))) {
            const int orderId = data.value("orderId").toInt(-1);
            if (orderId > 0) {
                activeOrderId = orderId;
                currentAmount = data.value("estimatedAmount").toDouble(
                    data.value("amount").toDouble(currentAmount));
                currentFee = data.value("estimatedFee").toDouble(
                    data.value("fee").toDouble(currentFee));
                m_lastOrder = data;
                m_pendingPileId = -1;

                // 订单仍在充电中：只恢复充电监控页，绝不自动结算或弹结算窗。
                if (status == QStringLiteral("充电中")) {
                    const int socPercent = data.value("socPercent").toInt(5);
                    const int remainingSeconds = data.value("remainingSeconds").toInt(60);
                    ui->labelMonStatus->setText(tr("正在充电 · 数据每 1 秒刷新"));
                    ui->labelMonStation->setText(
                        m_lastStationName.isEmpty()
                            ? QStringLiteral("睿光充电站") : m_lastStationName);
                    ui->labelMonOrderId->setText(
                        tr("订单号：#%1").arg(activeOrderId));
                    ui->labelMonStart->setText(
                        tr("开始时间：%1")
                            .arg(data.value("startTime").toString()));
                    activeOrderStartTime = QDateTime::fromString(
                        data.value("startTime").toString(),
                        QStringLiteral("yyyy-MM-dd HH:mm:ss"));
                    ui->labelMonKwh->setText(
                        QStringLiteral("%1 kWh")
                            .arg(data.value("estimatedAmount").toDouble(
                                currentAmount), 0, 'f', 2));
                    ui->labelMonFee->setText(
                        QStringLiteral("¥%1").arg(data.value("estimatedFee")
                            .toDouble(currentFee), 0, 'f', 2));
                    ui->labelMonSoc->setText(QStringLiteral("%1%").arg(socPercent));
                    ui->ringCharge->setValue(socPercent);
                    ui->labelMonEta->setText(formatRemainingChargeTime(remainingSeconds));
                    ui->stackedWidget->setCurrentWidget(ui->pageChargeMonitor);
                    ui->BtnCharge->setChecked(true);
                    ui->BtnHome->setChecked(false);
                    ui->BtnMine->setChecked(false);
                    displayTimer.start();
                    orderTimer.start();
                    return;
                }

                // 订单已是待结算状态：进入结算流程。
                QMessageBox::information(
                    this, tr("提示"), tr("您有未完成的充电订单，请先结算"));
                ui->stackedWidget->setCurrentWidget(ui->pageChargeMonitor);
                ui->BtnCharge->setChecked(true);
                ui->BtnHome->setChecked(false);
                ui->BtnMine->setChecked(false);

                m_pendingAction = QStringLiteral("prepare_settlement");
                connection->sendRequest(QStringLiteral("prepare_settlement"), {
                    {"orderId", activeOrderId},
                    {"amount", currentAmount},
                    {"fee", currentFee}
                });
                orderTimer.stop();
                displayTimer.stop();
                m_settlingOrderId = activeOrderId;
                return;
            }
        }
        ui->stackedWidget->setCurrentWidget(ui->pageCharge);
        ui->BtnCharge->setChecked(true);
        ui->BtnHome->setChecked(false);
        ui->BtnMine->setChecked(false);
        if (connection->isConnected()) {
            on_BtnLoadOrderStation_clicked();
        }
        return;
    }

    if (data.contains("stations")) {
        const QJsonArray stations = data.value("stations").toArray();
        m_lastStations = stations;
        ui->labelNearbyCount->setText(tr("共 %1 个电站").arg(stations.size()));
        rebuildNearbyCards(stations);
        if (!m_chargeStation.isEmpty()) {
            updateChargeStationCard(m_chargeStation);
        }
        return;
    }

    if (data.contains("piles") && data.contains("stationId")) {
        m_lastStationId = data.value("stationId").toInt(-1);
        if (data.contains("latitude")) {
            m_lastStationLat = data.value("latitude").toDouble();
        }
        if (data.contains("longitude")) {
            m_lastStationLng = data.value("longitude").toDouble();
        }
        m_lastStationName = data.value("name").toString(
            data.value("stationName").toString());
        m_lastStationAddress = data.value("address").toString(
            data.value("stationAddress").toString());
        populateStationDetail(data);

        QStringList orderDetails;
        m_chargeStation = data;
        m_chargePiles = data.value(QStringLiteral("piles")).toArray();
        m_chargeStationPrice = data.value(QStringLiteral("price")).toDouble();
        if (!m_chargeStation.contains(QStringLiteral("freePileCount"))) {
            int freeCount = 0;
            for (const QJsonValue &value : m_chargePiles) {
                if (isPileAvailable(value.toObject())) {
                    ++freeCount;
                }
            }
            m_chargeStation[QStringLiteral("freePileCount")] = freeCount;
        }
        updateChargeStationCard(m_chargeStation);

        const int requestedPileId = m_pendingPileId > 0
            ? m_pendingPileId : data.value(QStringLiteral("pileId")).toInt(-1);
        const QString stationName = data.value("name").toString(
            data.value("stationName").toString());
        orderDetails << tr("站点：%1\n地址：%2\n价格：%3 元/度")
                            .arg(stationName)
                            .arg(data.value("address").toString(
                                data.value("stationAddress").toString()))
                            .arg(data.value("price").toDouble(), 0, 'f', 2);
        orderDetails << QStringLiteral("电桩：");
        {
            const QSignalBlocker blocker(ui->comboPile);
            ui->comboPile->clear();
            ui->comboPile->setCurrentIndex(-1);
            for (const QJsonValue &value : data.value("piles").toArray()) {
                const QJsonObject pile = value.toObject();
                orderDetails << tr("%1(%2) %3")
                                    .arg(pile.value("code").toString())
                                    .arg(pile.value("type").toString())
                                    .arg(pile.value("status").toString());

                const QString pileDescription = tr("%1 | %2 | %3 kW | %4")
                    .arg(pile.value("code").toString())
                    .arg(pile.value("type").toString())
                    .arg(pile.value("power").toDouble(), 0, 'f', 1)
                    .arg(pile.value("status").toString());
                ui->comboPile->addItem(pileDescription, pile.value("pileId"));
            }
            ui->comboPile->setCurrentIndex(-1);
            if (requestedPileId > 0) {
                for (int index = 0; index < m_chargePiles.size(); ++index) {
                    const QJsonObject pile = m_chargePiles.at(index).toObject();
                    if (pile.value(QStringLiteral("pileId")).toInt() == requestedPileId
                        && isPileAvailable(pile)) {
                        selectPileInCombo(requestedPileId);
                        break;
                    }
                }
            }
        }
        m_pendingPileId = -1;
        updateChargePileCard();
        ui->labelOrderStationDetails->setPlainText(orderDetails.join(QStringLiteral("\n")));
        if (data.contains("pileId") && data.value("pileId").toInt() > 0) {
            ui->labelPileStation->setText(
                tr("当前选择：%1（站点ID：%2，电桩ID：%3）")
                    .arg(stationName)
                    .arg(data.value("stationId").toInt())
                    .arg(data.value("pileId").toInt()));
        } else {
            ui->labelPileStation->setText(
                tr("已选择站点：%1（站点ID：%2）")
                    .arg(stationName)
                    .arg(data.value("stationId").toInt()));
        }
        if (m_showStationDetailPage) {
            m_showStationDetailPage = false;
            ui->stackedWidget->setCurrentWidget(ui->pageStationDetail);
            ui->BtnHome->setChecked(true);
        }
        return;
    }

    if(action == "update_user_profile" || action == "login"){

        if( !data.contains("userId") || !data.contains("phone") ||
            !data.contains("nickname")){
            QMessageBox::warning(
                this,
                tr("提示"),
                tr("服务器返回的信息不完整")
                );
            return;
        }

        // 保存最新用户的完整信息
        m_currentUser = data;
        userId = data.value("userId").toInt(-1);
        phoneNumber = data.value("phone").toString();

        QString nickname = data.value("nickname").toString();
        QString phone = data.value("phone").toString();
        QString avatarPath = data.value("avatarPath").toString();
        double balance = data.value("balance").toDouble();

        // 将服务器返回的信息显示到“我的”页面
        ui->labelNickname->setText(nickname);

        ui->labelPhone->setText(phone);

        ui->labelMoney_c->setText(
            QStringLiteral("¥ %1").arg(balance, 0, 'f', 2));
        updateBalanceLabels(balance);

        QPixmap avatar;

        // 有自定义头像时先尝试加载
        if (!avatarPath.isEmpty()) {
            avatar.load(avatarPath);
        }

        // 没有自定义头像，或自定义头像加载失败时，显示默认头像
        if (avatar.isNull()) {
            avatar.load(":/images/default_avatar.jpeg");
        }

        if (!avatar.isNull()) {
            ui->labelPhoto->setFixedSize(80,80);
            ui->labelPhoto->setAlignment(Qt::AlignCenter);
            ui->labelPhoto->setPixmap(circularPixmap(avatar,80));
        } else {
            qWarning() << "默认头像加载失败";
        }

        // 登录成功后才进入主界面
        if(action == "login"){
            const bool backgroundRefresh = m_backgroundBalanceRefresh;
            m_backgroundBalanceRefresh = false;

            const int ongoingOrderId = data.value("ongoingOrderId").toInt(-1);
            if (ongoingOrderId > 0) {
                activeOrderId = ongoingOrderId;
                ui->stackedWidget->setCurrentWidget(ui->pageChargeMonitor);
                ui->labelMonStatus->setText(
                    tr("正在恢复订单 %1，请稍候...").arg(activeOrderId));
                connection->sendRequest(QStringLiteral("query_order"), {
                    {"orderId", activeOrderId}
                });
            } else {
                activeOrderId = -1;
                currentAmount = 0;
                currentFee = 0;
                activeOrderStartTime = QDateTime();
                orderTimer.stop();
                displayTimer.stop();
                // 后台余额刷新不抢走用户当前页面，仅更新钱包数值。
                if (!backgroundRefresh) {
                    ui->stackedWidget->setCurrentWidget(ui->pageHome);
                    connection->sendRequest(QStringLiteral("query_stations"), {});
                }
            }
            ui->widgetNavigation->show();
            ui->label_title_mine_6->setText(
                tr("欢迎%1").arg(data.value("nickname").toString()));

            if (!backgroundRefresh) {
                QMessageBox::information(
                    this,
                    tr("登陆成功"),
                    tr("欢迎,%1").arg(data.value("nickname").toString())
                    );
            }
            return;
        }

        if(action == "update_user_profile"){
            m_selectedAvatarPath = avatarPath;

            ui->widgetNavigation->show();
            ui->stackedWidget->setCurrentWidget(ui->pageMine);

            QMessageBox::information(
                this,
                tr("更改成功"),
                tr("数据库中的用户资料已更新")
                );

            return;
        }
    }

    if(action == "recharge_balance"){
        QJsonObject data = response.value("data").toObject();

        if(!data.contains("userId") || !data.contains("balance")){
            QMessageBox::warning(this,tr("充值失败"),tr("服务器返回信息不完整"));
            return;
        }

        m_currentUser = data;

        double balance = data.value("balance").toDouble();

        ui->labelMoney_c->setText(QStringLiteral("¥ %1").arg(balance, 0, 'f', 2));
        updateBalanceLabels(balance);

        ui->editRecharge->clear();
        ui->widgetNavigation->show();
        ui->stackedWidget->setCurrentWidget(ui->pageMine);

        QMessageBox::information(this,
                                 tr("充值成功"),
                                 QString("当前余额: %1 元").arg(balance,0,'f',2));

        return;
    }


    if (m_settlingOrderId > 0) {
        const int responseOrderId = data.value("orderId").toInt(-1);
        const QString responseStatus = data.value("status").toString();
        const bool genuinePrepare =
            responseOrderId > 0
            && responseStatus == QStringLiteral("待结算")
            && !data.contains("startTime");
        if (genuinePrepare) {
            const int settlingId = m_settlingOrderId;
            m_settlingOrderId = -1;
            activeOrderId = settlingId;
            currentAmount = data.value("amount").toDouble(currentAmount);
            currentFee = data.value("fee").toDouble(currentFee);
            m_lastOrder = data;
            orderTimer.stop();
            displayTimer.stop();
            ui->labelMonStatus->setText(
                tr("充电已完成，正在生成账单…"));
            showPaymentPreview();
            return;
        }
        // 收到的是发起结算前就发出的旧订单状态回包，继续等待真正的
        // prepare_settlement 结果，避免误弹结算窗。
        m_pendingAction = QStringLiteral("prepare_settlement");
        return;
    }

    if (data.contains("status")) {
        const QString status = data.value("status").toString();
        if (data.contains("orderId")) {
            activeOrderId = data.value("orderId").toInt();
            m_lastOrder = data;
        }
        const int durationMinutes = data.value("durationMinutes").toInt();
        const double estAmount = data.value("estimatedAmount").toDouble();
        const double estFee = data.value("estimatedFee").toDouble();
        if (status == QStringLiteral("充电中")) {
            currentAmount = estAmount;
            currentFee = estFee;
            activeOrderStartTime = QDateTime::fromString(
                data.value("startTime").toString(),
                QStringLiteral("yyyy-MM-dd HH:mm:ss"));
            const int socPercent = data.value("socPercent").toInt(
                qBound(5, 8 + durationMinutes, 99));
            const int remainingSeconds = data.value("remainingSeconds").toInt(60);
            ui->labelMonStatus->setText(tr("正在充电 · 数据每 1 秒刷新"));
            ui->labelMonKwh->setText(QStringLiteral("%1 kWh").arg(estAmount, 0, 'f', 2));
            ui->labelMonFee->setText(QStringLiteral("¥%1").arg(estFee, 0, 'f', 2));
            ui->labelMonSoc->setText(QStringLiteral("%1%").arg(socPercent));
            ui->ringCharge->setValue(socPercent);
            ui->labelMonEta->setText(formatRemainingChargeTime(remainingSeconds));
            if (!ui->labelMonOrderId->text().contains('#')) {
                ui->labelMonOrderId->setText(
                    tr("订单号：#%1 · 电桩 #%2")
                        .arg(activeOrderId)
                        .arg(data.value("pileId").toInt()));
            }
            ui->labelMonStart->setText(
                tr("开始时间：%1").arg(data.value("startTime").toString()));
            displayTimer.start();
            orderTimer.start();
            // 仅在用户当前位于充电相关页面时才切回充电监控页，
            // 避免5秒一轮的后台刷新把用户从其他页面强行拽回充电页。
            if (ui->stackedWidget->currentWidget() == ui->pageChargeMonitor
                || ui->stackedWidget->currentWidget() == ui->pageCharge) {
                ui->stackedWidget->setCurrentWidget(ui->pageChargeMonitor);
                ui->BtnCharge->setChecked(true);
                ui->BtnHome->setChecked(false);
                ui->BtnMine->setChecked(false);
            }
        }
        if (status == QStringLiteral("待结算")) {
            currentAmount = data.value("amount").toDouble(estAmount);
            currentFee = data.value("fee").toDouble(estFee);
            orderTimer.stop();
            displayTimer.stop();
            ui->labelMonStatus->setText(tr("充电已完成，等待结算"));
            ui->labelMonSoc->setText(QStringLiteral("100%"));
            ui->ringCharge->setValue(100);
            ui->labelMonEta->setText(tr("已充满，订单已结束"));
        }
        if (status == QStringLiteral("已结算")) {
            activeOrderStartTime = QDateTime();
            activeOrderId = -1;
            currentAmount = 0;
            currentFee = 0;
            ui->labelElapsedTime->setText(QStringLiteral("00:00:00"));
            ui->labelMonKwh->setText(QStringLiteral("0.0 kWh"));
            ui->labelMonFee->setText(QStringLiteral("¥0.00"));
            orderTimer.stop();
            displayTimer.stop();
            m_backgroundBalanceRefresh = true;
            m_pendingAction = QStringLiteral("login");
            connection->sendRequest(QStringLiteral("login"), {{"phone", phoneNumber}});
        }
        return;
    }

    if (data.contains("orderId")) {
        activeOrderId = data.value("orderId").toInt();
        activeOrderStartTime = QDateTime::currentDateTime();
        m_lastOrder = data;
        ui->labelMonStation->setText(
            m_lastStationName.isEmpty() ? QStringLiteral("睿光充电站") : m_lastStationName);
        ui->labelMonStatus->setText(tr("订单创建成功，正在开始充电…"));
        ui->labelMonOrderId->setText(tr("订单号：#%1").arg(activeOrderId));
        ui->labelMonStart->setText(
            tr("开始时间：%1")
                .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))));
        ui->labelMonSoc->setText(QStringLiteral("5%"));
        ui->ringCharge->setValue(5);
        ui->labelMonEta->setText(formatRemainingChargeTime(60));
        ui->labelMonKwh->setText(QStringLiteral("0.0 kWh"));
        ui->labelMonFee->setText(QStringLiteral("¥0.00"));
        ui->labelElapsedTime->setText(QStringLiteral("00:00:00"));
        ui->stackedWidget->setCurrentWidget(ui->pageChargeMonitor);
        ui->BtnCharge->setChecked(true);
        ui->BtnHome->setChecked(false);
        ui->BtnMine->setChecked(false);
        displayTimer.start();
        orderTimer.start();
        on_BtnRefreshOrder_clicked();
        return;
    }

}

// 进入修改用户信息界面
void MainWindow::on_BtnSetting_clicked()
{
    ui->lineEditNickname->setText(m_currentUser.value("nickname").toString());

    m_selectedAvatarPath = m_currentUser.value("avatarPath").toString();
    ui->labelPhotoEdit->setPixmap(ui->labelPhoto->pixmap());

    ui->widgetNavigation->hide();
    ui->stackedWidget->setCurrentWidget(ui->pageEditMine);

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
void MainWindow::onConnectionError(const QString &message){
    ui->Btnlogin->setEnabled(true);
    ui->BtnConfirm_pageRecharge->setEnabled(true);
    ui->BtnConfirm_PageEdit->setEnabled(true);

    m_pendingAction.clear();
    m_chargePagePending = false;
    m_settlingOrderId = -1;
    if (m_balanceBeforeSettlement >= 0.0) {
        m_currentUser["balance"] = m_balanceBeforeSettlement;
        updateBalanceLabels(m_balanceBeforeSettlement);
        m_balanceBeforeSettlement = -1.0;
    }

    if (ui->stackedWidget->currentWidget() == m_navigationPage) {
        m_navigationPage->showError(message);
        return;
    }

    QMessageBox::warning(
        this,
        tr("网络错误"),
        message
        );
}

void MainWindow::applyShadow(QWidget *widget)
{
    auto *shadow = new QGraphicsDropShadowEffect(widget);
    shadow->setBlurRadius(24);
    shadow->setOffset(0, 6);
    shadow->setColor(QColor(19, 27, 46, 22));
    widget->setGraphicsEffect(shadow);
}

void MainWindow::setupNavIcons()
{
    const QSize navIconSize(24, 24);
    ui->BtnHome->setIconSize(navIconSize);
    ui->BtnCharge->setIconSize(navIconSize);
    ui->BtnMine->setIconSize(navIconSize);

    applyNavIcon(ui->BtnHome, ui->BtnHome->isChecked(), QStringLiteral("nav_home"));
    applyNavIcon(ui->BtnCharge, ui->BtnCharge->isChecked(), QStringLiteral("nav_charge"));
    applyNavIcon(ui->BtnMine, ui->BtnMine->isChecked(), QStringLiteral("nav_mine"));

    auto updateIcon = [this](QAbstractButton *button, const QString &base) {
        return [this, button, base](bool checked) {
            applyNavIcon(button, checked, base);
        };
    };

    connect(ui->BtnHome, &QToolButton::toggled,
            updateIcon(ui->BtnHome, QStringLiteral("nav_home")));
    connect(ui->BtnCharge, &QToolButton::toggled,
            updateIcon(ui->BtnCharge, QStringLiteral("nav_charge")));
    connect(ui->BtnMine, &QToolButton::toggled,
            updateIcon(ui->BtnMine, QStringLiteral("nav_mine")));
}

void MainWindow::applyNavIcon(QAbstractButton *button, bool checked, const QString &base)
{
    const QString suffix = checked ? QStringLiteral("_active") : QString();
    button->setIcon(QIcon(QStringLiteral(":/icons/%1%2.svg").arg(base).arg(suffix)));
}

void MainWindow::updateBalanceLabels(double balance)
{
    ui->labelMoney_c->setText(QStringLiteral("¥ %1").arg(balance, 0, 'f', 2));
    ui->labelRechargeBalance->setText(QStringLiteral("¥ %1").arg(balance, 0, 'f', 2));
}

QString MainWindow::formatRemainingChargeTime(int seconds) const
{
    seconds = qMax(0, seconds);
    if (seconds <= 0) {
        return tr("已充满，订单即将结束");
    }
    if (seconds < 60) {
        return tr("预计还需约 %1 秒充满").arg(seconds);
    }
    const int minutes = (seconds + 59) / 60;
    return tr("预计还需约 %1 分钟充满").arg(minutes);
}

int MainWindow::selectPileInCombo(int pileId)
{
    for (int i = 0; i < ui->comboPile->count(); ++i) {
        if (ui->comboPile->itemData(i).toInt() == pileId) {
            ui->comboPile->setCurrentIndex(i);
            return i;
        }
    }
    return -1;
}

void MainWindow::populateStationDetail(const QJsonObject &data)
{
    const QString stationName = data.value("name").toString(
        data.value("stationName").toString());
    const QString address = data.value("address").toString(
        data.value("stationAddress").toString());

    ui->labelSDName->setText(stationName);
    ui->labelSDAddress->setText(address);
    ui->labelSDFree->setText(QStringLiteral("%1/%2")
                                  .arg(data.value("freePileCount").toInt())
                                  .arg(data.value("pileCount").toInt()));
    ui->labelSDOnline->setText(QStringLiteral("%1%")
                                   .arg(data.value("onlineRate").toDouble(), 0, 'f', 0));
    ui->labelSDPrice->setText(QStringLiteral("¥%1/度")
                                  .arg(data.value("price").toDouble(), 0, 'f', 2));

    while (QLayoutItem *child = ui->pileLayout->takeAt(0)) {
        if (QWidget *w = child->widget()) {
            w->deleteLater();
        }
        delete child;
    }

    const QJsonArray piles = data.value("piles").toArray();
    for (const QJsonValue &value : piles) {
        const QJsonObject pile = value.toObject();
        const QString status = pile.value("status").toString();
        const bool idle = (status == QStringLiteral("闲置"));

        auto *card = new QFrame(ui->pileHost);
        card->setObjectName(QStringLiteral("pileCard"));
        auto *row = new QHBoxLayout(card);
        row->setContentsMargins(14, 10, 12, 10);
        row->setSpacing(10);

        auto *info = new QLabel(card);
        info->setTextFormat(Qt::RichText);
        info->setText(
            QStringLiteral("<span style=\"font-size:15px;font-weight:700;color:#131b2e;\">%1</span>"
                           "<span style=\"color:#737686;font-size:12px;\">&nbsp;%2 kW</span>"
                           "<br><span style=\"color:#434655;font-size:12px;\">%3 · 累计使用 %4 次</span>")
                .arg(pile.value("code").toString())
                .arg(pile.value("power").toDouble(), 0, 'f', 1)
                .arg(pile.value("type").toString())
                .arg(pile.value("totalSessions").toInt()));
        row->addWidget(info, 1);

        if (idle) {
            auto *btn = new QPushButton(tr("开始充电"), card);
            btn->setObjectName(QStringLiteral("pileStartBtn"));
            const int pileId = pile.value("pileId").toInt();
            const int stationId = data.value("stationId").toInt();
            connect(btn, &QPushButton::clicked, this, [this, pileId, stationId]() {
                m_pendingPileId = pileId;
                if (stationId > 0) {
                    ui->spinOrderStationId->setValue(stationId);
                }
                openChargePage();
            });
            row->addWidget(btn);
        } else {
            auto *badge = new QLabel(status, card);
            badge->setObjectName(status == QStringLiteral("充电中")
                                     ? QStringLiteral("badgeCharging")
                                     : QStringLiteral("badgeFault"));
            row->addWidget(badge);
        }
        ui->pileLayout->addWidget(card);
    }
    ui->pileLayout->addStretch();
}

void MainWindow::rebuildNearbyCards(const QJsonArray &stations)
{
    while (QLayoutItem *child = ui->nearbyListLayout->takeAt(0)) {
        if (QWidget *w = child->widget()) {
            w->deleteLater();
        }
        delete child;
    }

    if (stations.isEmpty()) {
        auto *empty = new QLabel(tr("没有找到匹配的充电站"), ui->nearbyListHost);
        empty->setAlignment(Qt::AlignCenter);
        empty->setStyleSheet(QStringLiteral("color:#737686;font-size:13px;"));
        ui->nearbyListLayout->addWidget(empty);
        return;
    }

    for (const QJsonValue &value : stations) {
        const QJsonObject station = value.toObject();
        const int stationId = station.value("stationId").toInt();

        auto *card = new QFrame(ui->nearbyListHost);
        card->setObjectName(QStringLiteral("stationCard"));
        auto *layout = new QVBoxLayout(card);
        layout->setContentsMargins(16, 14, 16, 14);
        layout->setSpacing(8);

        // 第一行：电站名 + 可选角标
        auto *titleRow = new QHBoxLayout;
        titleRow->setSpacing(6);
        const QString fullName = station.value("name").toString();
        auto *name = new QLabel(card);
        name->setTextFormat(Qt::RichText);
        QFont nameFont = name->font();
        nameFont.setPointSizeF(nameFont.pointSizeF() + 1.0);
        nameFont.setBold(true);
        name->setFont(nameFont);
        const QString elidedName = QFontMetrics(name->font())
            .elidedText(fullName, Qt::ElideRight, 235);
        name->setText(QStringLiteral("<span style=\"color:#131b2e;\">%1</span>")
                          .arg(elidedName.toHtmlEscaped()));
        titleRow->addWidget(name, 1);
        if (station.contains("badge") && !station.value("badge").toString().isEmpty()) {
            auto *badge = new QLabel(station.value("badge").toString(), card);
            badge->setObjectName(QStringLiteral("badgeStation"));
            titleRow->addWidget(badge, 0, Qt::AlignVCenter);
        }
        layout->addLayout(titleRow);

        // 第二行：距离 · 地址
        QStringList placeParts;
        if (station.contains("distanceKm")) {
            placeParts << tr("%1 km")
                              .arg(station.value("distanceKm").toDouble(), 0, 'f', 1);
        }
        placeParts << station.value("address").toString();
        auto *place = new QLabel(placeParts.join(QStringLiteral(" · ")), card);
        place->setStyleSheet(
            QStringLiteral("color:#8a8f9e;font-size:12px;background:transparent;"));
        layout->addWidget(place);

        // 第三行：信息胶囊（可换行）
        auto *pills = new FlowLayout(nullptr, 0, 6, 6);
        auto *freePill = new QLabel(card);
        freePill->setObjectName(QStringLiteral("pillFree"));
        freePill->setText(tr("● %1/%2 个空闲")
                              .arg(station.value("freePileCount").toInt())
                              .arg(station.value("pileCount").toInt()));
        pills->addWidget(freePill);
        if (station.contains("maxPower")) {
            auto *powerPill = new QLabel(card);
            powerPill->setObjectName(QStringLiteral("pillPower"));
            powerPill->setText(tr("⚡ %1 kW")
                                   .arg(station.value("maxPower").toDouble(), 0, 'f', 0));
            pills->addWidget(powerPill);
        }
        auto *onlinePill = new QLabel(card);
        onlinePill->setObjectName(QStringLiteral("pillOnline"));
        onlinePill->setText(tr("在线率 %1%")
                                .arg(station.value("onlineRate").toDouble(), 0, 'f', 0));
        pills->addWidget(onlinePill);
        layout->addLayout(pills);

        // 第四行：单价（大号蓝色）+ 查看详情
        auto *bottomRow = new QHBoxLayout;
        bottomRow->setSpacing(8);
        auto *price = new QLabel(card);
        price->setTextFormat(Qt::RichText);
        price->setText(
            QStringLiteral("<span style=\"font-size:24px;font-weight:700;color:#2563eb;\">"
                           "¥%1</span>"
                           "<span style=\"font-size:12px;color:#8a8f9e;\">&nbsp;/ kWh</span>")
                .arg(station.value("price").toDouble(), 0, 'f', 2));
        bottomRow->addWidget(price, 1, Qt::AlignVCenter);

        auto *btn = new QPushButton(tr("查看详情"), card);
        btn->setObjectName(QStringLiteral("stationDetailBtn"));
        btn->setCursor(Qt::PointingHandCursor);
        btn->setMinimumHeight(34);
        connect(btn, &QPushButton::clicked, this, [this, stationId]() {
            m_showStationDetailPage = true;
            connection->sendRequest(QStringLiteral("query_station_detail"), {
                {"stationId", stationId}
            });
        });
        bottomRow->addWidget(btn, 0, Qt::AlignVCenter);

        layout->addLayout(bottomRow);

        ui->nearbyListLayout->addWidget(card);
    }
    ui->nearbyListLayout->addStretch();
}
