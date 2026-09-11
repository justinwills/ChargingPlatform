#include "adminwindow.h"

#include <QComboBox>
#include <QDateEdit>
#include <QJsonObject>
#include <QLineEdit>
#include <QMessageBox>
#include <QSpinBox>
#include <QTableWidget>
#include <QAbstractButton>

// Thin request-sending methods for each tab: refresh/query/mutate calls
// that hand off to send() and get their results via handleResponse().

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
    if (!usersTable->item(row, 0) || !usersTable->item(row, 4)) {
        showError(QStringLiteral("请选择有效用户"));
        return;
    }

    const int userId = usersTable->item(row, 0)->text().toInt();
    const QString phone = usersTable->item(row, 1) ? usersTable->item(row, 1)->text() : QString();
    const QString currentStatus = usersTable->item(row, 4)->text();
    const QString nextStatus = currentStatus == QStringLiteral("冻结")
                                   ? QStringLiteral("正常")
                                   : QStringLiteral("冻结");
    const QString actionText = nextStatus == QStringLiteral("冻结")
                                   ? QStringLiteral("冻结")
                                   : QStringLiteral("恢复");

    QMessageBox msgBox(this);
    msgBox.setIcon(nextStatus == QStringLiteral("冻结") ? QMessageBox::Warning : QMessageBox::Question);
    msgBox.setWindowTitle(QStringLiteral("%1用户").arg(actionText));
    msgBox.setText(QStringLiteral("确认%1该用户吗？").arg(actionText));
    msgBox.setInformativeText(QStringLiteral("用户ID：%1\n手机号：%2\n当前状态：%3")
                                  .arg(userId)
                                  .arg(phone.isEmpty() ? QStringLiteral("-") : phone)
                                  .arg(currentStatus));
    msgBox.setStandardButtons(QMessageBox::Cancel | QMessageBox::Ok);
    msgBox.button(QMessageBox::Ok)->setText(actionText);
    msgBox.button(QMessageBox::Cancel)->setText(QStringLiteral("取消"));
    msgBox.setDefaultButton(QMessageBox::Cancel);
    msgBox.setStyleSheet(QStringLiteral(
        "QMessageBox { background: #ffffff; }"
        "QLabel { color: #131b2e; font-size: 14px; }"
        "QPushButton { background: #2563eb; color: #ffffff; border: none; "
        "border-radius: 10px; padding: 8px 24px; font-weight: 600; min-width: 60px; }"
        "QPushButton:hover { background: #1d4ed8; }"
    ));
    if (msgBox.exec() != QMessageBox::Ok) {
        return;
    }

    send(QStringLiteral("set_user_status"), {
        {"userId", userId},
        {"status", nextStatus}
    });
}

void AdminWindow::refreshStationsAndPiles()
{
    send(QStringLiteral("admin_query_stations"));
}

void AdminWindow::addStation()
{
    const QString name = stationNameEdit->text().trimmed();
    const QString address = stationAddressEdit->text().trimmed();
    if (name.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("增加站点"), QStringLiteral("请输入站点名称"));
        return;
    }
    if (address.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("增加站点"), QStringLiteral("请输入站点地址"));
        return;
    }

    send(QStringLiteral("admin_add_station"), {
        {"name", name},
        {"address", address},
        {"longitude", stationLongitudeEdit->value()},
        {"latitude", stationLatitudeEdit->value()},
        {"price", stationPriceEdit->value()},
        {"pileCount", stationPileCountEdit->value()}
    });
}

void AdminWindow::adjustStationPileCount()
{
    send(QStringLiteral("admin_set_station_pile_count"), {
        {"stationId", stationAdjustIdSpin->value()},
        {"pileCount", stationAdjustPileCountSpin->value()}
    });
}

void AdminWindow::setSelectedPileStatus()
{
    send(QStringLiteral("admin_set_pile_status"), {
        {"pileId", pileIdSpin->value()},
        {"status", pileStatusCombo->currentData().toString()}
    });
}

void AdminWindow::refreshStats()
{
    send(QStringLiteral("admin_stats"), {
        {"days", revenuePeriodCombo->currentData().toInt()}
    });
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

