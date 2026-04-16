#include "rtspclient.h"
#include<QDebug>
RtspClient::RtspClient(QObject *parent) : QObject(parent),m_socket_{}
{
    QObject::connect(&m_socket_,&QTcpSocket::connected,this,&RtspClient::onConnected);
    QObject::connect(&m_socket_,&QTcpSocket::readyRead,this,&RtspClient::onReadyRead);
}

void RtspClient::setUrl(const QUrl &url)
{
    m_url_=url;
}

void RtspClient::start()
{
    //启动流程
    //先检查m_url_是否为空
    if(m_url_.isEmpty()||m_url_.isValid()){
        qDebug()<<"url is invalid";
        return;
    }
    //开始连接
    connect();
}

void RtspClient::connect()
{
    m_socket_.connectToHost(m_url_.host(),static_cast<quint16>(m_url_.port()==-1?554:m_url_.port()));
}

void RtspClient::request(const QString& method,const QString& url)
{
    if(method.isEmpty()||url.isEmpty()){
        qDebug()<<"RtspClient::request invalid params";
        return;
    }
}

void RtspClient::onConnected()
{
    qDebug()<<"onConnected success";
    //连接成功后主动与服务端交互
    //先询问服务端支持哪些rtsp方法
    request("OPTIONS",m_url_.path());

}

void RtspClient::onReadyRead()
{
    qDebug()<<"RtspClient::onReadyRead";
}


