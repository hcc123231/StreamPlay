#include "rtpreceiver.h"
#include"threadqueue.h"
#include <QtEndian>
#include <QByteArray>
#include<QDebug>
extern "C"{
#include<libavformat/avformat.h>
}
RtpReceiver::RtpReceiver(ThreadQueue<QByteArray*>* queue,ThreadQueue<AVPacket*>* out_queue,QObject *parent) : QObject(parent),m_queue_{queue},m_working_{false},m_out_queue_{out_queue}
{

}
bool parseRtpPacket(const QByteArray& packet, RtpParsedInfo& info) {
    if (packet.size() < 12) {
        return false;   // 不足固定头长度
    }

    const uint8_t* data = reinterpret_cast<const uint8_t*>(packet.constData());

    // 第一个字节
    info.version      = (data[0] >> 6) & 0x03;
    info.hasPadding   = (data[0] >> 5) & 0x01;
    info.hasExtension = (data[0] >> 4) & 0x01;
    info.csrcCount    = data[0] & 0x0F;

    // 第二个字节
    info.marker       = (data[1] >> 7) & 0x01;
    info.payloadType  = data[1] & 0x7F;

    // 后面字段（网络字节序转主机序）
    info.sequence     = qFromBigEndian<uint16_t>(data + 2);
    info.timestamp    = qFromBigEndian<uint32_t>(data + 4);
    info.ssrc         = qFromBigEndian<uint32_t>(data + 8);

    // 计算头部总长度
    int headerLen = 12 + info.csrcCount * 4;

    // 处理扩展头（如果存在）
    if (info.hasExtension) {
        if (packet.size() < headerLen + 4) {
            return false;   // 不够扩展头的长度
        }
        uint16_t extLen = qFromBigEndian<uint16_t>(data + headerLen + 2);
        headerLen += 4 + extLen * 4;
    }

    // 检查长度
    if (packet.size() < headerLen) {
        return false;
    }

    // 负载数据
    int payloadLen = packet.size() - headerLen;
    if (info.hasPadding && payloadLen > 0) {
        // 最后一个字节表示填充长度
        uint8_t paddingLen = data[packet.size() - 1];
        if (paddingLen <= payloadLen) {
            payloadLen -= paddingLen;
        } else {
            return false;
        }
    }

    info.payload= packet.mid(headerLen, payloadLen);

    return true;
}
struct FuABuffer {
QByteArray data;      // 已接收的分片数据（不含 FU header）
uint32_t startTimestamp;
bool active = false;
};
static QMap<uint32_t, FuABuffer> fuBuffers;
AVPacket* makePacket(const QByteArray& nalu, uint32_t rtpTimestamp){
    AVPacket* pkt = av_packet_alloc();
    if (!pkt) return nullptr;

    // 分配数据空间并拷贝
    pkt->data = (uint8_t*)av_malloc(nalu.size());
    if (!pkt->data) {
        av_packet_free(&pkt);
        return nullptr;
    }
    memcpy(pkt->data, nalu.constData(), nalu.size());
    pkt->size = nalu.size();

    // 设置时间戳：RTP 时间戳需要转换为解码器的时间基
    // 通常 H.264 RTP 时钟频率为 90000 Hz
    pkt->pts = pkt->dts = rtpTimestamp;
    return pkt;
}
void processH264RtpPacket(const QByteArray& rtpPayload, uint32_t timestamp, bool marker,
                          uint8_t nalType, uint32_t ssrc,ThreadQueue<AVPacket*>* out_queue) {
    if (rtpPayload.isEmpty()) return;

    // 情况1：单 NALU 包 (1-23, 24? STAP-A 暂不处理)
    if (nalType >= 1 && nalType <= 23) {
        // 构造 Annex-B NALU: 起始码 + 原始负载
        QByteArray nalu;
        nalu.append("\x00\x00\x00\x01", 4);
        nalu.append(rtpPayload);

        //sendToDecoder(nalu, timestamp, codecCtx);
        //组包
        AVPacket* pkt=makePacket(nalu, timestamp);
        if(pkt==nullptr){
            qDebug()<<"makePacket return nullptr";
            return;
        }
        //赛进队列，等待其他线程来取包
        out_queue->enqueue(pkt);
        return;
    }

    // 情况2：FU-A 分片 (nalType == 28)
    if (nalType == 28 && rtpPayload.size() >= 2) {
        uint8_t fuHeader = static_cast<uint8_t>(rtpPayload[1]);
        bool start = (fuHeader & 0x80) != 0;   // S bit
        bool end   = (fuHeader & 0x40) != 0;   // E bit
        uint8_t originalNalType = fuHeader & 0x1F;

        // 分片载荷：去掉前两个字节 (NAL header + FU header)
        QByteArray fragment = rtpPayload.mid(2);

        auto& buf = fuBuffers[ssrc];   // 更严谨应用 (timestamp, ssrc) 做 key
        if (start) {
            // 开始新分片：重建 NAL 头（原始 NAL 头的高 3 位 + 原 NAL 类型）
            uint8_t originalNalHeader = (rtpPayload[0] & 0xE0) | originalNalType;
            buf.data.clear();
            buf.data.append(static_cast<char>(originalNalHeader));
            buf.data.append(fragment);
            buf.startTimestamp = timestamp;
            buf.active = true;
        } else if (buf.active) {
            // 中间或结束分片
            buf.data.append(fragment);
            if (end) {
                // 重组完成，构造 Annex-B NALU
                QByteArray completeNalu;
                completeNalu.append("\x00\x00\x00\x01", 4);
                completeNalu.append(buf.data);
                //sendToDecoder(completeNalu, buf.startTimestamp, codecCtx);
                //组包
                AVPacket* packet=makePacket(completeNalu,buf.startTimestamp);
                //入队
                out_queue->enqueue(packet);
                buf.active = false;
            }
        }
        return;
    }

    // 其他类型（如 STAP-A, FU-B 等）可根据需要实现
    qWarning() << "Unsupported H.264 RTP packet type:" << nalType;
}
void RtpReceiver::receive()
{
    m_working_=true;
    while(m_working_){
        QByteArray* array=m_queue_->dequeue();
        qDebug()<<"array is coming";
        //循环接收到rtp包数据
        //开始解析
        RtpParsedInfo* info=new RtpParsedInfo{};
        bool ok=parseRtpPacket(*array,*info);
        delete array;
        if (ok) {
            /*qDebug()<<"-------------------------------------------";
            qDebug() << "Version:" << info->version;
            qDebug() << "Payload Type:" << info->payloadType;
            qDebug() << "Sequence:" << info->sequence;
            qDebug() << "Timestamp:" << info->timestamp;
            qDebug() << "SSRC:" << QString::number(info->ssrc, 16);
            qDebug() << "Marker:" << info->marker;
            qDebug() << "Payload size:" << info->payload.size();
            qDebug()<<"-------------------------------------------";*/
            // 根据 payloadType 分发处理
            if (info->payloadType == 96) {  // H.264
                //m_out_queue_->enqueue(&(info->payload));
                uint8_t nalUnitType = info->payload[0] & 0x1F;
                //处理h264的rtp包，可能会重组
                processH264RtpPacket(info->payload, info->timestamp, info->marker,nalUnitType, info->ssrc,m_out_queue_);
            } /*else if (info.payloadType == 97) { // AAC
                processAacPayload(info.payload);
            }*/
        }

    }
}
