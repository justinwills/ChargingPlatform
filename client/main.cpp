#include "mainwindow.h"

#include <QApplication>
#include <QFont>
#include <QPalette>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setStyle(QStringLiteral("Fusion"));
    QFont appFont(QStringLiteral("Inter"));
    appFont.setStyleHint(QFont::SansSerif);
    a.setFont(appFont);

    QPalette pal = a.palette();
    pal.setColor(QPalette::Window, QColor("#faf8ff"));
    pal.setColor(QPalette::WindowText, QColor("#131b2e"));
    pal.setColor(QPalette::Base, QColor("#ffffff"));
    pal.setColor(QPalette::Text, QColor("#434655"));
    pal.setColor(QPalette::Button, QColor("#2563eb"));
    pal.setColor(QPalette::ButtonText, QColor("#ffffff"));
    a.setPalette(pal);
    a.setStyleSheet(QStringLiteral(
        "QMessageBox {"
        "  background: #ffffff;"
        "  border: 1px solid #dfe5ff;"
        "  border-radius: 20px;"
        "  color: #131b2e;"
        "}"
        "QMessageBox QLabel {"
        "  color: #131b2e;"
        "  font-family: Inter, 'Segoe UI';"
        "  font-size: 16px;"
        "  padding: 8px 12px;"
        "}"
        "QMessageBox QPushButton {"
        "  background: #2563eb;"
        "  color: #ffffff;"
        "  border: none;"
        "  border-radius: 12px;"
        "  min-width: 96px;"
        "  min-height: 38px;"
        "  padding: 0 18px;"
        "  font-family: Inter, 'Segoe UI';"
        "  font-size: 15px;"
        "  font-weight: 600;"
        "}"
        "QMessageBox QPushButton:hover {"
        "  background: #1d4ed8;"
        "}"
        "QMessageBox QPushButton:pressed {"
        "  background: #1e40af;"
        "}"
    ));

    MainWindow w;
    w.show();
    return QApplication::exec();
}
