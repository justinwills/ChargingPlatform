#ifndef ADMINWINDOW_H
#define ADMINWINDOW_H

#include <QMainWindow>
#include "../clientconnection.h"

class QLineEdit;
class QLabel;
class QTableWidget;
class QStackedWidget;

class AdminWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit AdminWindow(QWidget *parent = nullptr);

private slots:
    void login();
    void handleResponse(const QJsonObject &response);
    void handleError(const QString &message);
    void refreshUsers();
    void toggleSelectedUser();
    void refreshStationsAndPiles();
    void restartSelectedPile();
    void refreshStats();

private:
    void send(const QString &action, const QJsonObject &params = {});
    void requestInitialData();
    void showError(const QString &message);

    ClientConnection *connection;
    QStackedWidget *pages;
    QLineEdit *usernameEdit;
    QLineEdit *passwordEdit;
    QLabel *loginStatus;
    QLineEdit *userFilterEdit;
    QTableWidget *usersTable;
    QTableWidget *stationsTable;
    QTableWidget *pilesTable;
    QLabel *statsLabel;
    QString pendingAction;
};

#endif
