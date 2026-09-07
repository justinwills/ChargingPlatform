#include "clientthread.h"
#include "protocolcodec.h"
#include "requestdispatcher.h"
#include <QTcpSocket>
#include <QJsonDocument>

ClientThread::ClientThread(qintptr socketDescriptor, QObject *parent)
    : QThread(parent), m_socketDescriptor(socketDescriptor)
{
}

ClientThread::~ClientThread()
{
    // 保险措施：如果线程还在跑（比如客户端没有正常断开就直接把整个程序关掉），
    // 先让它退出事件循环、等它真正结束，再允许这个对象被析构——
    // 不这么做的话Qt会在"线程还在跑的时候把QThread对象销毁掉"报错甚至崩溃
    // （这就是最早测试时出现"QThread: Destroyed while thread is still running"的原因）。
    if (isRunning()) {
        quit();
        if (!wait(3000)) {
            terminate(); // 3秒内没能正常退出，兜底强制终止，避免程序卡死退不出去
            wait();
        }
    }
}

void ClientThread::run()
{
    // socket必须在这个线程里创建（而不是从外面传进来），
    // 因为QTcpSocket跟创建它的那个线程绑定，收发信号都得在同一个线程里处理，
    // 这也是官方"一客户端一线程"范例的标准写法。
    QTcpSocket socket;
    if (!socket.setSocketDescriptor(m_socketDescriptor)) {
        emit logMessage(QStringLiteral("绑定socket描述符失败：%1").arg(socket.errorString()));
        return;
    }
    emit logMessage(QStringLiteral("[线程%1] 客户端已连接：%2")
                         .arg(quintptr(QThread::currentThreadId())).arg(socket.peerAddress().toString()));

    FrameReceiver receiver;

    // 注意：connect()的第三个参数（上下文对象）这里故意用&socket/&receiver，
    // 不能用this（也就是ClientThread自己）——因为ClientThread这个QObject
    // 的线程归属是"创建它的那个线程"（ServerListener所在的主线程），
    // 而socket/receiver是在run()里、也就是这个新开的工作线程里创建的。
    // 如果上下文对象用this，Qt会因为"上下文对象所在线程"和"信号发出者所在线程"
    // 不一致而把连接当成跨线程连接处理，进而触发"Cannot create children for
    // a parent that is in a different thread"这个警告，出现莫名其妙的问题。
    connect(&socket, &QTcpSocket::readyRead, &socket, [&]() {
        receiver.feed(socket.readAll());
    });

    connect(&receiver, &FrameReceiver::frameReady, &receiver, [&](const QJsonObject &request) {
        QString action = request.value("action").toString();
        emit logMessage(QStringLiteral("[线程%1] 收到请求：action=%2")
                             .arg(quintptr(QThread::currentThreadId())).arg(action));

        QJsonObject response = RequestDispatcher::handle(request);
        socket.write(ProtocolCodec::encode(response));
    });

    connect(&receiver, &FrameReceiver::frameError, &receiver, [&](const QString &msg) {
        emit logMessage(QStringLiteral("[线程%1] 报文解析错误：%2")
                             .arg(quintptr(QThread::currentThreadId())).arg(msg));
    });

    // 这一行receiver用this（ClientThread自己），因为&QThread::quit是要"在某个
    // QThread实例上调用"的成员函数指针写法，receiver必须是QThread类型；
    // 加上Qt::DirectConnection强制同步直接调用，不受context对象线程归属影响，
    // 保证disconnected一触发，quit()立刻在当前线程执行，正确让本线程的exec()退出
    connect(&socket, &QTcpSocket::disconnected, this, &QThread::quit, Qt::DirectConnection);

    // 起这个线程自己的事件循环，socket的readyRead等信号才能被处理；
    // exec()会一直阻塞在这里，直到上面disconnected触发quit()为止
    exec();

    emit logMessage(QStringLiteral("[线程%1] 客户端已断开，线程结束")
                         .arg(quintptr(QThread::currentThreadId())));
}
