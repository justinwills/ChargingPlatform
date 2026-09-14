#include "navigationpage.h"

#include <QButtonGroup>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTextBrowser>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>

#ifdef CHARGING_HAS_WEBENGINE
#include <QWebEnginePage>
#include <QWebEngineSettings>
#include <QWebEngineView>
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
#include <QWebEnginePermission>
#endif
#include <functional>

namespace {

class NavigationWebPage final : public QWebEnginePage
{
public:
    explicit NavigationWebPage(QObject *parent = nullptr)
        : QWebEnginePage(parent)
    {
    }

    std::function<void(double, double)> locationReceived;
    std::function<void(const QString &)> locationFailed;

protected:
    void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level,
                                  const QString &message,
                                  int lineNumber,
                                  const QString &sourceId) override
    {
        Q_UNUSED(level)
        Q_UNUSED(lineNumber)
        Q_UNUSED(sourceId)

        const QString locationPrefix = QStringLiteral("CHARGING_GEO:");
        if (message.startsWith(locationPrefix)) {
            const QStringList coordinates = message.mid(locationPrefix.size()).split(',');
            bool latitudeOk = false;
            bool longitudeOk = false;
            const double latitude = coordinates.value(0).toDouble(&latitudeOk);
            const double longitude = coordinates.value(1).toDouble(&longitudeOk);
            if (latitudeOk && longitudeOk && locationReceived) {
                locationReceived(latitude, longitude);
            }
            return;
        }

        const QString errorPrefix = QStringLiteral("CHARGING_GEO_ERROR:");
        if (message.startsWith(errorPrefix) && locationFailed) {
            locationFailed(message.mid(errorPrefix.size()));
        }
    }
};

} // namespace
#endif

NavigationPage::NavigationPage(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("navigationPage"));
    buildUi();
}

