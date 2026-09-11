#include "database.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QDebug>

// ---------- 销售业绩统计 ----------

double Database::getRevenueToday()
{
    QSqlQuery query("select sum(fee) from orders where status = '已结算' "
                     "and date(end_time) = date('now', 'localtime')", currentThreadDb());
    if (query.next()) {
        return query.value(0).toDouble();
    }
    return 0;
}

double Database::getRevenueThisMonth()
{
    QSqlQuery query("select sum(fee) from orders where status = '已结算' "
                     "and strftime('%Y-%m', end_time) = strftime('%Y-%m', 'now', 'localtime')", currentThreadDb());
    if (query.next()) {
        return query.value(0).toDouble();
    }
    return 0;
}

double Database::getRevenueTotal()
{
    QSqlQuery query("select sum(fee) from orders where status = '已结算'", currentThreadDb());
    if (query.next()) {
        return query.value(0).toDouble();
    }
    return 0;
}

QList<QPair<QString, double>> Database::getRevenueTrend(int days)
{
    QList<QPair<QString, double>> result;
    days = qMax(1, days);
    const QDate endDate = QDate::currentDate();
    const QDate startDate = endDate.addDays(-(days - 1));

    QSqlQuery query(currentThreadDb());
    query.prepare("select date(end_time) as d, sum(fee) from orders "
                   "where status = '已结算' and date(end_time) >= ? and date(end_time) <= ? "
                   "group by d order by d");
    query.addBindValue(startDate.toString(QStringLiteral("yyyy-MM-dd")));
    query.addBindValue(endDate.toString(QStringLiteral("yyyy-MM-dd")));
    if (!query.exec()) {
        qDebug() << "getRevenueTrend 失败：" << query.lastError().text();
        return result;
    }
    QMap<QString, double> dailyRevenue;
    while (query.next()) {
        dailyRevenue.insert(query.value(0).toString(), query.value(1).toDouble());
    }
    for (int offset = 0; offset < days; ++offset) {
        const QString date = startDate.addDays(offset).toString(QStringLiteral("yyyy-MM-dd"));
        result.append(qMakePair(date, dailyRevenue.value(date, 0.0)));
    }
    return result;
}
