#include "adminwindow.h"
#include "admindesign.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <QStackedWidget>

// Builds Tab 2 (Stations & Piles) and adds it to contentStack. Must run
// after buildDashboardShell().
void AdminWindow::buildStationsTab()
{
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
    stationPileCountEdit->setRange(0, 200);
    stationPileCountEdit->setValue(2);

    addField(0, 0, QStringLiteral("站点名称"), stationNameEdit);
    addField(0, 1, QStringLiteral("地址"), stationAddressEdit);
    addField(1, 0, QStringLiteral("经度"), stationLongitudeEdit);
    addField(1, 1, QStringLiteral("纬度"), stationLatitudeEdit);
    addField(2, 0, QStringLiteral("充电单价"), stationPriceEdit);
    addField(2, 1, QStringLiteral("初始电桩数量"), stationPileCountEdit);

    addCardLayout->addLayout(formGrid);

    auto *addStationBtn = new QPushButton(QStringLiteral("＋ 增加站点"));
    addStationBtn->setObjectName(QStringLiteral("successBtn"));
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
    stcLayout->setContentsMargins(20, 16, 20, 16);
    stcLayout->setSpacing(10);
    auto *stationTableTitle = new QLabel(QStringLiteral("站点列表"));
    stationTableTitle->setStyleSheet(QStringLiteral(
        "color:#131b2e; font-size:15px; font-weight:700; background:transparent; border:none;"));
    stcLayout->addWidget(stationTableTitle);
    stcLayout->addWidget(makeDivider());

    stationsTable->setColumnCount(7);
    stationsTable->setHorizontalHeaderLabels({
        QStringLiteral("ID"), QStringLiteral("名称"), QStringLiteral("地址"),
        QStringLiteral("价格"), QStringLiteral("空闲/总数"), QStringLiteral("在线率"),
        QStringLiteral("经纬度")
    });
    stationsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    stationsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
    stationsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    stationsTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    stationsTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
    stationsTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Fixed);
    stationsTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Fixed);
    stationsTable->setColumnWidth(0, 70);
    stationsTable->setColumnWidth(1, 170);
    stationsTable->setColumnWidth(3, 100);
    stationsTable->setColumnWidth(4, 110);
    stationsTable->setColumnWidth(5, 100);
    stationsTable->setColumnWidth(6, 170);
    stationsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    stationsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    stationsTable->setAlternatingRowColors(true);
    stationsTable->setShowGrid(false);
    stationsTable->verticalHeader()->setVisible(false);
    stationsTable->verticalHeader()->setDefaultSectionSize(48);
    stationsTable->setMinimumHeight(190);
    stationsTable->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    stationsTable->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    stationsTable->setToolTip(QStringLiteral("点击一行可在下方电桩列表中查看该站点的电桩明细"));
    stcLayout->addWidget(stationsTable);

    auto *stationAdjustRow = new QHBoxLayout;
    stationAdjustRow->setContentsMargins(0, 10, 0, 0);
    stationAdjustRow->setSpacing(12);
    auto *stationAdjustIdLabel = new QLabel(QStringLiteral("站点 ID"));
    stationAdjustIdLabel->setStyleSheet(QStringLiteral(
        "color:#737686; font-size:13px; font-weight:500; background:transparent; border:none;"));
    stationAdjustRow->addWidget(stationAdjustIdLabel);
    stationAdjustIdSpin->setRange(1, 1000000);
    stationAdjustIdSpin->setMinimumHeight(36);
    stationAdjustIdSpin->setFixedWidth(100);
    stationAdjustRow->addWidget(stationAdjustIdSpin);

    auto *stationAdjustCountLabel = new QLabel(QStringLiteral("电桩数量"));
    stationAdjustCountLabel->setStyleSheet(QStringLiteral(
        "color:#737686; font-size:13px; font-weight:500; background:transparent; border:none;"));
    stationAdjustRow->addWidget(stationAdjustCountLabel);
    stationAdjustPileCountSpin->setRange(0, 200);
    stationAdjustPileCountSpin->setMinimumHeight(36);
    stationAdjustPileCountSpin->setFixedWidth(100);
    stationAdjustRow->addWidget(stationAdjustPileCountSpin);

    auto *adjustPileCountBtn = new QPushButton(QStringLiteral("调整电桩数量"));
    adjustPileCountBtn->setObjectName(QStringLiteral("primaryBtn"));
    adjustPileCountBtn->setMinimumHeight(36);
    adjustPileCountBtn->setCursor(Qt::PointingHandCursor);
    stationAdjustRow->addWidget(adjustPileCountBtn);
    stationAdjustRow->addStretch();
    stcLayout->addLayout(stationAdjustRow);
    stationsMainLayout->addWidget(stationTableCard);

    // ── Piles Table Card ─────────────────────────────────────────────────────
    auto *pileTableCard = new QFrame;
    pileTableCard->setObjectName(QStringLiteral("contentCard"));
    applyShadow(pileTableCard, 16, 4, QColor(19, 27, 46, 12));
    auto *ptcLayout = new QVBoxLayout(pileTableCard);
    ptcLayout->setContentsMargins(20, 16, 20, 16);
    ptcLayout->setSpacing(10);
    auto *pileTableTitle = new QLabel(QStringLiteral("电桩列表"));
    pileTableTitle->setStyleSheet(QStringLiteral(
        "color:#131b2e; font-size:15px; font-weight:700; background:transparent; border:none;"));
    ptcLayout->addWidget(pileTableTitle);
    ptcLayout->addWidget(makeDivider());

    auto *pileFilterRow = new QHBoxLayout;
    pileFilterRow->setContentsMargins(0, 0, 0, 0);
    pileFilterRow->setSpacing(10);
    pileFilterLabel->setText(QStringLiteral("当前显示：全部电桩"));
    pileFilterLabel->setStyleSheet(QStringLiteral(
        "color:#737686; font-size:13px; font-weight:500; background:transparent; border:none;"));
    pileFilterRow->addWidget(pileFilterLabel);
    pileFilterRow->addStretch();
    showAllPilesBtn->setText(QStringLiteral("显示全部电桩"));
    showAllPilesBtn->setObjectName(QStringLiteral("secondaryBtn"));
    showAllPilesBtn->setMinimumHeight(30);
    showAllPilesBtn->setCursor(Qt::PointingHandCursor);
    showAllPilesBtn->setVisible(false);
    pileFilterRow->addWidget(showAllPilesBtn);
    ptcLayout->addLayout(pileFilterRow);

    pilesTable->setColumnCount(8);
    pilesTable->setHorizontalHeaderLabels({
        QStringLiteral("ID"), QStringLiteral("站点"), QStringLiteral("编号"),
        QStringLiteral("类型"), QStringLiteral("功率"), QStringLiteral("状态"),
        QStringLiteral("累计次数"), QStringLiteral("累计充电时长")
    });
    pilesTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    pilesTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
    pilesTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    pilesTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    pilesTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
    pilesTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Fixed);
    pilesTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Fixed);
    pilesTable->horizontalHeader()->setSectionResizeMode(7, QHeaderView::Fixed);
    pilesTable->setColumnWidth(0, 70);
    pilesTable->setColumnWidth(1, 170);
    pilesTable->setColumnWidth(2, 90);
    pilesTable->setColumnWidth(3, 90);
    pilesTable->setColumnWidth(4, 100);
    pilesTable->setColumnWidth(5, 100);
    pilesTable->setColumnWidth(6, 100);
    pilesTable->setColumnWidth(7, 130);
    pilesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    pilesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    pilesTable->setAlternatingRowColors(true);
    pilesTable->setShowGrid(false);
    pilesTable->verticalHeader()->setVisible(false);
    pilesTable->verticalHeader()->setDefaultSectionSize(48);
    pilesTable->setMinimumHeight(190);
    pilesTable->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    pilesTable->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
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

    auto *pileStatusLabel = new QLabel(QStringLiteral("目标状态"));
    pileStatusLabel->setStyleSheet(QStringLiteral(
        "color:#737686; font-size:13px; font-weight:500; background:transparent; border:none;"));
    pileControlRow->addWidget(pileStatusLabel);
    pileStatusCombo->addItem(QStringLiteral("闲置"), QStringLiteral("闲置"));
    pileStatusCombo->addItem(QStringLiteral("在用"), QStringLiteral("在用"));
    pileStatusCombo->addItem(QStringLiteral("故障"), QStringLiteral("故障"));
    pileStatusCombo->setMinimumHeight(36);
    pileStatusCombo->setFixedWidth(110);
    pileControlRow->addWidget(pileStatusCombo);
    auto *setPileStatusBtn = new QPushButton(QStringLiteral("设置状态"));
    setPileStatusBtn->setObjectName(QStringLiteral("pileStatusBtn"));
    setPileStatusBtn->setMinimumHeight(36);
    setPileStatusBtn->setCursor(Qt::PointingHandCursor);
    pileControlRow->addWidget(setPileStatusBtn);
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
    connect(adjustPileCountBtn, &QPushButton::clicked, this, &AdminWindow::adjustStationPileCount);
    connect(stationsTable, &QTableWidget::currentCellChanged, this,
            [this](int currentRow, int, int, int) {
        if (currentRow < 0 || !stationsTable->item(currentRow, 0)
            || !stationsTable->item(currentRow, 4)) {
            return;
        }
        stationAdjustIdSpin->setValue(stationsTable->item(currentRow, 0)->text().toInt());
        const QString countText = stationsTable->item(currentRow, 4)->text().section('/', 1).trimmed();
        stationAdjustPileCountSpin->setValue(countText.toInt());
    });
    // 点击电站行，可查看该站所有电桩的实时状态明细
    connect(stationsTable, &QTableWidget::cellClicked, this,
            [this](int row, int) {
        if (!stationsTable->item(row, 0) || !stationsTable->item(row, 1)) return;
        showPilesForStation(stationsTable->item(row, 0)->text().toInt(),
                             stationsTable->item(row, 1)->text());
    });
    connect(showAllPilesBtn, &QPushButton::clicked, this, &AdminWindow::showAllPiles);
    connect(setPileStatusBtn, &QPushButton::clicked, this, &AdminWindow::setSelectedPileStatus);
    connect(pilesTable, &QTableWidget::cellClicked, this,
            [this](int row, int) {
        if (!pilesTable->item(row, 0) || !pilesTable->item(row, 5)) return;
        pileIdSpin->setValue(pilesTable->item(row, 0)->text().toInt());
        const int statusIndex = pileStatusCombo->findData(pilesTable->item(row, 5)->text());
        if (statusIndex >= 0) pileStatusCombo->setCurrentIndex(statusIndex);
    });

}