void NavigationPage::buildUi()
{
    setStyleSheet(QStringLiteral(R"(
        QWidget#navigationPage { background: #faf8ff; }
        QFrame#navModeCard { background: #e8ebff; border: none; border-radius: 20px; }
        QPushButton#navModeButton {
            background: transparent; color: #4d5162; border: none; border-radius: 17px;
            font-size: 14px; font-weight: 600; padding: 7px 12px;
        }
        QPushButton#navModeButton:checked { background: #ffffff; color: #0752ce; }
        QFrame#navLocationCard, QFrame#navMapCard, QFrame#navSummaryCard {
            background: #ffffff; border: 1px solid #e8ecf5; border-radius: 16px;
        }
        QLineEdit#navOriginEdit {
            background: transparent; border: none; border-radius: 0; padding: 0;
            color: #182033; font-size: 13px;
        }
        QPushButton#navLocateButton {
            background: #edf3ff; color: #0752ce; border: none; border-radius: 10px;
            font-size: 11px; font-weight: 700; padding: 6px 9px;
        }
        QLabel#navDestination { color: #182033; font-size: 13px; }
        QLabel#navStatus { color: #737686; font-size: 11px; }
        QLabel#navDuration { color: #0752ce; font-size: 22px; font-weight: 800; }
        QLabel#navEta, QLabel#navDistance { color: #4d5162; font-size: 12px; }
        QLabel#navInstruction { color: #737686; font-size: 11px; }
        QPushButton#navPlanButton {
            background: #0752ce; color: white; border: none; border-radius: 14px;
            font-size: 14px; font-weight: 700; padding: 10px 14px;
        }
        QPushButton#navPlanButton:hover { background: #004ac6; }
        QPushButton#navEndButton {
            background: #c91b1b; color: white; border: none; border-radius: 14px;
            font-size: 14px; font-weight: 700; padding: 10px 14px;
        }
        QPushButton#navEndButton:hover { background: #a90f16; }
        QPushButton#navBackButton {
            background: #e8ebff; color: #182033; border: none; border-radius: 12px;
            font-size: 20px; font-weight: 700;
        }
    )"));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(16, 10, 16, 12);
    root->setSpacing(8);

    auto *header = new QHBoxLayout;
    header->setSpacing(10);
    auto *backButton = new QPushButton(QStringLiteral("‹"), this);
    backButton->setObjectName(QStringLiteral("navBackButton"));
    backButton->setFixedSize(40, 38);
    backButton->setCursor(Qt::PointingHandCursor);
    connect(backButton, &QPushButton::clicked, this, &NavigationPage::backRequested);
    header->addWidget(backButton);

    auto *title = new QLabel(tr("路线导航"), this);
    title->setStyleSheet(QStringLiteral(
        "color:#131b2e;font-size:20px;font-weight:800;background:transparent;"));
    header->addWidget(title, 1);
    m_statusLabel = new QLabel(tr("准备规划"), this);
    m_statusLabel->setObjectName(QStringLiteral("navStatus"));
    m_statusLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    header->addWidget(m_statusLabel);
    root->addLayout(header);

    auto *modeCard = new QFrame(this);
    modeCard->setObjectName(QStringLiteral("navModeCard"));
    modeCard->setFixedHeight(42);
    auto *modeLayout = new QHBoxLayout(modeCard);
    modeLayout->setContentsMargins(4, 4, 4, 4);
    modeLayout->setSpacing(4);
    m_driveButton = new QPushButton(tr("🚗  驾车（推荐）"), modeCard);
    m_walkButton = new QPushButton(tr("🚶  步行"), modeCard);
    for (QPushButton *button : {m_driveButton, m_walkButton}) {
        button->setObjectName(QStringLiteral("navModeButton"));
        button->setCheckable(true);
        button->setCursor(Qt::PointingHandCursor);
        modeLayout->addWidget(button, 1);
    }
    m_modeGroup = new QButtonGroup(this);
    m_modeGroup->setExclusive(true);
    m_modeGroup->addButton(m_driveButton);
    m_modeGroup->addButton(m_walkButton);
    m_driveButton->setChecked(true);
    connect(m_driveButton, &QPushButton::clicked, this, [this]() {
        setActiveMode(QStringLiteral("driving"));
        emit modeChanged(m_mode);
    });
    connect(m_walkButton, &QPushButton::clicked, this, [this]() {
        setActiveMode(QStringLiteral("walking"));
        emit modeChanged(m_mode);
    });
    root->addWidget(modeCard);

    auto *locationCard = new QFrame(this);
    locationCard->setObjectName(QStringLiteral("navLocationCard"));
    auto *locationLayout = new QVBoxLayout(locationCard);
    locationLayout->setContentsMargins(12, 7, 10, 7);
    locationLayout->setSpacing(4);

    auto *originRow = new QHBoxLayout;
    originRow->setSpacing(7);
    auto *originDot = new QLabel(QStringLiteral("●"), locationCard);
    originDot->setStyleSheet(QStringLiteral("color:#167d55;font-size:14px;"));
    originRow->addWidget(originDot);
    m_originEdit = new QLineEdit(locationCard);
    m_originEdit->setObjectName(QStringLiteral("navOriginEdit"));
    m_originEdit->setPlaceholderText(tr("当前位置（也可输入详细起点）"));
    originRow->addWidget(m_originEdit, 1);
    auto *locateButton = new QPushButton(tr("重新定位"), locationCard);
    locateButton->setObjectName(QStringLiteral("navLocateButton"));
    locateButton->setCursor(Qt::PointingHandCursor);
    connect(locateButton, &QPushButton::clicked,
            this, &NavigationPage::requestCurrentLocation);
    originRow->addWidget(locateButton);
    locationLayout->addLayout(originRow);

    auto *divider = new QFrame(locationCard);
    divider->setFrameShape(QFrame::HLine);
    divider->setStyleSheet(QStringLiteral("color:#edf0f7;"));
    locationLayout->addWidget(divider);

    auto *destinationRow = new QHBoxLayout;
    destinationRow->setSpacing(7);
    auto *destinationDot = new QLabel(QStringLiteral("●"), locationCard);
    destinationDot->setStyleSheet(QStringLiteral("color:#0752ce;font-size:14px;"));
    destinationRow->addWidget(destinationDot);
    m_destinationLabel = new QLabel(tr("目标充电站"), locationCard);
    m_destinationLabel->setObjectName(QStringLiteral("navDestination"));
    m_destinationLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    destinationRow->addWidget(m_destinationLabel, 1);
    locationLayout->addLayout(destinationRow);
    root->addWidget(locationCard);

    auto *mapCard = new QFrame(this);
    mapCard->setObjectName(QStringLiteral("navMapCard"));
    auto *mapLayout = new QVBoxLayout(mapCard);
    mapLayout->setContentsMargins(1, 1, 1, 1);
