#ifndef RTSPCLIENT_H
#define RTSPCLIENT_H

#include <QObject>
#include<QTcpSocket>
#include<QString>
#include<QUrl>
class RtspClient : public QObject
{
    Q_OBJECT
public:
    explicit RtspClient(QObject *parent = nullptr);
    void setUrl(const QUrl& url);
    void start();
signals:

public slots:
    void onConnected();
    void onReadyRead();

private:
    void connect();
    void request(const QString& method,const QString& url);
private:
    QTcpSocket m_socket_;
    QUrl m_url_;
};

#endif // RTSPCLIENT_H
