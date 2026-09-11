#include "serverlistener.h"
#include "clientthread.h"

ServerListener::ServerListener(QObject *parent) : QTcpServer(parent) {}

void ServerListener::incomingConnection(qintptr socketDescriptor)
{
    auto *thread = new ClientThread(socketDescriptor, this);
    connect(thread, &ClientThread::logMessage, this, &ServerListener::clientLog);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}
