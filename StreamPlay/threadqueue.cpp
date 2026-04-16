#include "threadqueue.h"

ThreadQueue::ThreadQueue(QObject *parent) : QObject(parent)
{

}

void ThreadQueue::enqueue(AVPacket *pkt)
{
    m_mutex_.lock();
    m_packets_.enqueue(pkt);
    m_condition_.notify_all();
    m_mutex_.unlock();
}

AVPacket* ThreadQueue::dequeue()
{
    QMutexLocker lock{&m_mutex_};
    while(m_packets_.empty()){
        m_condition_.wait(&m_mutex_);
    }
    return m_packets_.dequeue();
}

