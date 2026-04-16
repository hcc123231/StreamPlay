#ifndef RTSP_H
#define RTSP_H
#include<QString>
#include<QVector>
#include<QHash>

#include <QObject>
extern "C"{
    #include<libavcodec/avcodec.h>
}
struct AVFormatContext;
class ThreadQueue;
class OpenGlShow;
class RtspPlay:public QObject{
    Q_OBJECT
public:
    RtspPlay(QString& url,OpenGlShow* show):m_url_{url},m_context_{nullptr},m_reading_{false},m_show_{show}{}
    /*
     * @brief 初始化ffmpeg
     */
    int rtspInit();
    /*
     * @brief 开始拉流
     */
    int play();
    void stop(){
        emit finished();
    }
private:
    OpenGlShow* m_show_;
    QString m_url_;
    AVFormatContext* m_context_;
    QVector<unsigned int> m_audio_streams_;
    QVector<unsigned int> m_video_streams_;
    QVector<ThreadQueue*> m_audio_queues_;
    QVector<ThreadQueue*> m_video_queues_;
    QHash<unsigned int,unsigned int> m_audio_hash_;//记录stream_index与m_audio_streams_中每个下标的关系
    QHash<unsigned int,unsigned int> m_video_hash_;//记录stream_index与m_video_streams_中每个下标的关系
    bool m_reading_;
signals:
    void finished();
};

#endif // RTSP_H
