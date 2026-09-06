#ifndef CLIENTTHREAD_H
#define CLIENTTHREAD_H

#include <QThread>
#include <QJsonObject>

// ClientThread：服务端每接入一个客户端连接，就new一个ClientThread并start()，
// 这个连接后续的收发、业务处理都在这个独立线程里跑，不会互相阻塞，
// 也不会占用主线程（主线程只负责UI和accept新连接）——
// 对应《概要设计说明书》2.3节"服务端使用多线程（pthread）为其分配独立处理线程"的要求。
//
// 这是Qt官方"每客户端一线程"的经典写法（QTcpServer::incomingConnection + QThread::run
// 里创建socket），比moveToThread的写法更直观，出问题也更容易排查。
class ClientThread : public QThread
{
    Q_OBJECT
public:
    explicit ClientThread(qintptr socketDescriptor, QObject *parent = nullptr);
    ~ClientThread() override;

signals:
    // 给主线程/UI用的日志回调，业务处理本身不依赖这个信号
    void logMessage(const QString &text);

protected:
    void run() override;

private:
    qintptr m_socketDescriptor;
};

#endif // CLIENTTHREAD_H
