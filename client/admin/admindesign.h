#ifndef ADMINDESIGN_H
#define ADMINDESIGN_H

// Design-system constants and small UI helpers shared by the AdminWindow
// page builders (adminwindow_*.cpp). Split out of adminwindow.cpp so each
// builder file can reuse the same colors/helpers without redeclaring them.

#include <QColor>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QString>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QWidget>

// ─── Design System Constants ───────────────────────────────────────────────
// Colors
static const QString cPrimary       = QStringLiteral("#2563eb");
static const QString cPrimaryDark   = QStringLiteral("#1d4ed8");
static const QString cPrimaryLight  = QStringLiteral("#dbe1ff");
static const QString cPrimaryFaint  = QStringLiteral("#e2e7ff");
static const QString cOnPrimary     = QStringLiteral("#ffffff");
static const QString cSurface       = QStringLiteral("#faf8ff");
static const QString cSurfaceCard   = QStringLiteral("#ffffff");
static const QString cOnSurface     = QStringLiteral("#131b2e");
static const QString cOnSurfaceVar  = QStringLiteral("#434655");
static const QString cOutline       = QStringLiteral("#737686");
static const QString cOutlineVar    = QStringLiteral("#c3c6d7");
static const QString cBorder        = QStringLiteral("#eaedff");
static const QString cInputBg       = QStringLiteral("#f2f3ff");
static const QString cSuccess       = QStringLiteral("#006c49");
static const QString cSuccessBg     = QStringLiteral("#e2f6ec");
static const QString cSuccessBorder = QStringLiteral("#a7f3d0");
static const QString cWarning       = QStringLiteral("#b45309");
static const QString cWarningBg     = QStringLiteral("#fff1e6");
static const QString cWarningBorder = QStringLiteral("#fde68a");
static const QString cDanger        = QStringLiteral("#ba1a1a");
static const QString cDangerBg      = QStringLiteral("#ffdad6");
static const QString cDangerBorder  = QStringLiteral("#fecaca");
static const QString cDangerText    = QStringLiteral("#93000a");

// ─── Helpers ────────────────────────────────────────────────────────────────
static inline void applyShadow(QWidget *w, int blur = 20, int yOff = 6,
                        const QColor &color = QColor(19, 27, 46, 22))
{
    auto *shadow = new QGraphicsDropShadowEffect(w);
    shadow->setBlurRadius(blur);
    shadow->setOffset(0, yOff);
    shadow->setColor(color);
    w->setGraphicsEffect(shadow);
}

static inline QFrame *makeDivider(const QString &bg = QStringLiteral("#f2f3ff"))
{
    auto *line = new QFrame;
    line->setFixedHeight(1);
    line->setStyleSheet(QStringLiteral("background:%1; border:none;").arg(bg));
    return line;
}

static inline void showTableEmptyState(QTableWidget *table, const QString &message)
{
    table->clearContents();
    table->clearSpans();
    table->setRowCount(1);
    auto *item = new QTableWidgetItem(message);
    item->setTextAlignment(Qt::AlignCenter);
    item->setForeground(QColor(cOutline));
    table->setItem(0, 0, item);
    table->setSpan(0, 0, 1, table->columnCount());
}

#endif // ADMINDESIGN_H
