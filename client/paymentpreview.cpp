#include "paymentpreview.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

PaymentPreview::PaymentPreview(int orderId, double amount, double fee, double balance,
                               QWidget *parent)
    : QDialog(parent)
    , paymentFee(fee)
{
    setWindowTitle(tr("支付结算"));
    setFixedSize(360, 612);
    setModal(true);
    setStyleSheet(
        QStringLiteral("QDialog { background: #f5f7fb; }"
                       "QLabel { color: #263449; font-size: 14px; }"
                       "QPushButton { padding: 9px 18px; border-radius: 5px; "
                       "background: #2563eb; color: white; }"
                       "QPushButton:disabled { background: #94a3b8; }"));

    auto *layout = new QVBoxLayout(this);
    auto *title = new QLabel(tr("充电订单 · 支付结算"), this);
    title->setStyleSheet(QStringLiteral("font-size: 23px; font-weight: bold;"));
    layout->addWidget(title);

    pages = new QStackedWidget(this);
    layout->addWidget(pages, 1);

    auto *confirmPage = new QWidget(pages);
    auto *form = new QFormLayout(confirmPage);
    form->addRow(tr("订单号"), new QLabel(QString::number(orderId), confirmPage));
    form->addRow(tr("充电量"), new QLabel(tr("%1 度").arg(amount, 0, 'f', 2), confirmPage));
    form->addRow(tr("充电费用"), new QLabel(tr("%1 元").arg(fee, 0, 'f', 2), confirmPage));
    form->addRow(tr("预计支付"), new QLabel(tr("%1 元").arg(fee, 0, 'f', 2), confirmPage));
    form->addRow(tr("当前余额"), new QLabel(tr("%1 元").arg(balance, 0, 'f', 2), confirmPage));

    auto *confirmHint = new QLabel(
        balance >= fee ? tr("余额充足，可以结束充电并完成支付。")
                       : tr("余额不足，请先充值后再结束充电。"),
        confirmPage);
    confirmHint->setWordWrap(true);
    form->addRow(confirmHint);

    auto *confirmButtons = new QHBoxLayout;
    auto *confirm = new QPushButton(tr("确认支付"), confirmPage);
    auto *cancel = new QPushButton(tr("返回订单"), confirmPage);
    confirmButtons->addWidget(cancel);
    confirmButtons->addStretch();
    confirmButtons->addWidget(confirm);
    form->addRow(confirmButtons);
    pages->addWidget(confirmPage);

    auto *failurePage = new QWidget(pages);
    auto *failureLayout = new QVBoxLayout(failurePage);
    auto *failureTitle = new QLabel(tr("余额不足，请先充值"), failurePage);
    failureTitle->setStyleSheet(QStringLiteral("color: #c2410c; font-size: 24px; font-weight: bold;"));
    auto *failureText = new QLabel(
        tr("预计支付：%1 元\n当前余额：%2 元\n还差：%3 元")
            .arg(fee, 0, 'f', 2)
            .arg(balance, 0, 'f', 2)
            .arg(qMax(0.0, fee - balance), 0, 'f', 2),
        failurePage);
    failureText->setWordWrap(true);
    auto *recharge = new QPushButton(tr("去充值"), failurePage);
    failureLayout->addWidget(failureTitle);
    failureLayout->addWidget(failureText);
    failureLayout->addWidget(recharge);
    failureLayout->addStretch();
    pages->addWidget(failurePage);

    auto *successPage = new QWidget(pages);
    auto *successLayout = new QVBoxLayout(successPage);
    auto *successTitle = new QLabel(tr("支付成功"), successPage);
    successTitle->setStyleSheet(QStringLiteral("color: #16803c; font-size: 24px; font-weight: bold;"));
    successText = new QLabel(successPage);
    successText->setWordWrap(true);
    auto *close = new QPushButton(tr("完成"), successPage);
    successLayout->addWidget(successTitle);
    successLayout->addWidget(successText);
    successLayout->addStretch();
    successLayout->addWidget(close);
    pages->addWidget(successPage);

    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(recharge, &QPushButton::clicked, this, [this]() {
        emit rechargeRequested();
        reject();
    });
    connect(close, &QPushButton::clicked, this, &QDialog::accept);

    if (balance < fee) {
        confirm->setEnabled(false);
        pages->setCurrentWidget(failurePage);
    } else {
        connect(confirm, &QPushButton::clicked, this, [this, confirm]() {
            emit paymentConfirmed();
            confirm->setEnabled(false);
            showPaymentSuccess();
            QTimer::singleShot(700, this, [this]() {
                accept();
            });
        });
    }
}

void PaymentPreview::showPaymentSuccess()
{
    if (successText) {
        successText->setText(tr("订单已完成结算，已支付 %1 元。")
                              .arg(paymentFee, 0, 'f', 2));
    }
    pages->setCurrentIndex(2);
}
