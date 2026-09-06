#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QDateTime>
#include <QTimer>
#include "clientconnection.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void on_BtnHome_clicked();

    void on_BtnCharge_clicked();

    void on_BtnMine_clicked();

    void on_Btnlogin_clicked();

    void on_BtnStartCharging_clicked();

    void on_BtnRefreshOrder_clicked();

    void on_BtnSettleOrder_clicked();

    void onServerResponse(const QJsonObject &response);

    void onConnectionError(const QString &message);

private:
    Ui::MainWindow *ui;

    ClientConnection *connection;
    QTimer orderTimer;
    QTimer displayTimer;
    int userId = -1;
    int activeOrderId = -1;
    QDateTime activeOrderStartTime;
    QString phoneNumber;
};
#endif // MAINWINDOW_H
