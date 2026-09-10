#include "httpdashboard.h"
#include "database.h"

#include <QTcpSocket>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QUrlQuery>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QCoreApplication>

HttpDashboard::HttpDashboard(QObject *parent) : QTcpServer(parent) {}

void HttpDashboard::incomingConnection(qintptr socketDescriptor)
{
    handleRequest(socketDescriptor);
}

void HttpDashboard::writeJson(QTcpSocket *socket, int httpStatus, const QByteArray &statusText,
                               const QJsonObject &body)
{
    const QByteArray json = QJsonDocument(body).toJson(QJsonDocument::Compact);
    QByteArray response;
    response += "HTTP/1.1 " + QByteArray::number(httpStatus) + " " + statusText + "\r\n";
    response += "Content-Type: application/json; charset=utf-8\r\n";
    response += "Access-Control-Allow-Origin: *\r\n";
    response += "Content-Length: " + QByteArray::number(json.size()) + "\r\n";
    response += "Connection: close\r\n\r\n";
    response += json;

    socket->write(response);
    socket->flush();
    socket->disconnectFromHost();
}

void HttpDashboard::sendFile(QTcpSocket *socket, const QFileInfo &info)
{
    QFile file(info.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly)) {
        writeJson(socket, 500, "Internal Server Error",
                  QJsonObject{{"code", 1}, {"msg", "文件打开失败：" + info.fileName()}});
        return;
    }

    const QByteArray content = file.readAll();
    const QString suffix = info.suffix().toLower();
    QByteArray contentType = "text/plain; charset=utf-8";
    if (suffix == QStringLiteral("html")) contentType = "text/html; charset=utf-8";
    else if (suffix == QStringLiteral("js")) contentType = "application/javascript; charset=utf-8";
    else if (suffix == QStringLiteral("css")) contentType = "text/css; charset=utf-8";

    QByteArray response;
    response += "HTTP/1.1 200 OK\r\n";
    response += "Content-Type: " + contentType + "\r\n";
    response += "Access-Control-Allow-Origin: *\r\n";
    response += "Content-Length: " + QByteArray::number(content.size()) + "\r\n";
    response += "Connection: close\r\n\r\n";
    response += content;

    socket->write(response);
    socket->flush();
    socket->disconnectFromHost();
}

void HttpDashboard::writeStaticFile(QTcpSocket *socket, const QString &relativePath)
{
    QString safeName = relativePath;
    if (safeName.isEmpty() || safeName == QStringLiteral("/")) {
        safeName = QStringLiteral("dashboard.html");
    }
    safeName.replace(QStringLiteral(".."), QString());
    while (safeName.startsWith('/')) safeName.remove(0, 1);

    const QString envRoot = qEnvironmentVariable("DASHBOARD_ROOT").trimmed();
    if (!envRoot.isEmpty()) {
        const QFileInfo info(QDir(envRoot).filePath(safeName));
        if (info.exists() && info.isFile()) {
            sendFile(socket, info);
            return;
        }
    }

    const QStringList searchStarts = {
        QCoreApplication::applicationDirPath(),
        QDir::currentPath(),
    };

    for (const QString &start : searchStarts) {
        QDir dir(start);
        for (int level = 0; level < 8; ++level) {
            const QDir candidate(dir.filePath(QStringLiteral("dashboard")));
            const QFileInfo info(candidate.filePath(safeName));
            if (info.exists() && info.isFile()) {
                sendFile(socket, info);
                return;
            }
            if (!dir.cdUp()) break;
        }
    }

    writeJson(socket, 404, "Not Found",
              QJsonObject{{"code", 1}, {"msg", "静态文件未找到：" + safeName}});
}

QJsonObject HttpDashboard::buildStatsJson(int daysParam)
{
    const int days = daysParam == 30 ? 30 : 7;

    QJsonObject pileStatus;
    const QMap<QString, int> stats = Database::getPileStatusStats();
    for (auto it = stats.cbegin(); it != stats.cend(); ++it) {
        pileStatus[it.key()] = it.value();
    }

    QJsonArray trend;
    for (const auto &point : Database::getRevenueTrend(days)) {
        trend.append(QJsonObject{{"date", point.first}, {"revenue", point.second}});
    }

    QJsonArray stations;
    for (const StationInfo &station : Database::getAllStations()) {
        const int freeCount = Database::getFreePileCount(station.id);
        const double onlineRate = station.pileCount > 0
            ? (double(freeCount) / double(station.pileCount)) * 100.0
            : 0.0;
        stations.append(QJsonObject{
            {"stationId", station.id}, {"name", station.name},
            {"pileCount", station.pileCount}, {"freePileCount", freeCount},
            {"onlineRate", onlineRate}
        });
    }

    return QJsonObject{
        {"code", 0}, {"msg", "ok"},
        {"data", QJsonObject{
            {"revenueToday", Database::getRevenueToday()},
            {"revenueThisMonth", Database::getRevenueThisMonth()},
            {"revenueTotal", Database::getRevenueTotal()},
            {"pileStatus", pileStatus},
            {"trendDays", days},
            {"revenueTrend", trend},
            {"stations", stations}
        }}
    };
}

void HttpDashboard::handleRequest(qintptr socketDescriptor)
{
    auto *socket = new QTcpSocket(this);
    if (!socket->setSocketDescriptor(socketDescriptor)) {
        delete socket;
        return;
    }

    connect(socket, &QTcpSocket::readyRead, this, [socket]() {
        if (!socket->canReadLine()) return;
        const QByteArray requestLine = socket->readLine();
        const QList<QByteArray> parts = requestLine.split(' ');
        const QString path = parts.size() >= 2 ? QString::fromUtf8(parts[1]) : QString();
        const QUrl url(QStringLiteral("http://localhost") + path);

        if (url.path() == QStringLiteral("/api/stats")) {
            QUrlQuery query(url);
            const int requestedDays = query.queryItemValue(QStringLiteral("days")).toInt();
            writeJson(socket, 200, "OK", buildStatsJson(requestedDays));
            return;
        }

        writeStaticFile(socket, url.path());
    });

    connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
}