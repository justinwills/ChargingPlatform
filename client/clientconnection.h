#ifndef CLIENTCONNECTION_H
#define CLIENTCONNECTION_H

#include <QObject>
#include <QTcpSocket>
#include <QJsonObject>
#include <QQueue>
#include "protocolcodec.h"

// ClientConnection：充电用户端用来跟PC服务器端说话的封装。
// 只需要 connectToServer() 一次，之后反复调用 sendRequest(action, params)，
// 响应会从 responseReceived 信号里拿到。连接层按TCP请求顺序记录action，
// 并在响应对象的_requestAction字段中带回对应action。
//
// 用法（充电用户端项目里）：
//   auto *conn = new ClientConnection(this);
//   connect(conn, &ClientConnection::responseReceived, this, &MainWindow::onServerResponse);
//   connect(conn, &ClientConnection::connectionError, this, &MainWindow::onConnError);
//   conn->connectToServer("127.0.0.1", 8888);
//   ...
//   conn->sendRequest("login", {{"phone", "13800000001"}});
class ClientConnection : public QObject
{
    Q_OBJECT
public:
    explicit ClientConnection(QObject *parent = nullptr);

    void connectToServer(const QString &host, quint16 port);
    void disconnectFromServer();
    bool isConnected() const;

    // action: 如 "login" / "query_stations" / "start_charging" 等，参照
    // 《概要设计说明书》4.2节和 requestdispatcher.h 顶部注释里列的清单
    void sendRequest(const QString &action, const QJsonObject &params);

signals:
    void connected();
    void disconnected();
    void connectionError(const QString &message);
    void responseReceived(const QJsonObject &response); // 完整响应：{code, msg, data}

private:
    QTcpSocket m_socket;
    FrameReceiver m_receiver;
    QQueue<QString> m_pendingActions;
};

#endif // CLIENTCONNECTION_H
