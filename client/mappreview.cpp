#include "mappreview.h"

#include <QFont>
#include <QJsonObject>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPointF>
#include <QtGlobal>

namespace {

struct Pin {
    QString name;
    int free;
    int total;
    double power;
};

QPointF pinPos(int index, const QRectF &rect)
{
    const QPointF anchors[4] = {
        QPointF(0.28, 0.34),
        QPointF(0.72, 0.20),
        QPointF(0.58, 0.74),
        QPointF(0.20, 0.68)
    };
    const QPointF a = anchors[qBound(0, index, 3)];
    return QPointF(rect.left() + rect.width() * a.x(),
                   rect.top() + rect.height() * a.y());
}

} // namespace

MapPreview::MapPreview(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(260, 120);
}

void MapPreview::setStations(const QJsonArray &stations)
{
    m_stations = stations;
    update();
}

void MapPreview::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF r = QRectF(this->rect());

    p.fillRect(r, QColor(234, 238, 246));

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(213, 232, 212));
    p.drawRoundedRect(QRectF(r.left() + 16, r.top() + 8, 92, 46), 18, 18);
    p.drawRoundedRect(QRectF(r.right() - 96, r.top() + 60, 76, 42), 18, 18);

    QPainterPath water;
    water.moveTo(r.right() - 30, r.bottom() - 34);
    water.lineTo(r.right() - 6, r.bottom() - 34);
    water.lineTo(r.right() - 6, r.bottom());
    water.lineTo(r.right() - 58, r.bottom());
    water.closeSubpath();
    p.setBrush(QColor(209, 219, 255));
    p.drawPath(water);

    QPen road(QColor(226, 231, 255), 3, Qt::DashLine);
    QVector<qreal> dash;
    dash << 8 << 7;
    road.setDashPattern(dash);
    p.setPen(road);
    p.drawLine(QPointF(r.left() - 10, r.height() * 0.72),
               QPointF(r.right() + 10, r.height() * 0.66));

    QPen mainRoad(QColor(255, 255, 255), 15, Qt::SolidLine, Qt::RoundCap);
    p.setPen(mainRoad);
    p.drawLine(QPointF(r.left() - 10, r.height() * 0.44),
               QPointF(r.right() + 10, r.height() * 0.40));
    p.drawLine(QPointF(r.width() * 0.62, r.top() - 10),
               QPointF(r.width() * 0.52, r.bottom() + 10));

    QPen roadEdge(QColor(255, 255, 255), 5, Qt::SolidLine, Qt::RoundCap);
    p.setPen(roadEdge);
    p.drawLine(QPointF(r.left() - 10, r.height() * 0.94),
               QPointF(r.right() + 10, r.height() * 0.90));

    p.setPen(QColor(115, 118, 134));
    QFont f = p.font();
    f.setPointSize(7);
    p.setFont(f);
    p.drawText(QRectF(r.left() + 12, r.top() + 16, 72, 14),
               Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("主城区"));
    p.drawText(QRectF(r.width() * 0.66, r.top() + 24, 84, 14),
               Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("环线高架"));
    p.drawText(QRectF(r.width() * 0.16, r.height() * 0.78, 84, 14),
               Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("京西北路"));

    QVector<Pin> pins;
    const int realCount = qMin(4, m_stations.size());
    for (int i = 0; i < realCount; ++i) {
        const QJsonObject s = m_stations.at(i).toObject();
        Pin pin;
        pin.name = s.value("name").toString();
        pin.free = s.value("freePileCount").toInt();
        pin.total = s.value("pileCount").toInt();
        pin.power = s.value("price").toDouble();
        pins.append(pin);
    }
    if (pins.isEmpty()) {
        pins.append({QStringLiteral("睿光·常德站"), 8, 12, 180});
        pins.append({QStringLiteral("城市·中关村"), 5, 16, 120});
        pins.append({QStringLiteral("京西停车场"), 14, 20, 160});
    }

    for (int i = 0; i < pins.size(); ++i) {
        const QPointF c = pinPos(i, r);

        p.setPen(Qt::NoPen);
        p.setBrush(QColor(37, 99, 235, 40));
        p.drawEllipse(c, 17, 17);

        p.setBrush(QColor(255, 255, 255));
        p.drawEllipse(c, 13, 13);

        p.setBrush(pins[i].free > 0 ? QColor(0, 108, 73) : QColor(186, 26, 26));
        p.drawEllipse(QPointF(c.x(), c.y()), 4, 4);

        const QString label = QStringLiteral("%1  %2/%3  ·  %4kW")
                                  .arg(pins[i].name)
                                  .arg(pins[i].free)
                                  .arg(pins[i].total)
                                  .arg(pins[i].power, 0, 'g', 3);

        QFont sf = p.font();
        sf.setPointSize(7);
        sf.setBold(true);
        p.setFont(sf);
        const QRectF cr = p.boundingRect(QRectF(), 0, label).adjusted(-6, -3, 6, 3);
        const QRectF pill(c.x() - cr.width() / 2, c.y() - 26, cr.width(), cr.height());

        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, 236));
        p.drawRoundedRect(pill.translated(0, 1), 9, 9);
        p.setPen(QColor(19, 27, 46));
        p.drawText(pill, Qt::AlignCenter, label);

        p.setBrush(QColor(37, 99, 235));
        p.drawEllipse(QPointF(c.x() + cr.width() / 2 + 6, c.y() + 9), 2.5, 2.5);

        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 83, 219));
        p.drawEllipse(QPointF(c.x(), c.y() + 14), 3.5, 3.5);
    }
}