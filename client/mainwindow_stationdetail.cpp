#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "flowlayout.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QPushButton>
#include <QVBoxLayout>

// Renders server data into the station-detail page (address/price/pile
// summary) and rebuilds the nearby-stations card list.

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
