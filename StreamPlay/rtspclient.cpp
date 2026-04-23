#include "rtspclient.h"
#include<QDebug>
#include<QRegularExpression>
#include<QtGlobal>
RtspClient::RtspClient(ThreadQueue<QByteArray*>* queue,QObject *parent) : QObject(parent),m_socket_{nullptr},m_cseq_{0},m_interaction_status_{STATE_NULL},m_rtp_pkts_{queue}
{
    m_socket_=new QTcpSocket{};
    QObject::connect(m_socket_,&QTcpSocket::connected,this,&RtspClient::onConnected);
    QObject::connect(m_socket_,&QTcpSocket::readyRead,this,&RtspClient::onReadyRead);
    QObject::connect(m_socket_, SIGNAL(error(QAbstractSocket::SocketError)),this, SLOT(onError(QAbstractSocket::SocketError)));
}

void RtspClient::setUrl(const QUrl &url)
{
    m_url_=url;
}

void RtspClient::start()
{
    //启动流程
    //先检查m_url_是否为空
    //qDebug()<<"uurl:"<<m_url_;
    if(m_url_.isEmpty()){
        qDebug()<<"url is empty";
        return;
    }
    //开始连接
    connect();
}

void RtspClient::connect()
{
    //qDebug()<<"host:"<<m_url_.host()<<" port:"<<(m_url_.port()==-1?554:m_url_.port());
    m_socket_->connectToHost(m_url_.host(),static_cast<quint16>(m_url_.port()==-1?554:m_url_.port()));
}

void RtspClient::request(const QString& method,const QUrl& url,const void* args)
{
    m_cseq_++;
    if(method.isEmpty()||url.isEmpty()){
        qDebug()<<"RtspClient::request invalid params";
        return;
    }
    //构造请求行
    QString requestion=QString("%1 rtsp://%2:%3%4 RTSP/1.0\r\n").arg(method).arg(url.host()).arg(url.port()).arg(url.path());
    //构造请求头
    requestion+=QString("CSeq: %1\r\n").arg(m_cseq_);
    switch (m_interaction_status_) {
    case STATE_OPTIONS:
    {
        requestion+=QString("\r\n");
        break;
    }
    case STATE_DESCRIBE:
    {
        requestion+=QString("Accept: application/sdp\r\n\r\n");
        break;
    }
    case STATE_SETUP:
    {
        const char* type=static_cast<const char*>(args);
        if(strcmp(type,"video")==0){
            requestion+=QString("Transport: RTP/AVP/TCP;unicast;interleaved=%1-%2\r\n\r\n").arg(0).arg(1);
        }else if(strcmp(type,"audio")==0){
            requestion+=QString("Transport: RTP/AVP/TCP;unicast;interleaved=%1-%2\r\n").arg(2).arg(3);
            if(m_sessionId_.isEmpty()){
                qDebug()<<"session id is empty";
                return;
            }
            requestion+=QString("Session: %1\r\n\r\n").arg(m_sessionId_);
        }
        break;
    }
    case STATE_PLAY:
    {
        const char* session=static_cast<const char*>(args);
        //qDebug()<<"session~~~~~~~~~~~:"<<session;
        requestion+=QString("Session: %1\r\n").arg(session);
        requestion+=QString("Range: npt=0.000-\r\n\r\n");
        break;
    }

    }
    //写入
    m_socket_->write(requestion.toUtf8());
    qDebug()<<"requestion:"<<requestion;
}

