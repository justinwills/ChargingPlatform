#include "adminwindow.h"
#include "admindesign.h"

#include <QDateTime>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QLegend>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QComboBox>
#include <QStackedWidget>

// Builds Tab 0 (Revenue & Stats) and adds it to contentStack. Must run
// after buildDashboardShell().
void AdminWindow::buildStatsTab()
{
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

    // ── Revenue details ──────────────────────────────────────────────────────
    auto *trendCard = new QFrame;
    trendCard->setObjectName(QStringLiteral("contentCard"));
    applyShadow(trendCard, 16, 4, QColor(19, 27, 46, 12));
    auto *trendCardLayout = new QVBoxLayout(trendCard);
    trendCardLayout->setContentsMargins(24, 20, 24, 24);
    trendCardLayout->setSpacing(12);

    auto *trendTitle = new QLabel(QStringLiteral("营收明细"));
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

    // ── Revenue trend chart (bottom of the statistics page) ─────────────────
    auto *chartCard = new QFrame;
    chartCard->setObjectName(QStringLiteral("contentCard"));
    applyShadow(chartCard, 16, 4, QColor(19, 27, 46, 12));
    auto *chartCardLayout = new QVBoxLayout(chartCard);
    chartCardLayout->setContentsMargins(24, 20, 24, 24);
    chartCardLayout->setSpacing(12);

    auto *chartHeader = new QHBoxLayout;
    auto *chartTitle = new QLabel(QStringLiteral("营收变化趋势"));
    chartTitle->setStyleSheet(QStringLiteral(
        "color:#131b2e; font-size:16px; font-weight:700; background:transparent; border:none;"));
    revenuePeriodCombo->addItem(QStringLiteral("近 7 日"), 7);
    revenuePeriodCombo->addItem(QStringLiteral("近 30 日"), 30);
    revenuePeriodCombo->setMinimumWidth(110);
    revenuePeriodCombo->setCursor(Qt::PointingHandCursor);
    chartHeader->addWidget(chartTitle);
    chartHeader->addStretch();
    chartHeader->addWidget(new QLabel(QStringLiteral("时间维度：")));
    chartHeader->addWidget(revenuePeriodCombo);
    chartCardLayout->addLayout(chartHeader);
    chartCardLayout->addWidget(makeDivider());

    auto *initialChart = new QChart;
    initialChart->setTitle(QStringLiteral("近 7 日营收趋势"));
    initialChart->setBackgroundVisible(false);
    revenueChartView->setChart(initialChart);
    revenueChartView->setRenderHint(QPainter::Antialiasing);
    revenueChartView->setMinimumHeight(330);
    revenueChartView->setStyleSheet(QStringLiteral("background:transparent; border:none;"));
    chartCardLayout->addWidget(revenueChartView);
    statsMainLayout->addWidget(chartCard);
    statsMainLayout->addStretch();

    statsScroll->setWidget(statsInner);
    auto *statsOuterLayout = new QVBoxLayout(statsPage);
    statsOuterLayout->setContentsMargins(0, 0, 0, 0);
    statsOuterLayout->addWidget(statsScroll);
    contentStack->addWidget(statsPage);

}
