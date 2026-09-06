#include "navigationwidget.h"

#include <QWebEngineView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QUrl>
#include <QUrlQuery>
#include <QDebug>

// ⚠️ 占位符，还没申请到真的Key之前先放这个，编译能过但实际加载会失败
const QString NavigationWidget::TENCENT_MAP_KEY = "CGMBZ-ZJTLQ-PNV5Y-2SKEJ-V2DX7-3BBHZ";

NavigationWidget::NavigationWidget(QWidget *parent)
    : QWidget(parent)
    , m_webView(new QWebEngineView(this))
    , m_closeButton(new QPushButton(QStringLiteral("关闭导航"), this))
{
    auto *topBar = new QHBoxLayout();
    auto *modeHint = new QLabel(QStringLiteral("提示：可在地图页面内切换驾车/步行"), this);
    modeHint->setStyleSheet("color: #888;");
    topBar->addWidget(modeHint);
    topBar->addStretch();
    topBar->addWidget(m_closeButton);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(topBar);
    mainLayout->addWidget(m_webView);
    setLayout(mainLayout);

    connect(m_closeButton, &QPushButton::clicked, this, [this]() {
        // NO.60描述里提到"关闭导航返回详情"——本控件不负责切页面，
        // 只发信号，切页面的逻辑交给持有本控件的电站详情页处理
        emit navigationClosed();
    });
}

NavigationWidget::~NavigationWidget() = default;

void NavigationWidget::startNavigation(double fromLat, double fromLng, const QString &fromName,
                                        double toLat, double toLng, const QString &toName,
                                        TravelMode mode)
{
    m_fromLat = fromLat;
    m_fromLng = fromLng;
    m_fromName = fromName;
    m_toLat = toLat;
    m_toLng = toLng;
    m_toName = toName;
    m_mode = mode;
    m_hasRoute = true;
    reloadRoute();
}

void NavigationWidget::switchTravelMode(TravelMode newMode)
{
    if (!m_hasRoute) {
        qWarning() << "switchTravelMode被调用，但还没调用过startNavigation，忽略";
        return;
    }
    m_mode = newMode;
    reloadRoute();
}

QString NavigationWidget::buildRoutePlanUrl() const
{
    // 腾讯位置服务 Web URI API - 路线规划
    // 文档: https://lbs.qq.com/webApi/uriV1/uriGuide/uriMobileRoute
    // 坐标格式统一是"纬度,经度"，跟经纬度日常说法顺序相反，容易搞反要注意
    QUrl url(QStringLiteral("https://apis.map.qq.com/uri/v1/routeplan"));
    QUrlQuery query;
    query.addQueryItem("type", m_mode == TravelMode::Driving ? "drive" : "walk");
    query.addQueryItem("from", m_fromName);
    query.addQueryItem("fromcoord", QStringLiteral("%1,%2").arg(m_fromLat, 0, 'f', 6).arg(m_fromLng, 0, 'f', 6));
    query.addQueryItem("to", m_toName);
    query.addQueryItem("tocoord", QStringLiteral("%1,%2").arg(m_toLat, 0, 'f', 6).arg(m_toLng, 0, 'f', 6));
    query.addQueryItem("referer", TENCENT_MAP_KEY);
    url.setQuery(query);
    return url.toString();
}

void NavigationWidget::reloadRoute()
{
    const QString url = buildRoutePlanUrl();
    qDebug() << "[Navigation] 加载路线:" << url;
    m_webView->load(QUrl(url));
}
