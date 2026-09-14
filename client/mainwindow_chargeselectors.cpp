#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QMessageBox>

// Builds/updates the charge-page station and pile summary cards, and
// the modal selector dialogs opened from them.

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

