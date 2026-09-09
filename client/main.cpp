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

    MainWindow w;
    w.show();
    return QApplication::exec();
}
