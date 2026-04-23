#ifndef FRAMEREAD_H
#define FRAMEREAD_H

#include <QObject>
class AVCodecContext;
template <typename T>
class ThreadQueue;
class AVPacket;
class FrameRead : public QObject
{
    Q_OBJECT
public:
    explicit FrameRead(AVCodecContext* cdc_context,ThreadQueue<AVPacket*>* queue,QObject *parent = nullptr);

signals:
    void frameReady(const QImage &image);
public slots:
    void receiveFrame();
private:
    AVCodecContext* m_cdc_context_;
    ThreadQueue<AVPacket*>* m_queue_;
    bool m_working_;
};

#endif // FRAMEREAD_H
