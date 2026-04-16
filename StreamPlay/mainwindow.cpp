#include "mainwindow.h"
#include "ui_mainwindow.h"
#include"QPushButton"
#include<QString>
#include<QThread>
#include<QDebug>
#include"rtsp.h"
#include"openglshow.h"
#include<QHBoxLayout>
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);


    //绑定按钮点击事件
    connect(ui->pushButton,&QPushButton::clicked,this,&MainWindow::play);
}
//播放rtsp流
void MainWindow::play(){
    //先获取到lineEdit字符串
    QString url=ui->lineEdit->text();
    if(url.isNull()||url.isEmpty()){
        qDebug()<<"lineEdit val is null or empty";
        return;
    }
    qDebug()<<"url:"<<url;

    //新起一个线程
    QThread* thread=new QThread{};

    OpenGlShow* show=new OpenGlShow{this};
    QHBoxLayout* layout=new QHBoxLayout{};
    ui->widget->setLayout(layout);
    layout->addWidget(show);

    RtspPlay* rtsp=new RtspPlay{url,show};
    //if(rtsp->rtspInit())return;
    rtsp->moveToThread(thread);
    connect(thread,&QThread::started,rtsp,&RtspPlay::play);
    /*connect(&rtsp, &RtspPlay::finished, &thread, &QThread::quit);
    connect(&rtsp, &RtspPlay::finished, &thread, &QObject::deleteLater);
    connect(&thread, &QThread::finished, &thread, &QObject::deleteLater);*/

    thread->start();
}
MainWindow::~MainWindow()
{
    delete ui;
}

