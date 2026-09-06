#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QJsonObject>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class ClientConnection;

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

    void on_BtnSetting_clicked();
    void on_BtnConfirm_PageEdit_clicked();
    void on_BtnCancel_clicked();
    void on_BtnChoosePhoto_clicked();

    void on_Btn_50_clicked();
    void on_Btn_100_clicked();
    void on_Btn_200_clicked();
    void on_BtnConfirm_pageRecharge_clicked();
    void on_BtnRecharge_clicked();
    void on_BtnCancel_pageRecharge_clicked();

    void onServerResponse(const QJsonObject &response);
    void onConnectionError(const QString &message);

private:
    Ui::MainWindow *ui;
    ClientConnection *m_connection;

    QString phoneNumber;
    QJsonObject m_currentUser;
};
#endif // MAINWINDOW_H
