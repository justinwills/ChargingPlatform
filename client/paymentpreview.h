#ifndef PAYMENTPREVIEW_H
#define PAYMENTPREVIEW_H

#include <QDialog>

class QLabel;
class QStackedWidget;

class PaymentPreview : public QDialog
{
    Q_OBJECT

public:
    PaymentPreview(int orderId, double amount, double fee, double balance,
                   QWidget *parent = nullptr);

signals:
    void paymentConfirmed();
    void rechargeRequested();

public slots:
    void showPaymentSuccess();

private:
    QStackedWidget *pages = nullptr;
    QLabel *successText = nullptr;
    double paymentFee = 0;
};

#endif // PAYMENTPREVIEW_H
