#ifndef RTSPCLIENT_H
#define RTSPCLIENT_H

#include <QObject>
#include<QTcpSocket>
#include"threadqueue.h"
#include<QString>
#include<QUrl>
//使用状态枚举来枚举当前网络状态
enum InteractionStatus{
    STATE_NULL=0,
    STATE_OPTIONS,
    STATE_DESCRIBE,
    STATE_SETUP,
    STATE_PLAY,
    STATE_FINISH
};
struct MediaInfo {
    QString m_type_;          // "video" / "audio"
    QString m_control_url_;   // SETUP 用的 URL
    QString m_rtpmap_;        // 如 "96 H264/90000"
    QString m_fmtp_;          // 完整的 fmtp 行内容（可选）
    QByteArray m_sps;         // H.264 序列参数集
    QByteArray m_pps;         // H.264 图像参数集
    // 对于 AAC，可以增加 m_config 等
};
//static QList<MediaInfo> media_infos;
class RtspClient : public QObject
{
    Q_OBJECT
public:
    explicit RtspClient(ThreadQueue<QByteArray*>* queue,QObject *parent = nullptr);
    void setUrl(const QUrl& url);
    void start();
signals:
    void meidainfo(QList<MediaInfo> infos);
public slots:
    void onConnected();
    void onReadyRead();
    void onError(QAbstractSocket::SocketError);
private:
    void connect();
    void request(const QString& method,const QUrl& url,const void* args);
private:
    QTcpSocket* m_socket_;
    QUrl m_url_;
    int m_cseq_;
    QByteArray m_rbuf_;//接收缓冲区
    InteractionStatus m_interaction_status_;//当前交互状态
    QVector<QString> m_options_;//保存rtsp能力
    QString m_sessionId_;
    QString m_audio_ctl_url_;
    ThreadQueue<QByteArray*>* m_rtp_pkts_;
};

#endif // RTSPCLIENT_H
