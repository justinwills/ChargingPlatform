#include "adminwindow.h"

QString AdminWindow::stylesheet()
{
    return QStringLiteral(R"(
        /* ── Base ─────────────────────────────────────────────────── */
        QWidget {
            font-family: "Inter", "Microsoft YaHei";
            color: #434655;
            font-size: 13px;
        }
        QMainWindow, QWidget#centralwidget {
            background: #faf8ff;
        }
        QScrollBar:vertical {
            background: transparent; width: 6px; margin: 0;
        }
        QScrollBar::handle:vertical {
            background: #c3c6d7; border-radius: 3px; min-height: 24px;
        }
        QScrollBar::handle:vertical:hover { background: #a3a6b7; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }

        /* ── Sidebar ──────────────────────────────────────────────── */
        QFrame#sidebar {
            background: #ffffff;
            border-right: 1px solid #eaedff;
        }
        QLabel#sidebarBrand {
            color: #2563eb;
            font-size: 15px;
            font-weight: 700;
            background: transparent;
            border: none;
        }
        QLabel#sidebarSub {
            color: #737686;
            font-size: 11px;
            background: transparent;
            border: none;
        }
        QPushButton#navBtn {
            background: transparent;
            color: #434655;
            border: none;
            border-radius: 12px;
            font-size: 14px;
            font-weight: 500;
            text-align: left;
            padding: 12px 20px;
        }
        QPushButton#navBtn:hover {
            background: #f2f3ff;
            color: #131b2e;
        }
        QPushButton#navBtn0:checked {
            background: #e2e7ff;
            color: #2563eb;
            font-weight: 700;
        }
        QPushButton#navBtn1:checked {
            background: #dff2e8;
            color: #006c49;
            font-weight: 700;
        }
        QPushButton#navBtn2:checked {
            background: #ffe9cc;
            color: #b45309;
            font-weight: 700;
        }
        QPushButton#navBtn3:checked {
            background: #ece4ff;
            color: #6d28d9;
            font-weight: 700;
        }

        /* ── Header bar ───────────────────────────────────────────── */
        QFrame#headerBar {
            background: #ffffff;
            border-bottom: 1px solid #eaedff;
        }
        QLabel#headerTitle {
            color: #131b2e;
            font-size: 20px;
            font-weight: 700;
            background: transparent;
            border: none;
        }
        QLabel#headerSub {
            color: #737686;
            font-size: 12px;
            background: transparent;
            border: none;
        }
        QPushButton#logoutBtn {
            background: #ffdad6;
            color: #93000a;
            border: none;
            border-radius: 10px;
            font-size: 13px;
            font-weight: 600;
            padding: 6px 16px;
        }
        QPushButton#logoutBtn:hover { background: #ffc9c2; }
        QPushButton#logoutBtn:pressed { background: #ffb3ab; }

        /* ── Stat cards ───────────────────────────────────────────── */
        QFrame#statCard {
            background: #ffffff;
            border: 1px solid #eaedff;
            border-radius: 16px;
        }
        QFrame#statCardAccent {
            background: #ffffff;
            border: 1px solid #eaedff;
            border-left: 3px solid #2563eb;
            border-radius: 16px;
        }
        QLabel#statLabel {
            color: #737686;
            font-size: 12px;
            font-weight: 500;
            background: transparent;
            border: none;
        }
        QLabel#statValue {
            color: #131b2e;
            font-family: "Inter", "Consolas", "Microsoft YaHei";
            font-size: 24px;
            font-weight: 700;
            background: transparent;
            border: none;
        }
        QLabel#statValueSm {
            color: #131b2e;
            font-family: "Inter", "Consolas", "Microsoft YaHei";
            font-size: 18px;
            font-weight: 700;
            background: transparent;
            border: none;
        }
        QLabel#statSub {
            color: #737686;
            font-size: 11px;
            background: transparent;
            border: none;
        }

        /* ── Status dot indicators ────────────────────────────────── */
        QLabel#dotIdle {
            background: #006c49;
            border-radius: 5px;
            border: none;
        }
        QLabel#dotBusy {
            background: #d97706;
            border-radius: 5px;
            border: none;
        }
        QLabel#dotFault {
            background: #ba1a1a;
            border-radius: 5px;
            border: none;
        }

        /* ── Content cards ────────────────────────────────────────── */
        QFrame#contentCard {
            background: #ffffff;
            border: 1px solid #eaedff;
            border-radius: 16px;
        }

        /* ── Input fields ─────────────────────────────────────────── */
        QLineEdit {
            background: #f2f3ff;
            border: 1px solid #eaedff;
            border-radius: 12px;
            padding: 10px 14px;
            color: #131b2e;
            font-size: 14px;
            selection-background-color: #dbe1ff;
            selection-color: #003ea8;
        }
        QLineEdit:focus {
            border: 1px solid #2563eb;
            background: #ffffff;
        }
        QLineEdit[placeholderText] {
            color: #9aa0b5;
        }

        QSpinBox, QDoubleSpinBox {
            background: #f2f3ff;
            border: 1px solid #eaedff;
            border-radius: 12px;
            padding: 8px 12px;
            color: #131b2e;
            font-size: 14px;
        }
        QSpinBox:focus, QDoubleSpinBox:focus {
            border: 1px solid #2563eb;
        }
        QComboBox {
            background: #f2f3ff;
            border: 1px solid #eaedff;
            border-radius: 12px;
            padding: 8px 12px;
            color: #131b2e;
            font-size: 14px;
        }
        QComboBox:focus {
            border: 1px solid #2563eb;
        }
        QComboBox::drop-down {
            border: none;
            width: 28px;
        }
        QComboBox QAbstractItemView {
            background: #ffffff;
            border: 1px solid #eaedff;
            border-radius: 12px;
            color: #131b2e;
            selection-background-color: #dbe1ff;
            selection-color: #003ea8;
            padding: 4px;
        }
        QDateEdit {
            background: #f2f3ff;
            border: 1px solid #eaedff;
            border-radius: 12px;
            padding: 8px 12px;
            color: #131b2e;
            font-size: 14px;
        }
        QDateEdit:focus {
            border: 1px solid #2563eb;
        }

        /* ── Primary buttons ──────────────────────────────────────── */
        QPushButton#primaryBtn {
            background: #2563eb;
            color: #ffffff;
            border: none;
            border-radius: 12px;
            font-size: 14px;
            font-weight: 600;
            padding: 10px 20px;
            min-height: 20px;
        }
        QPushButton#primaryBtn:hover { background: #1d4ed8; }
        QPushButton#primaryBtn:pressed { background: #003ea8; }
        QPushButton#primaryBtn:disabled { background: #a5b7e8; color: #e8ecf7; }

        /* ── Secondary buttons ────────────────────────────────────── */
        QPushButton#secondaryBtn {
            background: #ffffff;
            color: #131b2e;
            border: 1px solid #eaedff;
            border-radius: 12px;
            font-size: 14px;
            font-weight: 500;
            padding: 10px 20px;
            min-height: 20px;
        }
        QPushButton#secondaryBtn:hover {
            background: #f2f3ff;
            border: 1px solid #2563eb;
        }
        QPushButton#secondaryBtn:pressed { background: #e2e7ff; }

        /* ── Danger buttons ───────────────────────────────────────── */
        QPushButton#dangerBtn {
            background: #ffdad6;
            color: #93000a;
            border: none;
            border-radius: 12px;
            font-size: 14px;
            font-weight: 600;
            padding: 10px 20px;
            min-height: 20px;
        }
        QPushButton#dangerBtn:hover { background: #ffc9c2; }

        /* ── Warning buttons ──────────────────────────────────────── */
        QPushButton#warningBtn {
            background: #fff1e6;
            color: #b45309;
            border: none;
            border-radius: 12px;
            font-size: 14px;
            font-weight: 600;
            padding: 10px 20px;
            min-height: 20px;
        }
        QPushButton#warningBtn:hover { background: #ffddb8; }

        /* ── Pile status control button ───────────────────────────── */
        QPushButton#pileStatusBtn {
            background: #7c3aed;
            color: #d81e1e;
            border: none;
            border-radius: 12px;
            font-size: 14px;
            font-weight: 600;
            padding: 10px 20px;
            min-height: 20px;
        }
        QPushButton#pileStatusBtn:hover { background: #6d28d9; }
        QPushButton#pileStatusBtn:pressed { background: #5b21b6; }

        /* ── Success buttons ──────────────────────────────────────── */
        QPushButton#successBtn {
            background: #e2f6ec;
            color: #006c49;
            border: none;
            border-radius: 12px;
            font-size: 14px;
            font-weight: 600;
            padding: 10px 20px;
            min-height: 20px;
        }
        QPushButton#successBtn:hover { background: #c6eed8; }

        /* ── Tables ───────────────────────────────────────────────── */
        QTableWidget {
            background: #ffffff;
            alternate-background-color: #f4f6ff;
            border: 1px solid #eaedff;
            border-radius: 12px;
            gridline-color: #f2f3ff;
            color: #434655;
            font-size: 13px;
            selection-background-color: #dbe1ff;
            selection-color: #131b2e;
            outline: none;
            gridline-color: #f2f3ff;
            show-decoration-selected: 1;
        }
        QTableWidget::item {
            padding: 8px 10px;
            border-bottom: 1px solid #f2f3ff;
            color: #434655;
        }
        QTableWidget::item:selected {
            background: #e2e7ff;
            color: #131b2e;
        }
        QTableWidget::item:hover:!selected {
            background: #f8f9ff;
        }
        QHeaderView::section {
            background: #f8f9ff;
            color: #737686;
            font-size: 12px;
            font-weight: 600;
            border: none;
            border-bottom: 1px solid #eaedff;
            padding: 10px 12px;
            text-align: left;
        }
        QHeaderView::section:hover {
            background: #f2f3ff;
        }
        QTableCornerButton::section {
            background: #f8f9ff;
            border: none;
            border-bottom: 1px solid #eaedff;
            border-right: 1px solid #eaedff;
        }

        /* ── Scroll areas ─────────────────────────────────────────── */
        QScrollArea {
            border: none;
            background: transparent;
        }
        QScrollArea > QWidget > QWidget {
            background: transparent;
        }

        /* ── Login page ───────────────────────────────────────────── */
        QFrame#loginCard {
            background: #ffffff;
            border: 1px solid #eaedff;
            border-radius: 20px;
        }
        QLabel#loginLogo {
            background: #ffffff;
            border: 1px solid #eaedff;
            border-radius: 36px;
            color: #2563eb;
            font-size: 34px;
            font-weight: 700;
        }
        QLabel#loginTitle {
            color: #131b2e;
            font-size: 24px;
            font-weight: 700;
            background: transparent;
            border: none;
        }
        QLabel#loginTagline {
            color: #737686;
            font-size: 13px;
            background: transparent;
            border: none;
        }
        QLabel#errorLabel {
            color: #ba1a1a;
            font-size: 13px;
            background: transparent;
            border: none;
        }

        /* ── Trend label ──────────────────────────────────────────── */
        QLabel#trendLabel {
            color: #434655;
            font-size: 13px;
            background: transparent;
            border: none;
            line-height: 1.6;
        }
        QLabel#trendDate {
            color: #737686;
            font-size: 12px;
            background: transparent;
            border: none;
        }
        QLabel#trendRevenue {
            color: #131b2e;
            font-family: "Inter", "Consolas", "Microsoft YaHei";
            font-size: 14px;
            font-weight: 600;
            background: transparent;
            border: none;
        }

        /* ── Badge pills ──────────────────────────────────────────── */
        QLabel#badgeActive {
            background: #e2f6ec;
            color: #006c49;
            border-radius: 10px;
            font-size: 11px;
            font-weight: 700;
            padding: 3px 10px;
        }
        QLabel#badgeFrozen {
            background: #ffdad6;
            color: #93000a;
            border-radius: 10px;
            font-size: 11px;
            font-weight: 700;
            padding: 3px 10px;
        }
        QLabel#badgePrimary {
            background: #dbe1ff;
            color: #003ea8;
            border-radius: 10px;
            font-size: 11px;
            font-weight: 700;
            padding: 3px 10px;
        }
        QLabel#badgeIdle {
            background: #e2f6ec;
            color: #006c49;
            border-radius: 10px;
            font-size: 11px;
            font-weight: 700;
            padding: 3px 10px;
        }
        QLabel#badgeBusy {
            background: #fff1e6;
            color: #b45309;
            border-radius: 10px;
            font-size: 11px;
            font-weight: 700;
            padding: 3px 10px;
        }
        QLabel#badgeFault {
            background: #ffdad6;
            color: #93000a;
            border-radius: 10px;
            font-size: 11px;
            font-weight: 700;
            padding: 3px 10px;
        }

        /* ── Pile status card ─────────────────────────────────────── */
        QFrame#pileStatusCard {
            background: #ffffff;
            border: 1px solid #eaedff;
            border-radius: 16px;
        }
        QFrame#pileCardIdle {
            background: #eef9f2;
            border: 1px solid #a7f3d0;
            border-left: 4px solid #006c49;
            border-radius: 16px;
        }
        QFrame#pileCardBusy {
            background: #fff8ef;
            border: 1px solid #fde68a;
            border-left: 4px solid #d97706;
            border-radius: 16px;
        }
        QFrame#pileCardFault {
            background: #fff1f0;
            border: 1px solid #fecaca;
            border-left: 4px solid #ba1a1a;
            border-radius: 16px;
        }
        QLabel#countIdle {
            color: #006c49;
            font-family: "Inter", "Consolas", "Microsoft YaHei";
            font-size: 18px;
            font-weight: 700;
            background: transparent;
            border: none;
        }
        QLabel#countBusy {
            color: #d97706;
            font-family: "Inter", "Consolas", "Microsoft YaHei";
            font-size: 18px;
            font-weight: 700;
            background: transparent;
            border: none;
        }
        QLabel#countFault {
            color: #ba1a1a;
            font-family: "Inter", "Consolas", "Microsoft YaHei";
            font-size: 18px;
            font-weight: 700;
            background: transparent;
            border: none;
        }
    )");
}

