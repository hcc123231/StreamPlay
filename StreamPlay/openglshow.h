#ifndef OPENGLSHOW_H
#define OPENGLSHOW_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QImage>
#include<QMutex>

class OpenGlShow : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT
public:
    explicit OpenGlShow(QWidget *parent = nullptr);
    ~OpenGlShow();

public slots:
    // 用于接收解码线程传来的图像帧
    void updateFrame(const QImage &frame);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

private:
    QOpenGLShaderProgram *shaderProgram;
    QOpenGLTexture *texture;
    QImage currentFrame;
    QMutex mutex; // 保护currentFrame的互斥锁
};

#endif // OPENGLSHOW_H
