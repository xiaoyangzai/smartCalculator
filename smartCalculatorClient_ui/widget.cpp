#include "common.h"
#include "mysql/mysql.h"
#include <jpeg_util.h>
#include <cstdio>
#include "video_util.h"
#include "memory_pool.h"

#include "pthreadbalance.h"
#include "pthreadvideoclassify.h"
#include "pthreadvideocapture.h"
#include "widget.h"
#include "ui_widget.h"

Widget::Widget(const char *server_ip, short server_port, int balance_fd,int device_id, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Widget)
{
    ui->setupUi(this);
    auto_mode = true;
    send_img = false;
    this->pool = memory_pool_create(5*1024*1024);
    resize_rgb24 = (uint8_t*)memory_pool_alloc(pool,224*224*3);

    this->timer_video = new QTimer(this);
    this->showMaximized();
    grw = new QReadWriteLock();
    video_lock = new QReadWriteLock();
    ui->pla_videoimage->setScaledContents(true);
    QString video_load_failed_image_path = "./images/video_offline.jpg";
    QPixmap img(video_load_failed_image_path);
    img.scaled(ui->pla_videoimage->size());
    ui->pla_videoimage->setPixmap(img);
    this->balance_fd = balance_fd;
    this->device_id = device_id;

    //连接远程服务器，包括算法模型和数据库服务器
    int server_fd = ::socket(PF_INET,SOCK_STREAM,0);
    if(server_fd < 0)
    {
        ui->ple_statue->setText(QString("socket failed: %1").arg(strerror(errno)));
        exit(-1);
    }
    struct sockaddr_in serverInfo;
    serverInfo.sin_family = AF_INET;
    serverInfo.sin_port = htons(server_port);
    serverInfo.sin_addr.s_addr = inet_addr(server_ip);
    if(::connect(server_fd,(struct sockaddr *)&serverInfo,sizeof(serverInfo)) < 0)
    {
        ui->ple_statue->setText(QString(QObject::tr("手动模式已开启")));
        server_fd = -1;
    }
    if(server_fd > 0)
        ui->ple_statue->setText(QString(QObject::tr("智能模式已开启")));
    else
        auto_mode = false;

    this->server_fd = server_fd;

    this->mysql = NULL;
    if(server_fd > 0)
    {
        //连接数据库服务器
        MYSQL *mysql;
        if((mysql = ::mysql_init(NULL)) == NULL)
        {
            ui->ple_statue->setText(QString("Connect to Mysql server failed: %1").arg(::mysql_error(mysql)));
        }
        if(NULL == ::mysql_real_connect(mysql,"192.168.1.104","root","123456","wangyang",0,NULL,0))
        {
            ui->ple_statue->setText(QString("Connect to Mysql server failed: %1").arg(::mysql_error(mysql)));
        }
        int ret = ::mysql_query(mysql,"set names utf8");
        if(ret != 0)
        {
            ui->ple_statue->setText(QString("Initlizating Mysql server failed: %1").arg(::mysql_error(mysql)));
        }
        this->mysql = mysql;
    }

    //创建内存池
    this->parent = parent;


    //打开摄像头设备
    int video_fd = ::open("/dev/video0",O_RDWR);
    this->videoDevice.videofd = video_fd;
    if(video_fd > 0)
    {
        init_v4l2_device(&videoDevice,5,pool);
        orignal_rgb24 = (uint8_t *)memory_pool_alloc(pool,videoDevice.width*videoDevice.height*3);
        QString video_load_failed_image_path = "./images/video_online.jpg";
        QPixmap img(video_load_failed_image_path);
        img.scaled(ui->pla_videoimage->size());
        ui->pla_videoimage->setPixmap(img);
        //启动线程完成视频图像的采集
        pthreadVideoCapture *pvcap = new pthreadVideoCapture(this);
        pvcap->start();
        timer_video = new QTimer();
        QObject::connect(timer_video,SIGNAL(timeout()),this,SLOT(slot_update_video()));
        timer_video->start(50);

    }
    this->mysql = mysql;
    if(server_fd > 0 && video_fd > 0)
    {
        pthreadVideoClassify *pvc = new pthreadVideoClassify(this);
        pvc->start();
    }

    PthreadBalance *ptbalance = new PthreadBalance(balance_fd);
    QObject::connect(ptbalance,SIGNAL(signal_msg(QString)),this,SLOT(slot_balance_msg(QString)));
    QObject::connect(ptbalance,SIGNAL(signal_update_weight(float)),this,SLOT(slot_balance_update_weight(float)));
    ptbalance->start();
}

