#ifndef PROTOCOLCODEC_H
#define PROTOCOLCODEC_H

#include <QObject>
#include <QByteArray>
#include <QJsonObject>

// ChargingProtocol 的底层"打包/拆包"工具，客户端和服务端两边都用同一份，
// 保证双方对"一条消息在字节流里长什么样"的理解完全一致。
//
// 报文格式（对应《概要设计说明书》4.2节的JSON请求/响应，外面再包一层长度前缀，
// 解决JSON文本在TCP流里的粘包/拆包问题——这跟ChargingDB/FileTransferTool里
// 处理文件数据粘包用的是同一个思路，只是这里包的是JSON而不是文件字节）：
//
//   [4字节 大端序 消息长度N][N字节 UTF-8编码的JSON文本]
//
// JSON本体的结构见《概要设计说明书》：
//   请求：{"action": "query_stations", "params": {...}}
//   响应：{"code": 0, "msg": "ok", "data": {...}}

class ProtocolCodec
{
public:
    // 把一个JSON对象打包成"长度前缀+JSON字节"的完整一帧，可以直接socket->write()
    static QByteArray encode(const QJsonObject &obj);
};

// FrameReceiver：挂在一个QTcpSocket上，每次readyRead时把新数据喂进来(feed())，
// 内部维护缓冲区，攒够一条完整报文就通过frameReady信号吐出解析好的QJsonObject。
// 服务端和客户端各自各拿一个FrameReceiver实例接在自己的socket上就行，
// 不需要关心底层怎么攒字节、怎么应对一次readyRead来了半条或好几条消息的情况。
class FrameReceiver : public QObject
{
    Q_OBJECT
public:
    explicit FrameReceiver(QObject *parent = nullptr);

    void feed(const QByteArray &bytes);

signals:
    void frameReady(const QJsonObject &obj);
    void frameError(const QString &message);

private:
    enum class State { WaitingLength, WaitingBody };

    QByteArray m_buffer;
    State m_state = State::WaitingLength;
    quint32 m_bodyLen = 0;

    void processBuffer();
};

#endif // PROTOCOLCODEC_H
