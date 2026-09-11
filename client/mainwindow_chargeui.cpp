#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

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

