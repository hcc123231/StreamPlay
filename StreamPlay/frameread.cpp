#include "frameread.h"
#include"threadqueue.h"
#include<QDebug>
#include<QImage>

extern "C"{
#include<libavcodec/avcodec.h>
#include<libavformat/avformat.h>
#include<libswscale/swscale.h>
}
FrameRead::FrameRead(AVCodecContext* cdc_context,ThreadQueue<AVPacket*>* queue,QObject *parent) : QObject(parent),
    m_cdc_context_(cdc_context),m_queue_(queue),m_working_{false}
{

}

void FrameRead::receiveFrame()
{
    qDebug() << "receiveFromRtsp";
    if (!m_cdc_context_) {
        qDebug() << "context == nullptr";
        return;
    }

    m_working_ = true;

    // 用于格式转换的上下文和 RGB 帧，初始为 nullptr
    SwsContext *sws_ctx = nullptr;
    AVFrame *rgb_frame = nullptr;

    // 记录当前使用的宽高和像素格式，以便检测变化
    int current_width = 0;
    int current_height = 0;
    AVPixelFormat current_pix_fmt = AV_PIX_FMT_NONE;

    while (m_working_) {
        AVPacket *packet = m_queue_->dequeue();
        if (!packet) {
            // 队列可能被关闭或为空，根据实际情况处理，这里简单继续
            continue;
        }

        // 验证包
        if (packet->data == nullptr || packet->size <= 0) {
            qWarning() << "Invalid packet: data=" << packet->data << " size=" << packet->size;
            av_packet_free(&packet);
            continue;
        }

        // 发送到解码器
        int ret = avcodec_send_packet(m_cdc_context_, packet);
        av_packet_free(&packet);  // 发送后即可释放
        if (ret < 0) {
            qDebug() << "avcodec_send_packet failed:" << ret;
            continue;
        }

        // 接收解码后的帧（可能多帧）
        AVFrame *frame = av_frame_alloc();
        if (!frame) {
            continue;
        }

        while (true) {
            ret = avcodec_receive_frame(m_cdc_context_, frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            } else if (ret < 0) {
                qDebug() << "avcodec_receive_frame error:" << ret;
                break;
            }

            // 成功解码一帧
            // 检查帧的宽高和像素格式是否有效
            if (frame->width <= 0 || frame->height <= 0 || frame->format == AV_PIX_FMT_NONE) {
                qWarning() << "Decoded frame has invalid geometry or format";
                av_frame_unref(frame);
                continue;
            }

            // 如果格式发生变化（或尚未创建转换上下文），则重新创建
            if (sws_ctx == nullptr || current_width != frame->width ||
                current_height != frame->height || current_pix_fmt != frame->format) {

                // 释放旧的
                if (rgb_frame) {
                    av_frame_free(&rgb_frame);
                }
                if (sws_ctx) {
                    sws_freeContext(sws_ctx);
                }

                // 创建新的 RGB 帧
                rgb_frame = av_frame_alloc();
                if (!rgb_frame) {
                    qWarning() << "av_frame_alloc rgb_frame failed";
                    break;
                }
                rgb_frame->format = AV_PIX_FMT_RGB32;
                rgb_frame->width = frame->width;
                rgb_frame->height = frame->height;
                if (av_frame_get_buffer(rgb_frame, 32) < 0) {
                    qWarning() << "av_frame_get_buffer failed";
                    av_frame_free(&rgb_frame);
                    rgb_frame = nullptr;
                    break;
                }

                // 创建转换上下文
                sws_ctx = sws_getContext(
                    frame->width, frame->height, (AVPixelFormat)frame->format,
                    frame->width, frame->height, AV_PIX_FMT_RGB32,
                    SWS_BILINEAR, nullptr, nullptr, nullptr);
                if (!sws_ctx) {
                    qWarning() << "sws_getContext failed";
                    av_frame_free(&rgb_frame);
                    rgb_frame = nullptr;
                    break;
                }

                // 记录当前格式
                current_width = frame->width;
                current_height = frame->height;
                current_pix_fmt = (AVPixelFormat)frame->format;
                qDebug() << "Created sws_ctx for" << current_width << "x" << current_height
                         << "pix_fmt:" << current_pix_fmt;
            }

            // 执行格式转换
            sws_scale(sws_ctx,
                      frame->data, frame->linesize,
                      0, frame->height,
                      rgb_frame->data, rgb_frame->linesize);

            // 创建 QImage（注意：这里会复制数据，因为 rgb_frame 后续会被覆盖）
            QImage img(rgb_frame->data[0],
                       rgb_frame->width,
                       rgb_frame->height,
                       rgb_frame->linesize[0],
                       QImage::Format_RGB32);
            emit frameReady(img.copy());  // 发送拷贝

            av_frame_unref(frame);
        }
        av_frame_free(&frame);
    }

    // 清理
    if (rgb_frame) av_frame_free(&rgb_frame);
    if (sws_ctx) sws_freeContext(sws_ctx);
}
