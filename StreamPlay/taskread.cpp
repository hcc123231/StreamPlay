#include "taskread.h"
#include"threadqueue.h"
#include<QDebug>
#include<QImage>

extern "C"{
#include<libavcodec/avcodec.h>
#include<libavformat/avformat.h>
#include<libswscale/swscale.h>
}
TaskRead::TaskRead(AVFormatContext* fmt_context,AVCodecContext* cdc_context,ThreadQueue* queue,int stream_idx,QObject *parent) : QObject(parent),m_working_{false},m_packes_{queue},m_stream_idx_{stream_idx}
  ,m_fmt_context_{fmt_context},m_cdc_context_{cdc_context}
{

}

/*void TaskRead::receiveFromRtsp()
{
    qDebug()<<"receiveFromRtsp";
    if(m_fmt_context_==nullptr||m_cdc_context_==nullptr||m_stream_idx_<0){
        qDebug()<<"context==nullptr||stream_idx<0";
        return;
    }
    m_working_=true;
    //不断从rtsp源端读取包数据
    while(m_working_){
        AVPacket* packet=m_packes_->dequeue();
        if(0){
            qDebug()<<"packet->stream_index!=stream_idx";
            continue;
        }
        if(avcodec_send_packet(m_cdc_context_,packet)){
            qDebug()<<"avcodec_send_packet failed";
            continue;
        }
        AVFrame* frame=av_frame_alloc();
        if(!frame){
            qDebug()<<"av_frame_alloc failed";
            continue;
        }
        AVFrame *rgb_frame = av_frame_alloc();

        // 准备一个用于格式转换的上下文
        struct SwsContext *sws_ctx = sws_getContext(m_cdc_context_->width, m_cdc_context_->height, m_cdc_context_->pix_fmt,
                                                    m_cdc_context_->width, m_cdc_context_->height, AV_PIX_FMT_RGB32,
                                                    SWS_BILINEAR, nullptr, nullptr, nullptr);
        while(!avcodec_receive_frame(m_cdc_context_,frame)){
            sws_scale(sws_ctx, frame->data, frame->linesize,
                      0, m_cdc_context_->height,
                      rgb_frame->data, rgb_frame->linesize);

            // 将 RGB 数据封装成 QImage 并通过信号发送到主线程显示
            QImage img(rgb_frame->data[0], m_cdc_context_->width, m_cdc_context_->height, QImage::Format_RGB32);
            emit frameReady(img);
        }
    }
}*/

void TaskRead::receiveFromRtsp()
{
    qDebug() << "receiveFromRtsp";
    if (!m_fmt_context_ || !m_cdc_context_ || m_stream_idx_ < 0) {
        qDebug() << "context == nullptr || stream_idx < 0";
        return;
    }

    m_working_ = true;

    // 1. 准备 SwsContext（只创建一次，假设宽高像素格式不变）
    SwsContext *sws_ctx = sws_getContext(
        m_cdc_context_->width, m_cdc_context_->height, m_cdc_context_->pix_fmt,
        m_cdc_context_->width, m_cdc_context_->height, AV_PIX_FMT_RGB32,
        SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!sws_ctx) {
        qDebug() << "sws_getContext failed";
        return;
    }

    // 2. 为 rgb_frame 分配缓冲区（宽高不变，一次分配）
    AVFrame *rgb_frame = av_frame_alloc();
    if (!rgb_frame) {
        qDebug() << "av_frame_alloc rgb_frame failed";
        sws_freeContext(sws_ctx);
        return;
    }
    rgb_frame->format = AV_PIX_FMT_RGB32;
    rgb_frame->width = m_cdc_context_->width;
    rgb_frame->height = m_cdc_context_->height;
    if (av_frame_get_buffer(rgb_frame, 32) < 0) {  // 32 字节对齐
        qDebug() << "av_frame_get_buffer failed";
        av_frame_free(&rgb_frame);
        sws_freeContext(sws_ctx);
        return;
    }

    // 主循环
    while (m_working_) {
        AVPacket *packet = m_packes_->dequeue();  // 假设包队列
        if (!packet) continue;

        /*if (packet->stream_index != m_stream_idx_) {
            av_packet_unref(packet);
            av_packet_free(&packet);
            continue;
        }*/

        if (avcodec_send_packet(m_cdc_context_, packet) != 0) {
            qDebug() << "avcodec_send_packet failed";
            av_packet_unref(packet);
            av_packet_free(&packet);
            continue;
        }

        // 接收所有解码出的帧（可能一包多帧）
        AVFrame *frame = av_frame_alloc();
        if (!frame) {
            av_packet_unref(packet);
            av_packet_free(&packet);
            continue;
        }

        while (avcodec_receive_frame(m_cdc_context_, frame) == 0) {
            // 转换格式（YUV → RGB32）
            sws_scale(sws_ctx,
                      frame->data, frame->linesize,
                      0, m_cdc_context_->height,
                      rgb_frame->data, rgb_frame->linesize);

            // 将 RGB 数据封装成 QImage（注意：这里会复制数据，可以优化为直接引用，但要小心生命周期）
            QImage img(rgb_frame->data[0],
                       m_cdc_context_->width,
                       m_cdc_context_->height,
                       rgb_frame->linesize[0],
                       QImage::Format_RGB32);
            // 发送到主线程
            emit frameReady(img.copy());  // copy 确保数据独立

            // 如果不想复制，需要保证 rgb_frame 的数据在槽函数使用前不被修改，
            // 可以使用队列或深拷贝。简单起见用 copy()
        }
        av_frame_free(&frame);
        av_packet_unref(packet);
        av_packet_free(&packet);
    }

    // 清理
    av_frame_free(&rgb_frame);
    sws_freeContext(sws_ctx);
}

