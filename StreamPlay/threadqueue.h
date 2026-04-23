#ifndef THREADQUEUE_H
#define THREADQUEUE_H

#include <QObject>
#include<QQueue>
#include<QWaitCondition>
#include<QMutex>
struct AVPacket;
template <typename T>
class ThreadQueue : public QObject
{

public:
    explicit ThreadQueue(QObject *parent = nullptr): QObject(parent){}
    void enqueue(T pkt){
        m_mutex_.lock();
        m_packets_.enqueue(pkt);
        m_condition_.notify_all();
        m_mutex_.unlock();
    }
    T dequeue(){
        QMutexLocker lock{&m_mutex_};
        while(m_packets_.empty()){
            m_condition_.wait(&m_mutex_);
        }
        return m_packets_.dequeue();
    }
signals:

public slots:
private:
    QQueue<T> m_packets_;
    QMutex m_mutex_;
    QWaitCondition m_condition_;
};

#endif // THREADQUEUE_H
