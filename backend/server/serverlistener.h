#ifndef SERVERLISTENER_H
#define SERVERLISTENER_H

#include <QTcpServer>

// ServerListener：跑在PC服务器端主线程（跟界面同一个线程），只负责"接新连接"。
// 每来一个新连接，重写的incomingConnection()会直接new一个ClientThread扔出去，
// 主线程不参与该连接后续任何收发处理，界面不会被网络I/O卡住。
//
// 用法（特布新在PC服务器端项目里）：
//   ServerListener *server = new ServerListener(this);
//   connect(server, &ServerListener::clientLog, ui->logEdit, ...);  // 可选：把日志接到界面
//   server->listen(QHostAddress::Any, 8888);
class ServerListener : public QTcpServer
{
    Q_OBJECT
public:
    explicit ServerListener(QObject *parent = nullptr);

signals:
    void clientLog(const QString &text);

protected:
    void incomingConnection(qintptr socketDescriptor) override;
};

#endif // SERVERLISTENER_H
