#include "chargering.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPen>
#include <QtGlobal>

ChargeRing::ChargeRing(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(160, 160);
}

void ChargeRing::setValue(int percent)
{
    m_percent = qBound(0, percent, 100);
    update();
}

void ChargeRing::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF rect = QRectF(this->rect()).adjusted(14, 14, -14, -14);

    QPen trackPen(QColor(234, 237, 255), 14, Qt::SolidLine, Qt::RoundCap);
    painter.setPen(trackPen);
    painter.drawEllipse(rect);

    const int span = -static_cast<int>(m_percent * 3.6 * 16);

    QPen glowPen(QColor(0, 83, 219, 28), 30, Qt::SolidLine, Qt::RoundCap);
    painter.setPen(glowPen);
    painter.drawArc(rect.adjusted(-3, -3, 3, 3), 90 * 16, span);

    QLinearGradient gradient(rect.topLeft(), rect.bottomRight());
    gradient.setColorAt(0.0, QColor(37, 99, 235));
    gradient.setColorAt(0.62, QColor(0, 76, 198));
    gradient.setColorAt(1.0, QColor(0, 83, 219));

    QPen progressPen(QBrush(gradient), 14, Qt::SolidLine, Qt::RoundCap);
    painter.setPen(progressPen);
    painter.drawArc(rect, 90 * 16, span);

    QPen capPen(QColor(219, 225, 255), 6, Qt::SolidLine, Qt::RoundCap);
    painter.setPen(capPen);
    painter.drawArc(rect, 90 * 16, qMin<int>(span, 0) + 12 * 16);
}