void RtspClient::onConnected()
{
    qDebug()<<"onConnected success";
    //连接成功后主动与服务端交互
    //先询问服务端支持哪些rtsp方法
    m_interaction_status_=STATE_OPTIONS;
    request("OPTIONS",m_url_,nullptr);

}
//解析sdp
static QList<MediaInfo> parse_sdp(const QString& sdp) {
    QList<MediaInfo> medias;
    QStringList lines = sdp.split("\r\n", QString::SkipEmptyParts);
    MediaInfo current;
    bool inMedia = false;

    for (const QString& line : lines) {
        if (line.startsWith("m=")) {
            // 保存上一个媒体段
            if (inMedia && !current.m_control_url_.isEmpty()) {
                medias.append(current);
            }
            // 开始新段
            current = MediaInfo();
            inMedia = true;
            // 解析 m= 行，提取媒体类型 (video/audio)
            QStringList parts = line.split(' ');
            if (parts.size() >= 1) {
                QString mLine = parts[0];          // "m=video"
                current.m_type_ = mLine.mid(2);    // 去掉 "m=" -> "video"
            }
        }
        else if (inMedia && line.startsWith("a=control:")) {
            current.m_control_url_ = line.mid(10); // 去掉 "a=control:"
        }
        else if (inMedia && line.startsWith("a=rtpmap:")) {
            current.m_rtpmap_ = line.mid(9);
        }

        // 可继续添加其他属性 (fmtp 等)
        else if (inMedia && line.startsWith("a=fmtp:")) {
            current.m_fmtp_ = line.mid(7); // 去掉 "a=fmtp:"
            // 如果是视频且编码为 H264，解析 sprop-parameter-sets
            if (current.m_type_ == "video" && current.m_rtpmap_.contains("H264")) {
                // 示例: a=fmtp:96 packetization-mode=1;sprop-parameter-sets=Z2QAKKw7...,aO484Q...
                QRegularExpression re("sprop-parameter-sets=([^,]+),?([^;]*)");
                QRegularExpressionMatch match = re.match(current.m_fmtp_);
                if (match.hasMatch()) {
                    QString spsBase64 = match.captured(1);
                    QString ppsBase64 = match.captured(2);
                    current.m_sps = QByteArray::fromBase64(spsBase64.toLatin1());
                    current.m_pps = QByteArray::fromBase64(ppsBase64.toLatin1());
                }
            }
        }
    }
    // 添加最后一个
    if (inMedia && !current.m_control_url_.isEmpty()) {
        medias.append(current);
    }
    return medias;
}
static void rtpPacketGet(){

}
struct AudioSetupArgs{
    QString m_type_;
    QString m_session_;
};