void Widget::setStatue(QString text)
{
    ui->ple_statue->setText(text);
}
void Widget::setClassLcd(QString text)
{
    ui->ple_classid->setText(text);
}
void Widget::setPriceLcd(float val)
{
    ui->ple_price->display(val);
}
void Widget::slot_update_video()
{
    video_lock->lockForRead();
    QImage img(resize_rgb24,224,224,QImage::Format_RGB888);
    video_lock->unlock();
    ui->pla_videoimage->setPixmap(QPixmap::fromImage(img.scaled(ui->pla_videoimage->size())));
}

void Widget::slot_balance_msg(QString s)
{
    grw->lockForWrite();
    ui->ple_statue->setText(s);
    grw->unlock();
}
void Widget::slot_balance_update_weight(float weight)
{
    //加载重量
    //grw->lockForWrite();
    int val = weight ;
    ui->ple_weight->display(val/1000.0);
    //grw->unlock();
    if(weight <= 0)
        send_img = false;
    else
    {
        send_img = true;

    }
     ui->ple_total->display(ui->ple_price->value()*ui->ple_weight->value());
}
Widget::~Widget()
{
    delete ui;
    ::release_v4l2_device(&videoDevice);
    if(server_fd > 0)
        ::close(server_fd);
    ::close(balance_fd);
    ::memory_pool_destroy(pool);
    if(mysql)
        ::mysql_close(mysql);
}

void Widget::on_numb1_clicked()
{
    if(auto_mode)
    {
        input_price.clear();
        //grw->lockForWrite();
        ui->ple_statue->setText(QString(QObject::tr("手动模式已开启")));
        //grw->unlock();
        auto_mode = false;
    }

    //grw->lockForWrite();
    input_price.append("1");
    bool ok = false;
    float tmp = input_price.toDouble(&ok);
    if(ok)
        ui->ple_price->display(tmp);
    //grw->unlock();

    grw->lockForRead();
    double weight = ui->ple_weight->value();
    ui->ple_total->display(weight * ui->ple_price->value());
    grw->unlock();
}


void Widget::on_pbn_accept_clicked()
{
    if(auto_mode == false)
    {
        if(server_fd > 0)
        {
            auto_mode = true;
            //grw->lockForWrite();
            ui->ple_statue->setText(QString(QObject::tr("自动模式已开启")));
            //grw->unlock();
        }
        input_price.clear();
    }
}

void Widget::on_pbt_clear_clicked()
{
    if(auto_mode == false)
    {
        ui->ple_price->display(0.0);
        ui->ple_total->display(0.0);
        input_price.clear();
    }
}

void Widget::on_numb2_clicked()
{
    if(auto_mode)
    {
        input_price.clear();
        //grw->lockForWrite();
        ui->ple_statue->setText(QString(QObject::tr("手动模式已开启")));
        //grw->unlock();
        auto_mode = false;
    }

    //grw->lockForWrite();
    input_price.append("2");
    bool ok = false;
    float tmp = input_price.toDouble(&ok);
    if(ok)
        ui->ple_price->display(tmp);
    //grw->unlock();

    //grw->lockForRead();
    double weight = ui->ple_weight->value();
    ui->ple_total->display(weight * ui->ple_price->value());
    //grw->unlock();
}

void Widget::on_numb3_clicked()
{
    if(auto_mode)
    {
        input_price.clear();
        //grw->lockForWrite();
        ui->ple_statue->setText(QString(QObject::tr("手动模式已开启")));
       // grw->unlock();
        auto_mode = false;
    }

    //grw->lockForWrite();
    input_price.append("3");
    bool ok = false;
    float tmp = input_price.toDouble(&ok);
    if(ok)
        ui->ple_price->display(tmp);
    //grw->unlock();

    grw->lockForRead();
    double weight = ui->ple_weight->value();
    ui->ple_total->display(weight * ui->ple_price->value());
    grw->unlock();
}

void Widget::on_numb4_clicked()
{
    if(auto_mode)
    {
        input_price.clear();
        //grw->lockForWrite();
        ui->ple_statue->setText(QString(QObject::tr("手动模式已开启")));
        //grw->unlock();
        auto_mode = false;
    }

    //grw->lockForWrite();
    input_price.append("4");
    bool ok = false;
    float tmp = input_price.toDouble(&ok);
    if(ok)
        ui->ple_price->display(tmp);
    ////grw->unlock();

    grw->lockForRead();
    double weight = ui->ple_weight->value();
    ui->ple_total->display(weight * ui->ple_price->value());
    grw->unlock();
}

