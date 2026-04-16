#include "openglshow.h"
#include <QDebug>

OpenGlShow::OpenGlShow(QWidget *parent) : QOpenGLWidget(parent), texture(nullptr)
{
}

OpenGlShow::~OpenGlShow()
{
    makeCurrent();
    delete texture;
    delete shaderProgram;
    doneCurrent();
}

void OpenGlShow::updateFrame(const QImage &frame)
{
    QMutexLocker locker(&mutex);
    currentFrame = frame;
    update(); // 请求重绘
}

void OpenGlShow::initializeGL()
{
    initializeOpenGLFunctions();

    // 编译和链接Shader
    shaderProgram = new QOpenGLShaderProgram(this);
    shaderProgram->addShaderFromSourceCode(QOpenGLShader::Vertex,
        "attribute highp vec4 vertex;\n"
        "attribute highp vec2 texCoord;\n"
        "varying highp vec2 texc;\n"
        "void main(void)\n"
        "{\n"
        "    gl_Position = vertex;\n"
        "    texc = texCoord;\n"
        "}\n");
    shaderProgram->addShaderFromSourceCode(QOpenGLShader::Fragment,
        "uniform sampler2D texture;\n"
        "varying highp vec2 texc;\n"
        "void main(void)\n"
        "{\n"
        "    gl_FragColor = texture2D(texture, texc);\n"
        "}\n");
    shaderProgram->link();

    // 创建纹理对象
    texture = new QOpenGLTexture(QOpenGLTexture::Target2D);
}

void OpenGlShow::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
}

void OpenGlShow::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT);

    QImage frame;
    {
        QMutexLocker locker(&mutex);
        if (currentFrame.isNull())
            return;
        frame = currentFrame;
    }

    // 确保纹理存在且尺寸匹配
    if (!texture) {
        texture = new QOpenGLTexture(QOpenGLTexture::Target2D);
    }

    // 检查尺寸变化
    if (texture->width() != frame.width() || texture->height() != frame.height()) {
        // 销毁旧纹理的存储
        texture->destroy();
        // 设置新尺寸和格式（RGBA 格式，与 QImage::Format_RGB32 对应）
        texture->setFormat(QOpenGLTexture::RGBA8_UNorm);
        texture->setSize(frame.width(), frame.height());
        // 分配存储
        texture->allocateStorage();
    }

    // 更新纹理数据（假设 frame 格式是 RGB32，即 QImage::Format_RGB32）
    // 注意：setData 会自动将 QImage 转换为纹理需要的格式，但最好明确指定
    // 更新纹理数据
    texture->setData(frame); // 使用单参数版本

    // 绑定并绘制（其余部分不变）
    shaderProgram->bind();
    texture->bind();

    static const GLfloat vertices[] = {
        -1.0f, -1.0f,  0.0f,  1.0f,   // 左下角：纹理(0,1) -> 图像左上角
         1.0f, -1.0f,  1.0f,  1.0f,   // 右下角：纹理(1,1) -> 图像右上角
         1.0f,  1.0f,  1.0f,  0.0f,   // 右上角：纹理(1,0) -> 图像右下角
        -1.0f,  1.0f,  0.0f,  0.0f,   // 左上角：纹理(0,0) -> 图像左下角
    };
    int stride = 4 * sizeof(GLfloat);
    shaderProgram->setAttributeArray("vertex", vertices, 2, stride);
    shaderProgram->setAttributeArray("texCoord", vertices + 2, 2, stride);
    shaderProgram->enableAttributeArray("vertex");
    shaderProgram->enableAttributeArray("texCoord");

    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    texture->release();
    shaderProgram->release();
}
