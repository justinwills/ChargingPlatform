#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QTimer>
#include "clientconnection.h"
#include "paymentpreview.h"

class QAbstractButton;
class NavigationPage;

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

    void on_BtnPileDetail_clicked();

    void on_BtnLoadOrderStation_clicked();

    void on_BtnRefreshOrder_clicked();

    void on_BtnSettleOrder_clicked();

    void on_BtnSearchStations_clicked();

    void on_BtnStationBack_clicked();
    void on_BtnStartChargeHere_clicked();
    void on_BtnNavigateHere_clicked();

    void on_BtnAdmin_clicked();
    void on_BtnSetting_clicked();
    void on_BtnChoosePhoto_clicked();
    void on_BtnConfirm_PageEdit_clicked();
    void on_BtnCancel_clicked();
    void on_BtnRecharge_clicked();

    void on_Btn_20_clicked();
    void on_Btn_50_clicked();
    void on_Btn_100_clicked();
    void on_Btn_200_clicked();
    void on_BtnConfirm_pageRecharge_clicked();
    void on_BtnCancel_pageRecharge_clicked();
    void on_BtnLeave_clicked();

    void onServerResponse(const QJsonObject &response);

    void onConnectionError(const QString &message);

    void showPaymentPreview();

private:
    void applyShadow(QWidget *widget);
    void openChargePage();
    void populateStationDetail(const QJsonObject &data);
    void rebuildNearbyCards(const QJsonArray &stations);
    void updateBalanceLabels(double balance);
    QString formatRemainingChargeTime(int seconds) const;
    int selectPileInCombo(int pileId);
    void setupNavIcons();
    void applyNavIcon(QAbstractButton *button, bool checked, const QString &base);
    void showNavigationPage();
    void leaveNavigationPage();
    void requestNavigation(const QString &mode, const QString &originAddress = QString());

    Ui::MainWindow *ui;

    ClientConnection *connection;
    QTimer orderTimer;
    QTimer displayTimer;
    int userId = -1;
    int activeOrderId = -1;
    double currentAmount = 0;
    double currentFee = 0;
    QDateTime activeOrderStartTime;

    QString phoneNumber;
    QString m_selectedAvatarPath;
    QJsonObject m_currentUser;
    QJsonObject m_lastOrder;
    QJsonArray m_lastStations;
    QString m_pendingAction;
    PaymentPreview *m_paymentPreview = nullptr;
    NavigationPage *m_navigationPage = nullptr;

    int m_lastStationId = -1;
    double m_lastStationLat = 0.0;
    double m_lastStationLng = 0.0;
    QString m_lastStationName;
    QString m_lastStationAddress;
    bool m_navigationStarted = false;
    bool m_hasNavigationOrigin = false;
    double m_navigationOriginLat = 0.0;
    double m_navigationOriginLng = 0.0;
    bool m_showStationDetailPage = false;
    bool m_chargePagePending = false;
    int m_pendingPileId = -1;
    int m_settlingOrderId = -1;
    double m_balanceBeforeSettlement = -1.0;
    bool m_backgroundBalanceRefresh = false;
};
#endif // MAINWINDOW_H