void Widget::on_numb5_clicked()
{
    if(auto_mode)
    {
        input_price.clear();
        //grw->lockForWrite();
        ui->ple_statue->setText(QString(QObject::tr("手动模式已开启")));
        //grw->unlock();
        auto_mode = false;
    }

    //grw->lockForWrite();
    input_price.append("5");
    bool ok = false;
    float tmp = input_price.toDouble(&ok);
    if(ok)
        ui->ple_price->display(tmp);
    //grw->unlock();

    grw->lockForRead();
    double weight = ui->ple_weight->value();
    ui->ple_total->display(weight * ui->ple_price->value());
    grw->unlock();
}

void Widget::on_numb6_clicked()
{
    if(auto_mode)
    {
        input_price.clear();
        //grw->lockForWrite();
        ui->ple_statue->setText(QString(QObject::tr("手动模式已开启")));
        //grw->unlock();
        auto_mode = false;
    }

    //grw->lockForWrite();
    input_price.append("6");
    bool ok = false;
    float tmp = input_price.toDouble(&ok);
    if(ok)
        ui->ple_price->display(tmp);
    //grw->unlock();

    grw->lockForRead();
    double weight = ui->ple_weight->value();
    ui->ple_total->display(weight * ui->ple_price->value());
    grw->unlock();
}

void Widget::on_numb7_clicked()
{
    if(auto_mode)
    {
        input_price.clear();
        //grw->lockForWrite();
        ui->ple_statue->setText(QString(QObject::tr("手动模式已开启")));
        //grw->unlock();
        auto_mode = false;
    }

    //grw->lockForWrite();
    input_price.append("7");
    bool ok = false;
    float tmp = input_price.toDouble(&ok);
    if(ok)
        ui->ple_price->display(tmp);
    //grw->unlock();

    grw->lockForRead();
    double weight = ui->ple_weight->value();
    ui->ple_total->display(weight * ui->ple_price->value());
    grw->unlock();
}

void Widget::on_numb8_clicked()
{
    if(auto_mode)
    {
        input_price.clear();
        //grw->lockForWrite();
        ui->ple_statue->setText(QString(QObject::tr("手动模式已开启")));
        //grw->unlock();
        auto_mode = false;
    }

    //grw->lockForWrite();
    input_price.append("8");

    bool ok = false;
    float tmp = input_price.toDouble(&ok);
    if(ok)
    {

        ui->ple_price->display(tmp);
    }
    //grw->unlock();

    grw->lockForRead();
    double weight = ui->ple_weight->value();
    ui->ple_total->display(weight * ui->ple_price->value());
    grw->unlock();
}

void Widget::on_numb9_clicked()
{
    if(auto_mode)
    {
        input_price.clear();
        //grw->lockForWrite();
        ui->ple_statue->setText(QString(QObject::tr("手动模式已开启")));
        //grw->unlock();
        auto_mode = false;
    }

    //grw->lockForWrite();
    input_price.append("9");

    bool ok = false;
    float tmp = input_price.toDouble(&ok);
    if(ok)
        ui->ple_price->display(tmp);
    //grw->unlock();

    grw->lockForRead();
    double weight = ui->ple_weight->value();
    ui->ple_total->display(weight * ui->ple_price->value());
    grw->unlock();
}

void Widget::on_numb0_clicked()
{
    if(auto_mode)
    {
        input_price.clear();
        //grw->lockForWrite();
        ui->ple_statue->setText(QString(QObject::tr("手动模式已开启")));
        //grw->unlock();
        auto_mode = false;
    }

    //grw->lockForWrite();
    input_price.append("0");
    bool ok = false;
    float tmp = input_price.toDouble(&ok);
    if(ok)
        ui->ple_price->display(tmp);
    //grw->unlock();

    grw->lockForRead();
    double weight = ui->ple_weight->value();
    ui->ple_total->display(weight * ui->ple_price->value());
    grw->unlock();
}

void Widget::on_numb_float_clicked()
{
    if(auto_mode)
    {
        input_price.clear();
        //grw->lockForWrite();
        ui->ple_statue->setText(QString(QObject::tr("手动模式已开启")));
        //grw->unlock();
        auto_mode = false;
    }
    input_price.append(".");
    bool ok = false;
    float tmp = input_price.toDouble(&ok);
    if(ok)
        ui->ple_price->display(tmp);
    //grw->unlock();

}
