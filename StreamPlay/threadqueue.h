#ifndef THREADQUEUE_H
#define THREADQUEUE_H

#include <QObject>
#include<QQueue>
#include<QWaitCondition>
#include<QMutex>
struct AVPacket;
class ThreadQueue : public QObject
{
    Q_OBJECT
public:
    explicit ThreadQueue(QObject *parent = nullptr);
    void enqueue(AVPacket* pkt);
    AVPacket* dequeue();
signals:

public slots:
private:
    QQueue<AVPacket*> m_packets_;
    QMutex m_mutex_;
    QWaitCondition m_condition_;
};

#endif // THREADQUEUE_H
