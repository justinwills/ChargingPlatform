#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

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

private:
    Ui::MainWindow *ui;

    QString phoneNumber;
};
#endif // MAINWINDOW_H
