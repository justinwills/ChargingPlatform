#include "clientconnection.h"

ClientConnection::ClientConnection(QObject *parent) : QObject(parent)
{
    connect(&m_socket, &QTcpSocket::connected, this, &ClientConnection::connected);
    connect(&m_socket, &QTcpSocket::disconnected, this, [this]() {
        m_pendingActions.clear();
        emit disconnected();
    });
    connect(&m_socket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        emit connectionError(m_socket.errorString());
    });
    connect(&m_socket, &QTcpSocket::readyRead, this, [this]() {
        m_receiver.feed(m_socket.readAll());
    });
    connect(&m_receiver, &FrameReceiver::frameReady, this,
            [this](const QJsonObject &response) {
        QJsonObject correlatedResponse = response;
        if (!m_pendingActions.isEmpty()) {
            correlatedResponse[QStringLiteral("_requestAction")] = m_pendingActions.dequeue();
        }
        emit responseReceived(correlatedResponse);
    });
    connect(&m_receiver, &FrameReceiver::frameError, this, &ClientConnection::connectionError);
}

void ClientConnection::connectToServer(const QString &host, quint16 port)
{
    m_socket.connectToHost(host, port);
}

void ClientConnection::disconnectFromServer()
{
    m_socket.disconnectFromHost();
}

bool ClientConnection::isConnected() const
{
    return m_socket.state() == QAbstractSocket::ConnectedState;
}

void ClientConnection::sendRequest(const QString &action, const QJsonObject &params)
{
    if (!isConnected()) {
        emit connectionError(QStringLiteral("尚未连接服务器，无法发送请求"));
        return;
    }
    QJsonObject req;
    req["action"] = action;
    req["params"] = params;
    m_pendingActions.enqueue(action);
    m_socket.write(ProtocolCodec::encode(req));
}
