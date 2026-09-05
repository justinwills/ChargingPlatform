#include "serverlistener.h"
#include "clientthread.h"

ServerListener::ServerListener(QObject *parent) : QTcpServer(parent) {}

void ServerListener::incomingConnection(qintptr socketDescriptor)
{
    auto *thread = new ClientThread(socketDescriptor, this);
    connect(thread, &ClientThread::logMessage, this, &ServerListener::clientLog);
    // 线程跑完(run()里exec()退出)之后自动delete掉自己，不用手动管理
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}
