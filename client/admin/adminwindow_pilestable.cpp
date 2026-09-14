#include "adminwindow.h"
#include "admindesign.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QPushButton>

// Renders server data into the Tab 2 (Stations & Piles) piles table, and
// the station-filter helpers used by its "view piles" actions.

// ─── Piles Table Population ────────────────────────────────────────────────────
void AdminWindow::populatePilesTable(const QJsonArray &piles)
{
    if (piles.isEmpty()) {
        showTableEmptyState(pilesTable, QStringLiteral("暂无电桩数据"));
    } else {
        pilesTable->clearSpans();
        pilesTable->setRowCount(piles.size());
    }
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
        if (pileStatus == QStringLiteral("闲置")) {
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

        // totalDuration is stored in minutes; show it as "Xh Ym" once it's an hour or more.
        const int totalMinutes = pile.value("totalDuration").toInt();
        const QString durationText = totalMinutes >= 60
            ? QStringLiteral("%1 小时 %2 分钟").arg(totalMinutes / 60).arg(totalMinutes % 60)
            : QStringLiteral("%1 分钟").arg(totalMinutes);
        pilesTable->setItem(row, 7, new QTableWidgetItem(durationText));
    }
}

void AdminWindow::showPilesForStation(int stationId, const QString &stationName)
{
    selectedPileStationId = stationId;
    selectedPileStationName = stationName;
    QJsonArray filtered;
    for (const QJsonValue &val : allPilesCache) {
        if (val.toObject().value("stationId").toInt() == stationId) {
            filtered.append(val);
        }
    }
    populatePilesTable(filtered);
    pileFilterLabel->setText(QStringLiteral("当前显示：%1 的电桩（共 %2 个）")
                                  .arg(stationName).arg(filtered.size()));
    showAllPilesBtn->setVisible(true);
}

void AdminWindow::showAllPiles()
{
    selectedPileStationId = -1;
    selectedPileStationName.clear();
    populatePilesTable(allPilesCache);
    pileFilterLabel->setText(QStringLiteral("当前显示：全部电桩"));
    showAllPilesBtn->setVisible(false);
}

