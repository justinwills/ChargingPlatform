#ifndef HTTPDASHBOARD_H
#define HTTPDASHBOARD_H

#include <QTcpServer>

class QTcpSocket;
class QJsonObject;
class QFileInfo;

// HttpDashboard：...
// 和 ServerListener（8888端口，自定义二进制协议，给Qt客户端用）是两回事——
// 这个是最简单的HTTP服务器，浏览器直接能访问，返回JSON，给dashboard.html用ECharts画图。
//
// 只支持一个GET接口：
//   GET /api/stats  ->  {"code":0,"data":{...跟admin_stats一样的数据...}}
//
// 用法（跟main.cpp里ServerListener的用法一样）：
//   HttpDashboard *dashboard = new HttpDashboard(this);
//   dashboard->listen(QHostAddress::Any, 8080);
class HttpDashboard : public QTcpServer
{
    Q_OBJECT
public:
    explicit HttpDashboard(QObject *parent = nullptr);

signals:
    void logMessage(const QString &text);

protected:
    void incomingConnection(qintptr socketDescriptor) override;

private:
    void handleRequest(qintptr socketDescriptor);
    static void writeJson(QTcpSocket *socket, int httpStatus, const QByteArray &statusText,
                           const QJsonObject &body);
    static void writeStaticFile(QTcpSocket *socket, const QString &relativePath);
    static void sendFile(QTcpSocket *socket, const QFileInfo &info);   // ← new
    static QJsonObject buildStatsJson(int days);
};

#endif // HTTPDASHBOARD_H