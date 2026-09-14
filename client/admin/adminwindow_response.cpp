#include "adminwindow.h"
#include "admindesign.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QStackedWidget>

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
        if (users.isEmpty()) {
            showTableEmptyState(usersTable, QStringLiteral("暂无用户数据"));
        } else {
            usersTable->clearSpans();
            usersTable->setRowCount(users.size());
        }
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
        const QSignalBlocker blocker(orderStationFilter);
        orderStationFilter->clear();
        orderStationFilter->addItem(QStringLiteral("全部站点"), -1);
        if (stations.isEmpty()) {
            showTableEmptyState(stationsTable, QStringLiteral("暂无站点数据"));
        } else {
            stationsTable->clearSpans();
            stationsTable->setRowCount(stations.size());
        }
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

            stationsTable->setItem(row, 6, new QTableWidgetItem(
                QStringLiteral("%1, %2")
                    .arg(station.value("longitude").toDouble(), 0, 'f', 6)
                    .arg(station.value("latitude").toDouble(), 0, 'f', 6)));
        }
        if (!stations.isEmpty()) {
            int row = stationsTable->currentRow();
            if (row < 0 || row >= stations.size()) {
                row = 0;
                stationsTable->selectRow(row);
            }
            stationAdjustIdSpin->setValue(stationsTable->item(row, 0)->text().toInt());
            stationAdjustPileCountSpin->setValue(stations.at(row).toObject().value("pileCount").toInt());
        }
        const int selectedIndex = orderStationFilter->findData(selectedStation);
        if (selectedIndex >= 0) orderStationFilter->setCurrentIndex(selectedIndex);
        send(QStringLiteral("admin_query_piles"));
    } else if (data.contains("piles")) {
        allPilesCache = data.value("piles").toArray();
        if (selectedPileStationId >= 0) {
            // Refreshing data shouldn't silently drop an active station filter.
            QJsonArray filtered;
            for (const QJsonValue &val : allPilesCache) {
                if (val.toObject().value("stationId").toInt() == selectedPileStationId) {
                    filtered.append(val);
                }
            }
            populatePilesTable(filtered);
            pileFilterLabel->setText(QStringLiteral("当前显示：%1 的电桩（共 %2 个）")
                                          .arg(selectedPileStationName).arg(filtered.size()));
        } else {
            populatePilesTable(allPilesCache);
        }
    } else if (data.contains("revenueToday") && data.contains("pileStatus")) {
        populateStatsCards(data);
        const QJsonArray trend = data.value("revenueTrend").toArray();
        const int days = data.value("trendDays").toInt(
            revenuePeriodCombo->currentData().toInt());
        populateRevenueTrend(trend);
        populateRevenueChart(trend, days);
    } else if (data.contains("orders")) {
        const QJsonArray orders = data.value("orders").toArray();
        if (orders.isEmpty()) {
            showTableEmptyState(ordersTable, QStringLiteral("暂无订单数据"));
            return;
        }
        ordersTable->clearSpans();
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
        stationPileCountEdit->setValue(2);
        QMessageBox::information(this, QStringLiteral("增加站点"), QStringLiteral("站点已成功添加"));
        refreshStationsAndPiles();
    } else if (action == QStringLiteral("admin_set_station_pile_count")) {
        refreshStationsAndPiles();
    } else if (action == QStringLiteral("set_user_status")) {
        refreshUsers();
    } else if (action == QStringLiteral("admin_set_pile_status")) {
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
