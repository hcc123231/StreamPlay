#ifndef RTSPRECEIVER_H
#define RTSPRECEIVER_H

#include <QObject>

class RtspReceiver : public QObject
{
    Q_OBJECT
public:
    explicit RtspReceiver(QObject *parent = nullptr);

signals:

public slots:
};

#endif // RTSPRECEIVER_H
