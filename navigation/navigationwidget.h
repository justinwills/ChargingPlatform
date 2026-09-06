#ifndef NAVIGATIONWIDGET_H
#define NAVIGATIONWIDGET_H

#include <QWidget>
#include <QString>

class QWebEngineView;
class QPushButton;

// ===== 一键导航模块（后端） 对应《需求矩阵》NO.59-60 =====
// 说明：这里的"后端"指充电用户端内部的业务逻辑层（组装参数、拼URL、管理WebView），
// 不是PC服务器端。本模块直接对接腾讯地图API，不经过Socket/ChargingProtocol，
// 因此不依赖 db/、protocol/、server/ 里的任何代码。
//
// 使用方式（配合电站详情页/NO.18-19，充电站查询模块）：
//   NavigationWidget *nav = new NavigationWidget(this);
//   nav->startNavigation(myLat, myLng, "我的位置", stationLat, stationLng, "东软科技园充电站");
//   connect(nav, &NavigationWidget::navigationClosed, this, [=]{ /* 收起导航，回到详情页 */ });

class NavigationWidget : public QWidget
{
    Q_OBJECT
public:
    enum class TravelMode { Driving, Walking };

    explicit NavigationWidget(QWidget *parent = nullptr);
    ~NavigationWidget();

    // NO.59 点击导航发起请求：组装起点/终点/出行方式，加载腾讯地图路线规划页面
    void startNavigation(double fromLat, double fromLng, const QString &fromName,
                          double toLat, double toLng, const QString &toName,
                          TravelMode mode = TravelMode::Driving);

    // NO.60 切换出行方式重新规划：沿用当前起终点，换一种出行方式后重新加载
    void switchTravelMode(TravelMode newMode);

    TravelMode currentMode() const { return m_mode; }

signals:
    // 用户点击"关闭导航"时发出，外层（电站详情页）收到后应收起本控件、恢复详情页显示
    void navigationClosed();

private:
    QString buildRoutePlanUrl() const;
    void reloadRoute();

    QWebEngineView *m_webView;
    QPushButton *m_closeButton;

    double m_fromLat = 0.0;
    double m_fromLng = 0.0;
    QString m_fromName;
    double m_toLat = 0.0;
    double m_toLng = 0.0;
    QString m_toName;
    TravelMode m_mode = TravelMode::Driving;
    bool m_hasRoute = false;

    // TODO(团队): 换成实际申请到的腾讯位置服务Key
    // 申请地址: https://lbs.qq.com -> 控制台 -> 应用管理 -> 我的应用
    // 这个Key目前是"共享导航模块"和"充电站查询模块"(地址解析)两边都要用的，
    // 申请后请同步到王清香/薛学刚。
    static const QString TENCENT_MAP_KEY;
};

#endif // NAVIGATIONWIDGET_H
