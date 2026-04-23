extern "C"{
    #include<libavformat/avformat.h>
    #include<libavcodec/avcodec.h>
}
#include"threadqueue.h"
#include"frameread.h"
#include"rtsp.h"
#include"openglshow.h"
#include"rtpreceiver.h"
#include"rtspclient.h"
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
#include <QtEndian>
#include <QByteArray>
#include <QDebug>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
}

/**
 * @brief 根据 H.264 的 SPS 和 PPS 初始化解码器
 * @param sps     序列参数集（二进制，已从 Base64 解码）
 * @param pps     图像参数集（二进制，已从 Base64 解码）
 * @param outCodecCtx 输出初始化解码器上下文（调用者需用 avcodec_free_context 释放）
 * @return true 成功，false 失败
 */
bool initH264Decoder(const QByteArray& sps, const QByteArray& pps, AVCodecContext** outCodecCtx) {
    // 1. 检查输入
    if (sps.isEmpty() || pps.isEmpty()) {
        qWarning() << "SPS or PPS is empty";
        return false;
    }

    // 2. 构建 AVCC 格式的 extradata
    // 格式参考: ISO/IEC 14496-15
    // 结构:
    //   configurationVersion (1 byte) = 0x01
    //   AVCProfileIndication (1 byte) = sps[0]
    //   profile_compatibility (1 byte) = sps[1]
    //   AVCLevelIndication (1 byte) = sps[2]
    //   lengthSizeMinusOne (1 byte) = 0xFC | 0x03 (表示 NALU 长度字段占 4 字节)
    //   numOfSequenceParameterSets (1 byte) = 0xE0 | 0x01 (表示 1 个 SPS)
    //   sequenceParameterSetLength (2 bytes) = sps.size() (大端)
    //   SPS data (N bytes)
    //   numOfPictureParameterSets (1 byte) = 0x01 (表示 1 个 PPS)
    //   pictureParameterSetLength (2 bytes) = pps.size() (大端)
    //   PPS data (M bytes)
    int extraSize = 8 + sps.size() + 1 + 2 + pps.size();
    QByteArray extraData;
    extraData.reserve(extraSize + AV_INPUT_BUFFER_PADDING_SIZE);

    // 2.1 固定头部 (6 bytes)
    extraData.append(static_cast<char>(0x01));                               // configurationVersion
    extraData.append(static_cast<char>(sps[0]));                             // AVCProfileIndication
    extraData.append(static_cast<char>(sps[1]));                             // profile_compatibility
    extraData.append(static_cast<char>(sps[2]));                             // AVCLevelIndication
    extraData.append(static_cast<char>(0xFC | 0x03));                        // lengthSizeMinusOne (bit 0-1)
    extraData.append(static_cast<char>(0xE0 | 0x01));                        // numOfSequenceParameterSets (bit 0-4)

    // 2.2 SPS 长度 (大端) + SPS 数据
    uint16_t spsLenBE = qToBigEndian<uint16_t>(static_cast<uint16_t>(sps.size()));
    extraData.append(reinterpret_cast<const char*>(&spsLenBE), 2);
    extraData.append(sps);

    // 2.3 PPS 个数
    extraData.append(static_cast<char>(0x01));                               // numOfPictureParameterSets

    // 2.4 PPS 长度 (大端) + PPS 数据
    uint16_t ppsLenBE = qToBigEndian<uint16_t>(static_cast<uint16_t>(pps.size()));
    extraData.append(reinterpret_cast<const char*>(&ppsLenBE), 2);
    extraData.append(pps);

    // 2.5 添加填充字节 (FFmpeg 要求)
    extraData.append(AV_INPUT_BUFFER_PADDING_SIZE, 0);

    // 3. 查找解码器
    const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec) {
        qWarning() << "H.264 decoder not found";
        return false;
    }

    // 4. 分配解码器上下文
    AVCodecContext* codecCtx = avcodec_alloc_context3(codec);
    if (!codecCtx) {
        qWarning() << "Failed to allocate codec context";
        return false;
    }

    // 5. 设置 extradata
    codecCtx->extradata = static_cast<uint8_t*>(av_mallocz(extraData.size() + AV_INPUT_BUFFER_PADDING_SIZE));
    if (!codecCtx->extradata) {
        qWarning() << "Failed to allocate extradata";
        avcodec_free_context(&codecCtx);
        return false;
    }
    memcpy(codecCtx->extradata, extraData.constData(), extraData.size());
    codecCtx->extradata_size = extraData.size();

    // 6. 可选设置时间基（RTP 常用 90kHz）
    codecCtx->pkt_timebase = AVRational{1, 90000};

    // 7. 打开解码器
    if (avcodec_open2(codecCtx, codec, nullptr) < 0) {
        qWarning() << "Failed to open H.264 decoder";
        avcodec_free_context(&codecCtx);
        return false;
    }

    *outCodecCtx = codecCtx;
    return true;
}
//extern QList<MediaInfo> media_infos;
int RtspPlay::play()
{
    ThreadQueue<QByteArray*>* queue=new ThreadQueue<QByteArray*>();

    ThreadQueue<AVPacket*>* out_queue=new ThreadQueue<AVPacket*>();
    m_out_queue_=out_queue;
    RtspClient* client=new RtspClient{queue};
    client->setUrl(QUrl("rtsp://192.168.5.48:8554/stream1"));
    client->start();
    RtpReceiver* receiver=new RtpReceiver(queue,out_queue);
    QThread* thread=new QThread();
    receiver->moveToThread(thread);
    connect(thread,&QThread::started,receiver,&RtpReceiver::receive);
    thread->start();
    //初始化解码器
    //当media_info准备好之后初始化解码器
    connect(client,&RtspClient::meidainfo,this,&RtspPlay::init_decoder);



    /*rtspInit();
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
    }*/
    return 0;
}

void RtspPlay::init_decoder(QList<MediaInfo> infos)
{
    QByteArray sps;
    QByteArray pps;
    while(infos.empty()){
        qDebug()<<"media_infos is empty";
    }
    for(const auto& item:infos){
        if(item.m_type_=="video"){
            sps=item.m_sps;
            pps=item.m_pps;
        }
    }
    AVCodecContext* cdc_context=nullptr;
    initH264Decoder(sps,pps,&cdc_context);
    //新起一个线程循环解码out_queue中packet
    QThread* newthread=new QThread();
    FrameRead* frame_read=new FrameRead(cdc_context,m_out_queue_);
    connect(frame_read,&FrameRead::frameReady,m_show_,&OpenGlShow::updateFrame);
    frame_read->moveToThread(newthread);
    connect(newthread,&QThread::started,frame_read,&FrameRead::receiveFrame);
    newthread->start();
}
