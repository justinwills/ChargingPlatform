#ifndef CHARGERING_H
#define CHARGERING_H

#include <QWidget>

class ChargeRing : public QWidget
{
    Q_OBJECT

public:
    explicit ChargeRing(QWidget *parent = nullptr);

    void setValue(int percent);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    int m_percent = 68;
};

#endif // CHARGERING_H