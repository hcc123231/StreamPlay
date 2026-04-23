#ifndef RTPRECEIVER_H
#define RTPRECEIVER_H
#include<QByteArray>
#include <QObject>


template <typename T>
class ThreadQueue;
struct RtpParsedInfo {
    uint8_t  version;
    bool     hasPadding;
    bool     hasExtension;
    uint8_t  csrcCount;
    bool     marker;
    uint8_t  payloadType;
    uint16_t sequence;
    uint32_t timestamp;
    uint32_t ssrc;
    QByteArray payload;          // RTP 负载数据（已跳过头部、CSRC、扩展）
};

class AVPacket;
class RtpReceiver : public QObject
{
    Q_OBJECT
public:
    explicit RtpReceiver(ThreadQueue<QByteArray*>* queue,ThreadQueue<AVPacket*>* out_queue,QObject *parent = nullptr);
    void receive();
signals:

public slots:
private:
    ThreadQueue<QByteArray*>* m_queue_;
    ThreadQueue<AVPacket*>* m_out_queue_;
    bool m_working_;
};

#endif // RTPRECEIVER_H
