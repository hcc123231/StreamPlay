QT       += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    openglshow.cpp \
    rtsp.cpp \
    rtspclient.cpp \
    rtspreceiver.cpp \
    taskread.cpp \
    threadqueue.cpp

HEADERS += \
    mainwindow.h \
    openglshow.h \
    rtsp.h \
    rtspclient.h \
    rtspreceiver.h \
    taskread.h \
    threadqueue.h

FORMS += \
    mainwindow.ui


INCLUDEPATH += D:\ffmpeg-8.1-full_build-shared\include
# 2. 直接链接 .dll.a 文件（使用绝对路径或相对路径）
LIBS += D:\ffmpeg-8.1-full_build-shared\lib\libavformat.dll.a
LIBS += D:\ffmpeg-8.1-full_build-shared\lib\libavcodec.dll.a
LIBS += D:\ffmpeg-8.1-full_build-shared\lib\libavutil.dll.a
LIBS += D:\ffmpeg-8.1-full_build-shared\lib\libavdevice.dll.a
LIBS += D:\ffmpeg-8.1-full_build-shared\lib\libavfilter.dll.a
LIBS += D:\ffmpeg-8.1-full_build-shared\lib\libswscale.dll.a
LIBS += D:\ffmpeg-8.1-full_build-shared\lib\libswresample.dll.a

# 3. 添加 Windows 系统库（FFmpeg 内部需要）
LIBS += -lws2_32 -lbcrypt -lsecur32 -lole32 -lstrmiids -luuid
# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
