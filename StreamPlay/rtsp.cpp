extern "C"{
    #include<libavformat/avformat.h>
    #include<libavcodec/avcodec.h>
}
#include"threadqueue.h"
#include"taskread.h"
#include"rtsp.h"
#include"openglshow.h"

#include<QDebug>
#include<QThread>
int RtspPlay::rtspInit()
{
    int ret=0;
    /*if(!(ret=avformat_network_init())){
        qDebug()<<"avformat_network_init failed";
        return ret;
    }*/
    AVFormatContext* context=avformat_alloc_context();
    if(!context){
        qDebug()<<"avformat_alloc_context failed";
        return -1;
    }
    m_context_=context;
    //设置打开选项
    AVDictionary* options=nullptr;
    ret=av_dict_set(&options,"rtsp_transport","tcp",0);
    if(ret<0)qDebug()<<"av_dict_set failed";

    //打开rtsp url
    ret=avformat_open_input(&context,m_url_.toStdString().c_str(),nullptr,&options);
    if(ret){
        av_dict_free(&options);
        avformat_free_context(context);
        m_context_=nullptr;
        qDebug()<<"avformat_open_input failed";
        return -1;
    }
    av_dict_free(&options);

    return ret;
}

int RtspPlay::play()
{
    rtspInit();
    if(avformat_find_stream_info(m_context_,nullptr)<0){
        qDebug()<<"avformat_find_stream_info failed";
        return -1;
    }
    //从context中找到视频流下标和音频流下标存储到m_audio_streams_，m_video_streams_
    unsigned int audio_idx=0;
    unsigned int video_idx=0;
    for(unsigned int i=0;i<m_context_->nb_streams;i++){
        if(m_context_->streams[i]->codecpar->codec_type==AVMEDIA_TYPE_AUDIO){
            m_audio_streams_.push_back(i);
            m_audio_hash_.insert(i,audio_idx++);
        }else if(m_context_->streams[i]->codecpar->codec_type==AVMEDIA_TYPE_VIDEO){
            m_video_streams_.push_back(i);
            m_video_hash_.insert(i,video_idx++);
        }
    }

    //这里暂时以视频第一个流为例，初始化其解码器
    if(m_video_streams_.empty()){
        qDebug()<<"three is no video stream";
        return -1;
    }
    //根据该流编码器id查找对应解码器
    const AVCodec* decoder=avcodec_find_decoder(m_context_->streams[m_video_streams_[0]]->codecpar->codec_id);
    if(!decoder){
        qDebug()<<"avcodec_find_decoder failed";
        return -1;
    }
    //分配解码器上下文
    AVCodecContext* decoder_context_0=avcodec_alloc_context3(decoder);
    if(!decoder_context_0){
        qDebug()<<"avcodec_alloc_context3 failed";
        return -1;
    }
    //将该流的编码器参数拷贝到解码器上下文
    if(avcodec_parameters_to_context(decoder_context_0,m_context_->streams[0]->codecpar)<0){
        qDebug()<<"avcodec_parameters_to_context failed";
    }
    //打开解码器
    if(avcodec_open2(decoder_context_0,decoder,nullptr)){
        avcodec_free_context(&decoder_context_0);
        qDebug()<<"avcodec_open2 failed";
        return -1;
    }

    //准备多个ThreadQueue,有几路音视频流就创建几个ThreadQueue
    for(int i=0;i<m_audio_streams_.size();i++){
        ThreadQueue* queue=new ThreadQueue{};
        m_audio_queues_.push_back(queue);
    }
    for(int i=0;i<m_video_streams_.size();i++){
        ThreadQueue* queue=new ThreadQueue{};
        m_video_queues_.push_back(queue);
    }

    //新起线程去读取视频帧数据
    QThread* thread=new QThread{this};
    TaskRead* task_read=new TaskRead{m_context_,decoder_context_0,m_video_queues_[0],0};
    connect(task_read,&TaskRead::frameReady,m_show_,&OpenGlShow::updateFrame);
    task_read->moveToThread(thread);
    connect(thread,&QThread::started,task_read,&TaskRead::receiveFromRtsp);
    thread->start();
    //循环读取frame
    m_reading_=true;
    while(m_reading_){
        AVPacket* pkt=av_packet_alloc();
        if(!pkt){
            qDebug()<<"av_packet_alloc failed";
            continue;
        }
        if(av_read_frame(m_context_,pkt)){
            continue;
        }
        //qDebug()<<"packet read success";
        //将这个pkt按照stream_index推进对应的ThreadQueue中
        int pkt_stream_idx=pkt->stream_index;
        AVStream* stream=m_context_->streams[pkt_stream_idx];
        if(stream->codecpar->codec_type==AVMEDIA_TYPE_AUDIO){
            //音频流类型，直接到m_audio_hash_中找到m_audio_streams_对应下标的流，然后再找到m_audio_queues_中对应的ThreadQueue
            ThreadQueue* queue=m_audio_queues_[static_cast<int>(m_audio_hash_[static_cast<unsigned int>(pkt_stream_idx)])];
            queue->enqueue(pkt);
        }else if(stream->codecpar->codec_type==AVMEDIA_TYPE_VIDEO){
            ThreadQueue* queue=m_video_queues_[static_cast<int>(m_video_hash_[static_cast<unsigned int>(pkt_stream_idx)])];
            queue->enqueue(pkt);
        }
    }
    return 0;
}
