#include "protocolcodec.h"
#include <QJsonDocument>
#include <QDataStream>
#include <QIODevice>

QByteArray ProtocolCodec::encode(const QJsonObject &obj)
{
    QByteArray body = QJsonDocument(obj).toJson(QJsonDocument::Compact);

    QByteArray frame;
    QDataStream out(&frame, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::BigEndian);
    out << quint32(body.size());
    frame.append(body);
    return frame;
}

FrameReceiver::FrameReceiver(QObject *parent) : QObject(parent) {}

void FrameReceiver::feed(const QByteArray &bytes)
{
    m_buffer.append(bytes);
    processBuffer();
}

void FrameReceiver::processBuffer()
{
    // 跟FileReceiver里的思路一样：while(true)反复处理，攒够当前阶段需要的字节数
    // 才往下推进，不够就return等下一次feed()；这样一次readyRead里粘着好几条
    // 消息、或者一条消息被拆成好几次到达，都能正确处理。
    while (true) {
        if (m_state == State::WaitingLength) {
            if (m_buffer.size() < 4) return;

            QDataStream in(m_buffer);
            in.setByteOrder(QDataStream::BigEndian);
            in >> m_bodyLen;
            m_buffer.remove(0, 4);
            m_state = State::WaitingBody;
            // 不return，往下试：JSON正文没准已经跟长度前缀一起到了
        } else { // WaitingBody
            if (quint32(m_buffer.size()) < m_bodyLen) return;

            QByteArray body = m_buffer.left(int(m_bodyLen));
            m_buffer.remove(0, int(m_bodyLen));
            m_state = State::WaitingLength;

            QJsonParseError err;
            QJsonDocument doc = QJsonDocument::fromJson(body, &err);
            if (err.error != QJsonParseError::NoError || !doc.isObject()) {
                emit frameError(QStringLiteral("JSON解析失败：%1").arg(err.errorString()));
                continue; // 这一条丢弃，继续处理缓冲区里剩下的（如果有的话）
            }
            emit frameReady(doc.object());
        }
    }
}
