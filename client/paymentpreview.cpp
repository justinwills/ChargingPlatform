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
        QStringLiteral("QDialog { background: #faf8ff; }"
                       "QLabel { color: #434655; font-family: \"Inter\",\"Microsoft YaHei\"; font-size: 14px; }"
                       "QPushButton { padding: 12px 20px; border-radius: 14px; "
                       "background: #004ac6; color: #ffffff; font-size: 15px; font-weight: 600; "
                       "border: none; }"
                       "QPushButton:hover { background: #003ea8; }"
                       "QPushButton:pressed { background: #003ea8; }"
                       "QPushButton:disabled { background: #e2e7ff; color: #737686; }"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 28, 24, 24);
    layout->setSpacing(16);
    auto *title = new QLabel(tr("充电订单 · 支付结算"), this);
    title->setStyleSheet(QStringLiteral("color: #131b2e; font-size: 24px; font-weight: 700;"));
    layout->addWidget(title);

    auto *statusBadge = new QLabel(tr("● 待结算"), this);
    statusBadge->setAlignment(Qt::AlignCenter);
    statusBadge->setStyleSheet(QStringLiteral("background: #ffddb8; color: #653e00;"
                                              " border-radius: 10px; font-size: 11px;"
                                              " font-weight: 700; padding: 3px 10px; max-width: 90px;"));
    layout->addWidget(statusBadge, 0, Qt::AlignLeft);

    pages = new QStackedWidget(this);
    layout->addWidget(pages, 1);

    auto *confirmPage = new QWidget(pages);
    auto *card = new QWidget(confirmPage);
    card->setStyleSheet(QStringLiteral("QWidget { background: #ffffff; border: 1px solid #eaedff;"
                                       " border-radius: 24px; }"));
    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(24, 28, 24, 28);
    cardLayout->setSpacing(14);
    auto *form = new QFormLayout;
    form->setHorizontalSpacing(16);
    form->setVerticalSpacing(10);
    form->addRow(tr("订单号"), new QLabel(QString::number(orderId), confirmPage));
    form->addRow(tr("充电量"), new QLabel(tr("%1 度").arg(amount, 0, 'f', 2), confirmPage));
    form->addRow(tr("充电费用"), new QLabel(tr("%1 元").arg(fee, 0, 'f', 2), confirmPage));
    form->addRow(tr("预计支付"), new QLabel(tr("%1 元").arg(fee, 0, 'f', 2), confirmPage));
    form->addRow(tr("当前余额"), new QLabel(tr("%1 元").arg(balance, 0, 'f', 2), confirmPage));
    cardLayout->addLayout(form);

    auto *confirmHint = new QLabel(
        balance >= fee ? tr("余额充足，可以结束充电并完成支付。")
                       : tr("余额不足，请先充值后再结束充电。"),
        confirmPage);
    confirmHint->setWordWrap(true);
    confirmHint->setStyleSheet(balance >= fee
        ? QStringLiteral("color: #006c49;")
        : QStringLiteral("color: #ba1a1a;"));
    cardLayout->addWidget(confirmHint);

    auto *confirmButtons = new QHBoxLayout;
    auto *cancel = new QPushButton(tr("返回订单"), confirmPage);
    cancel->setStyleSheet(QStringLiteral("background: #e2e7ff; color: #131b2e;"
                                         " border: none;"));
    auto *confirm = new QPushButton(tr("确认支付"), confirmPage);
    confirm->setStyleSheet(QStringLiteral("background: #004ac6; color: #ffffff;"
                                          " border: none;"));
    confirmButtons->addWidget(cancel);
    confirmButtons->addStretch();
    confirmButtons->addWidget(confirm);
    cardLayout->addLayout(confirmButtons);
    auto *confirmLayout = new QVBoxLayout(confirmPage);
    confirmLayout->addWidget(card);
    confirmLayout->addStretch();
    pages->addWidget(confirmPage);

    auto *failurePage = new QWidget(pages);
    auto *failureLayout = new QVBoxLayout(failurePage);
    auto *failureTitle = new QLabel(tr("余额不足，请先充值"), failurePage);
    failureTitle->setStyleSheet(QStringLiteral("color: #ba1a1a; font-size: 24px; font-weight: 700;"));
    auto *failureText = new QLabel(
        tr("预计支付：%1 元\n当前余额：%2 元\n还差：%3 元")
            .arg(fee, 0, 'f', 2)
            .arg(balance, 0, 'f', 2)
            .arg(qMax(0.0, fee - balance), 0, 'f', 2),
        failurePage);
    failureText->setWordWrap(true);
    failureText->setStyleSheet(QStringLiteral("line-height: 140%;"));
    auto *recharge = new QPushButton(tr("去充值"), failurePage);
    failureLayout->addWidget(failureTitle);
    failureLayout->addWidget(failureText);
    failureLayout->addStretch();
    failureLayout->addWidget(recharge);
    pages->addWidget(failurePage);

    auto *successPage = new QWidget(pages);
    auto *successLayout = new QVBoxLayout(successPage);
    auto *successTitle = new QLabel(tr("支付成功"), successPage);
    successTitle->setStyleSheet(QStringLiteral("color: #006c49; font-size: 24px; font-weight: 700;"));
    successText = new QLabel(successPage);
    successText->setWordWrap(true);
    auto *close = new QPushButton(tr("完成"), successPage);
    close->setStyleSheet(QStringLiteral("background: #2563eb; color: #ffffff;"));
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
