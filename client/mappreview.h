#ifndef MAPPREVIEW_H
#define MAPPREVIEW_H

#include <QJsonArray>
#include <QWidget>

class MapPreview : public QWidget
{
    Q_OBJECT

public:
    explicit MapPreview(QWidget *parent = nullptr);

    void setStations(const QJsonArray &stations);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QJsonArray m_stations;
};

#endif // MAPPREVIEW_H