#include <QCoreApplication>
#include <QDebug>

#include "database.h"
#include "serverlistener.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    if (!Database::init(QStringLiteral("charging.db"))) {
        qCritical() << "数据库初始化失败";
        return 1;
    }

    ServerListener server;
    QObject::connect(&server, &ServerListener::clientLog,
                     [](const QString &message) { qInfo().noquote() << message; });

    if (!server.listen(QHostAddress::Any, 8888)) {
        qCritical() << "服务器启动失败:" << server.errorString();
        return 1;
    }

    qInfo() << "Charging server listening on port 8888";
    return app.exec();
}