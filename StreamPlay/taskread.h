#ifndef TASKREAD_H
#define TASKREAD_H

#include <QObject>
struct AVFormatContext;
template <typename T>
class ThreadQueue;
struct AVCodecContext;
class AVPacket;
class TaskRead : public QObject
{
    Q_OBJECT
public:
    explicit TaskRead(AVFormatContext* fmt_context,AVCodecContext* cdc_context,ThreadQueue<AVPacket*>* queue,int stream_idx,QObject *parent = nullptr);

signals:
    void frameReady(const QImage &image);
public slots:
    void receiveFromRtsp();
private:
    bool m_working_;
    ThreadQueue<AVPacket*>* m_packes_;
    AVFormatContext* m_fmt_context_;
    AVCodecContext* m_cdc_context_;
    int m_stream_idx_;
};

#endif // TASKREAD_H