#ifdef CHARGING_HAS_WEBENGINE
    m_webView = new QWebEngineView(mapCard);
    auto *webPage = new NavigationWebPage(m_webView);
    webPage->locationReceived = [this](double latitude, double longitude) {
        m_originEdit->clear();
        m_originEdit->setPlaceholderText(tr("当前位置 · %1, %2")
                                             .arg(latitude, 0, 'f', 5)
                                             .arg(longitude, 0, 'f', 5));
        emit currentLocationResolved(latitude, longitude);
    };
    webPage->locationFailed = [this](const QString &message) {
        showError(tr("自动定位不可用，请输入起点（%1）").arg(message));
        loadTencentRoute();
    };
    m_webView->setPage(webPage);
    m_webView->settings()->setAttribute(
        QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    connect(webPage, &QWebEnginePage::permissionRequested, this,
            [](QWebEnginePermission permission) {
        if (permission.permissionType()
            == QWebEnginePermission::PermissionType::Geolocation) {
            permission.grant();
        }
    });
#else
    connect(webPage, &QWebEnginePage::featurePermissionRequested, this,
            [webPage](const QUrl &securityOrigin, QWebEnginePage::Feature feature) {
        if (feature == QWebEnginePage::Geolocation) {
            webPage->setFeaturePermission(
                securityOrigin, feature, QWebEnginePage::PermissionGrantedByUser);
        }
    });
#endif
#else
    m_webView = new QTextBrowser(mapCard);
    m_webView->setOpenExternalLinks(true);
    m_webView->setFrameShape(QFrame::NoFrame);
    m_webView->setStyleSheet(QStringLiteral(
        "background:#edf2ff;color:#4d5162;border-radius:15px;padding:18px;"));
#endif
    mapLayout->addWidget(m_webView);
    root->addWidget(mapCard, 1);

    auto *summaryCard = new QFrame(this);
    summaryCard->setObjectName(QStringLiteral("navSummaryCard"));
    auto *summaryLayout = new QVBoxLayout(summaryCard);
    summaryLayout->setContentsMargins(14, 8, 14, 8);
    summaryLayout->setSpacing(2);
    auto *summaryTop = new QHBoxLayout;
    summaryTop->setSpacing(8);
    m_durationLabel = new QLabel(tr("-- 分钟"), summaryCard);
    m_durationLabel->setObjectName(QStringLiteral("navDuration"));
    m_durationLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    summaryTop->addWidget(m_durationLabel);

    auto *etaDistanceColumn = new QVBoxLayout;
    etaDistanceColumn->setSpacing(0);
    m_etaLabel = new QLabel(tr("等待路线规划"), summaryCard);
    m_etaLabel->setObjectName(QStringLiteral("navEta"));
    etaDistanceColumn->addWidget(m_etaLabel);
    m_distanceLabel = new QLabel(tr("剩余距离 -- km"), summaryCard);
    m_distanceLabel->setObjectName(QStringLiteral("navDistance"));
    etaDistanceColumn->addWidget(m_distanceLabel);
    summaryTop->addLayout(etaDistanceColumn, 1);
    summaryLayout->addLayout(summaryTop);
    m_instructionLabel = new QLabel(tr("腾讯地图将根据实时位置规划路线"), summaryCard);
    m_instructionLabel->setObjectName(QStringLiteral("navInstruction"));
    m_instructionLabel->setWordWrap(true);
    summaryLayout->addWidget(m_instructionLabel);
    root->addWidget(summaryCard);

    auto *actions = new QHBoxLayout;
    actions->setSpacing(8);
    m_planButton = new QPushButton(tr("开始导航"), this);
    m_planButton->setObjectName(QStringLiteral("navPlanButton"));
    m_planButton->setCursor(Qt::PointingHandCursor);
    connect(m_planButton, &QPushButton::clicked, this, [this]() {
        emit planRequested(originAddress(), m_mode);
    });
    actions->addWidget(m_planButton, 1);
    m_endButton = new QPushButton(tr("× 结束导航"), this);
    m_endButton->setObjectName(QStringLiteral("navEndButton"));
    m_endButton->setCursor(Qt::PointingHandCursor);
    m_endButton->hide();
    connect(m_endButton, &QPushButton::clicked, this, &NavigationPage::endRequested);
    actions->addWidget(m_endButton, 1);
    root->addLayout(actions);
}

