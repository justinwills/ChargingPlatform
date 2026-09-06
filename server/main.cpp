#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDebug>
#include <QtGlobal>

#include "database.h"
#include "serverlistener.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    if (!Database::init(QStringLiteral("charging.db"))) {
        qCritical() << "数据库初始化失败";
        return 1;
    }

    const QString mapKey = qEnvironmentVariable("TENCENT_MAP_KEY").trimmed();
    if (mapKey.isEmpty()) {
        qInfo() << "Tencent geocoding: disabled (using local address matching)";
    } else {
        const QByteArray keyHash = QCryptographicHash::hash(
            mapKey.toUtf8(), QCryptographicHash::Sha256).toHex().left(8);
        qInfo().noquote() << QStringLiteral("Tencent geocoding: enabled (key fingerprint %1, suffix ...%2)")
                                 .arg(QString::fromLatin1(keyHash))
                                 .arg(mapKey.right(4));
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