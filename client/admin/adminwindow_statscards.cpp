#include "adminwindow.h"
#include "admindesign.h"

#include <QAbstractButton>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>

#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

#include <algorithm>
#include <cmath>

// Renders server data into the Tab 0 (Revenue & Stats) widgets: the four
// summary cards and the revenue trend chart.

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

void AdminWindow::populateRevenueChart(const QJsonArray &trend, int days)
{
    auto *chart = new QChart;
    chart->setTitle(QStringLiteral("近 %1 日营收趋势").arg(days));
    chart->setBackgroundVisible(false);
    chart->setAnimationOptions(QChart::SeriesAnimations);
    chart->legend()->setVisible(false);

    auto *series = new QLineSeries(chart);
    series->setName(QStringLiteral("营收（元）"));
    series->setPointsVisible(true);
    QPen linePen{QColor(cPrimary)};
    linePen.setWidth(3);
    series->setPen(linePen);

    QDateTime firstPoint;
    QDateTime lastPoint;
    double maximumRevenue = 0.0;
    for (const QJsonValue &value : trend) {
        const QJsonObject point = value.toObject();
        const QDate date = QDate::fromString(point.value("date").toString(), Qt::ISODate);
        if (!date.isValid()) continue;

        const QDateTime dateTime(date, QTime(12, 0));
        const double revenue = point.value("revenue").toDouble();
        series->append(dateTime.toMSecsSinceEpoch(), revenue);
        if (!firstPoint.isValid()) firstPoint = dateTime;
        lastPoint = dateTime;
        maximumRevenue = qMax(maximumRevenue, revenue);
    }

    chart->addSeries(series);
    if (series->count() > 0) {
        auto *dateAxis = new QDateTimeAxis(chart);
        dateAxis->setFormat(QStringLiteral("MM-dd"));
        dateAxis->setTitleText(QStringLiteral("日期"));
        dateAxis->setTickCount(days == 7 ? 7 : 8);
        dateAxis->setRange(firstPoint, lastPoint);

        auto *valueAxis = new QValueAxis(chart);
        valueAxis->setTitleText(QStringLiteral("营收（元）"));
        valueAxis->setLabelFormat(QStringLiteral("%.2f"));
        const double upperBound = qMax(10.0,
            std::ceil(maximumRevenue * 1.2 / 10.0) * 10.0);
        valueAxis->setRange(0.0, upperBound);

        chart->addAxis(dateAxis, Qt::AlignBottom);
        chart->addAxis(valueAxis, Qt::AlignLeft);
        series->attachAxis(dateAxis);
        series->attachAxis(valueAxis);
    } else {
        chart->setTitle(QStringLiteral("近 %1 日暂无已结算订单数据").arg(days));
    }

    QChart *oldChart = revenueChartView->chart();
    revenueChartView->setChart(chart);
    delete oldChart;
}