void NavigationPage::setDestination(int stationId, const QString &name,
                                    const QString &address, double latitude,
                                    double longitude)
{
    m_stationId = stationId;
    m_stationName = name;
    m_stationAddress = address;
    m_stationLatitude = latitude;
    m_stationLongitude = longitude;
    m_destinationLabel->setText(name);
    m_destinationLabel->setToolTip(address);
    resetNavigation();
}

void NavigationPage::setOriginHint(const QString &address)
{
    m_originEdit->setText(address.trimmed());
}

void NavigationPage::requestCurrentLocation()
{
    m_originEdit->clear();
    m_statusLabel->setStyleSheet(QString());
    m_statusLabel->setText(tr("正在获取当前位置…"));
#ifdef CHARGING_HAS_WEBENGINE
    const QString station = m_stationName.toHtmlEscaped();
    const QString html = QStringLiteral(R"(
<!doctype html><html><head><meta charset="utf-8"><style>
html,body{height:100%%;margin:0;font-family:"Microsoft YaHei",sans-serif;background:#edf2ff}
.wrap{height:100%%;display:flex;align-items:center;justify-content:center;text-align:center;color:#31517f}
.pin{font-size:42px}.title{font-size:16px;font-weight:700;margin-top:7px}.sub{font-size:12px;color:#71809b;margin-top:5px}
</style></head><body><div class="wrap"><div><div class="pin">⌖</div><div class="title">正在定位您的位置</div>
<div class="sub">即将规划前往 %1 的路线</div></div></div><script>
if (!navigator.geolocation) { console.log('CHARGING_GEO_ERROR:当前环境不支持定位'); }
else navigator.geolocation.getCurrentPosition(function(p) {
 console.log('CHARGING_GEO:' + p.coords.latitude + ',' + p.coords.longitude);
}, function(e) { console.log('CHARGING_GEO_ERROR:' + e.message); },
{enableHighAccuracy:true,timeout:10000,maximumAge:30000});
</script></body></html>)").arg(station);
    m_webView->setHtml(html, QUrl(QStringLiteral("https://charging.local/navigation")));
#else
    m_statusLabel->setText(tr("请输入起点后规划"));
    loadTencentRoute();
#endif
}

void NavigationPage::showRoute(const QJsonObject &data)
{
    setBusy(false);
    m_active = true;
    setActiveMode(data.value(QStringLiteral("mode")).toString(m_mode));

    const QJsonObject distance = data.value(QStringLiteral("distance")).toObject();
    const QJsonObject duration = data.value(QStringLiteral("duration")).toObject();
    const int minutes = duration.value(QStringLiteral("minutes")).toInt();
    const double kilometers = distance.value(QStringLiteral("km")).toDouble();
    m_durationLabel->setText(tr("%1 分钟").arg(minutes));
    m_distanceLabel->setText(tr("剩余距离 %1 km").arg(kilometers, 0, 'f', 1));

    const QString eta = data.value(QStringLiteral("eta")).toString();
    m_etaLabel->setText(eta.isEmpty() ? tr("路线已规划") : tr("预计 %1 到达").arg(eta.right(8)));

    const QJsonArray steps = data.value(QStringLiteral("steps")).toArray();
    QString instruction;
    if (!steps.isEmpty()) {
        instruction = steps.first().toObject().value(QStringLiteral("instruction")).toString();
    }
    if (instruction.isEmpty()) {
        instruction = data.value(QStringLiteral("stationAddress")).toString(m_stationAddress);
    }
    m_instructionLabel->setText(instruction);
    m_statusLabel->setStyleSheet(QStringLiteral("color:#167d55;font-weight:700;"));
    m_statusLabel->setText(tr("路线规划完成"));
    m_planButton->setText(tr("重新规划"));
    m_endButton->show();

    const QJsonObject origin = data.value(QStringLiteral("origin")).toObject();
    const double originLatitude = origin.value(QStringLiteral("latitude")).toDouble();
    const double originLongitude = origin.value(QStringLiteral("longitude")).toDouble();
    loadTencentRoute(originLatitude, originLongitude, true);
}

void NavigationPage::showError(const QString &message)
{
    setBusy(false);
    m_statusLabel->setStyleSheet(QStringLiteral("color:#b42318;font-weight:600;"));
    m_statusLabel->setText(tr("规划失败"));
    m_instructionLabel->setText(message);
}

void NavigationPage::setBusy(bool busy)
{
    m_planButton->setEnabled(!busy);
    m_driveButton->setEnabled(!busy);
    m_walkButton->setEnabled(!busy);
    if (busy) {
        m_statusLabel->setStyleSheet(QString());
        m_statusLabel->setText(tr("正在规划路线…"));
        m_planButton->setText(tr("规划中…"));
    } else {
        m_planButton->setText(m_active ? tr("重新规划") : tr("开始导航"));
    }
}

void NavigationPage::resetNavigation()
{
    m_active = false;
    setBusy(false);
    setActiveMode(QStringLiteral("driving"));
    m_statusLabel->setStyleSheet(QString());
    m_statusLabel->setText(tr("准备规划"));
    m_durationLabel->setText(tr("-- 分钟"));
    m_etaLabel->setText(tr("等待路线规划"));
    m_distanceLabel->setText(tr("剩余距离 -- km"));
    m_instructionLabel->setText(tr("腾讯地图将根据实时位置规划路线"));
    m_endButton->hide();
}

QString NavigationPage::mode() const
{
    return m_mode;
}

QString NavigationPage::originAddress() const
{
    return m_originEdit->text().trimmed();
}

void NavigationPage::loadTencentRoute(double originLatitude,
                                      double originLongitude,
                                      bool hasOriginCoordinates)
{
    const QUrl url = tencentRouteUrl(originLatitude, originLongitude,
                                     hasOriginCoordinates);
#ifdef CHARGING_HAS_WEBENGINE
    m_webView->load(url);
#else
    m_webView->setHtml(
        tr("<div style='text-align:center;margin-top:30px'>"
           "<div style='font-size:36px'>🗺</div>"
           "<h3>腾讯地图路线</h3>"
           "<p>当前 Qt Kit 未安装 WebEngine 模块。</p>"
           "<p><a href='%1'>在系统浏览器中打开腾讯地图</a></p>"
           "</div>").arg(url.toString().toHtmlEscaped()));
#endif
}

QUrl NavigationPage::tencentRouteUrl(double originLatitude,
                                     double originLongitude,
                                     bool hasOriginCoordinates) const
{
    QUrl url(QStringLiteral("https://apis.map.qq.com/uri/v1/routeplan"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("type"),
                       m_mode == QStringLiteral("walking")
                           ? QStringLiteral("walk") : QStringLiteral("drive"));
    query.addQueryItem(QStringLiteral("from"), tr("当前位置"));
    query.addQueryItem(QStringLiteral("fromcoord"),
                       hasOriginCoordinates
                           ? QStringLiteral("%1,%2")
                                 .arg(originLatitude, 0, 'f', 7)
                                 .arg(originLongitude, 0, 'f', 7)
                           : QStringLiteral("CurrentLocation"));
    query.addQueryItem(QStringLiteral("to"), m_stationName);
    query.addQueryItem(QStringLiteral("tocoord"),
                       QStringLiteral("%1,%2")
                           .arg(m_stationLatitude, 0, 'f', 7)
                           .arg(m_stationLongitude, 0, 'f', 7));
    query.addQueryItem(QStringLiteral("policy"), QStringLiteral("1"));

    QString referer = qEnvironmentVariable("TENCENT_MAP_REFERER").trimmed();
    if (referer.isEmpty()) {
        referer = qEnvironmentVariable("TENCENT_MAP_KEY").trimmed();
    }
    if (!referer.isEmpty()) {
        query.addQueryItem(QStringLiteral("referer"), referer);
    }
    url.setQuery(query);
    return url;
}

void NavigationPage::setActiveMode(const QString &mode)
{
    m_mode = mode == QStringLiteral("walking")
        ? QStringLiteral("walking") : QStringLiteral("driving");
    const QSignalBlocker driveBlocker(m_driveButton);
    const QSignalBlocker walkBlocker(m_walkButton);
    m_driveButton->setChecked(m_mode == QStringLiteral("driving"));
    m_walkButton->setChecked(m_mode == QStringLiteral("walking"));
}