void RtspClient::onReadyRead()
{
    qDebug()<<"RtspClient::onReadyRead";
    //先一次性将socket缓冲区数据拿到
    QByteArray read_data=m_socket_->readAll();
    //数据push进缓冲区
    m_rbuf_.append(read_data);
    if(m_interaction_status_==STATE_FINISH){
        //截取rtp包
        int ppos=m_rbuf_.indexOf('$');
        //qDebug()<<"ppos:"<<ppos;
        if(ppos==-1)return;
        //qDebug()<<"m_buf size:"<<m_rbuf_.size();
        if(m_rbuf_.size()<4)return;
        //直接找到第3个字节，往后取两个字节就是rtp包长度
        unsigned char high = static_cast<unsigned char>(m_rbuf_.at(ppos+2));
        unsigned char low  = static_cast<unsigned char>(m_rbuf_.at(ppos+3));
        quint16 length = static_cast<quint16>((high << 8) | low);
        //qDebug()<<"lenght:"<<length;
        if(m_rbuf_.size()<4+length){
            qDebug()<<"less";
            return;
        }

        unsigned char channel=static_cast<unsigned char>(m_rbuf_.at(ppos+1));
        //printf("channel::::0x%02X\n", channel);
        if(channel==0x00){
            qDebug()<<"rtp包";
            //入队
            QByteArray array=m_rbuf_.mid(ppos+4,length);
            QByteArray* pptr=new QByteArray(array);
            m_rtp_pkts_->enqueue(pptr);
        }
        m_rbuf_.remove(0, ppos + 4 + length);
    }
    else{
        //在m_rbuf_中找到第一个\r\n\r\n
        int head_end_idx=m_rbuf_.indexOf("\r\n\r\n");
        if(head_end_idx==-1){
            //一个\r\n\r\n都没有，直接结束本次解析，等待更多数据
            return;
        }
        //拿到rtsp头
        QByteArray head_data=m_rbuf_.left(head_end_idx+2);
        //qDebug()<<"head_data:---"<<QString(head_data);
        int head_data_len=head_data.length();
        //qDebug()<<"head_data_len:"<<head_data_len;
        //再分别拿到每一行
        QList<QByteArray> lines;
        int start_pos=0;
        while(1){
            int cur_pos=head_data.indexOf("\r\n",start_pos);
            if(cur_pos==-1)break;
            QByteArray byte_array=head_data.mid(start_pos,cur_pos-start_pos);
            lines.push_back(byte_array);
            start_pos=cur_pos+2;
            if(start_pos>=head_data_len)break;
        }
        //先保存响应行信息
        int state_code=lines[0].split(' ')[1].toInt();
        //然后根据得到的lines解析头部
        QMap<QString,QString> headers;
        for(int i=1;i<lines.size();i++){
            QString line=lines[i];
            int colon=line.indexOf(':');
            QString key=line.left(colon).trimmed();
            QString value=line.mid(colon+1).trimmed();
            headers.insert(key,value);
        }
        //开始接收body部分
        int content_length=0;
        if(headers.contains("Content-Length")){
            content_length=headers["Content-Length"].toInt();
            if(m_rbuf_.size()<head_end_idx+4+content_length)return;
        }
        QByteArray body=m_rbuf_.mid(head_end_idx+4,content_length);
        //qDebug()<<"body:"<<QString(body);
        //移除已读数据
        m_rbuf_.remove(0,head_end_idx+content_length+4);
        if(state_code==200){
            //正常响应
            switch (m_interaction_status_) {
            case STATE_NULL:
            {
                qDebug()<<"interaction state is NULL";
                return;
            }
            case STATE_OPTIONS:
            {
                //保存options能力
                QString options=headers["Public"];
                QStringList sl=options.split(',');
                for(int i=0;i<sl.size();i++){
                    m_options_.push_back(sl[i].trimmed());
                }
                //然后发起下一个请求,describe
                if(m_options_.indexOf("DESCRIBE")!=-1){
                    m_interaction_status_=STATE_DESCRIBE;
                    request("DESCRIBE",m_url_,nullptr);
                }
                break;
            }
            case STATE_DESCRIBE:
            {
                //解析describe响应体
                QList<MediaInfo> media_infos=parse_sdp(body);
                if(!media_infos.empty()){
                    emit meidainfo(media_infos);
                }
                m_interaction_status_=STATE_SETUP;
                for(const auto& item:media_infos){
                    if(item.m_type_=="video"){
                        if(m_options_.indexOf("SETUP")!=-1)
                            request("SETUP",item.m_control_url_,static_cast<const void*>("video"));
                    }else if(item.m_type_=="audio"){
                        /*if(m_options_.indexOf("SETUP")!=-1)
                        request("SETUP",item.m_control_url_,static_cast<const void*>("audio"));*/
                        m_audio_ctl_url_=item.m_control_url_;
                    }
                }

                break;
            }
            case STATE_SETUP:
            {

                if(headers.contains("Session")){
                    const char* sess=headers["Session"].toStdString().c_str();
                    m_sessionId_=sess;
                    request("SETUP",m_audio_ctl_url_,"audio");
                    m_interaction_status_=STATE_PLAY;

                    //qDebug()<<"sess==============:"<<sess;
                    request("PLAY",m_url_,static_cast<const void*>(sess));

                }


                break;
            }

            case STATE_PLAY:
            {
                qDebug()<<"Play response";
                m_interaction_status_=STATE_FINISH;//rtsp结束状态
                break;
            }

            }
        }

    }
}
void RtspClient::onError(QAbstractSocket::SocketError error)
{
    qDebug()<<"RtspClient::onError: "<<error;
}


