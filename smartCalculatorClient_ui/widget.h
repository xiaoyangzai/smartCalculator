#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QReadWriteLock>
#include <QTimer>
#include <mysql/mysql.h>
#include "video_util.h"
#include <stdint.h>
#include "common.h"
#include "memory_pool.h"
#pragma pack(1)
struct calculatorProtocol{
    //协议头: 固定0xEF
    uint8_t head;
    //协议类型:
    //C->S时，表示请求类型,0x01表示识别商品请求,length表示参数长度
    //S->C时，表示商品类型id，此时length值为0
    uint8_t type;
    int length;
};
#pragma pack()

namespace Ui {
class Widget;
}

class Widget : public QWidget
{
    Q_OBJECT
    
public:
    explicit Widget(const char *server_ip,short server_port,int balance_fd,int device_id,QWidget *parent = 0);
    ~Widget();
    void setStatue( QString text);
    void setClassLcd(QString text);
    void setPriceLcd(float val);
private slots:
    //接收电子秤设备读取的数据
    void slot_balance_msg(QString s);
    void slot_balance_update_weight(float weight);
    void slot_update_video();
    void on_numb1_clicked();

    void on_pbn_accept_clicked();

    void on_pbt_clear_clicked();

    void on_numb2_clicked();

    void on_numb3_clicked();

    void on_numb4_clicked();

    void on_numb5_clicked();

    void on_numb6_clicked();

    void on_numb7_clicked();

    void on_numb8_clicked();

    void on_numb9_clicked();

    void on_numb0_clicked();

    void on_numb_float_clicked();

private:
    Ui::Widget *ui;
public:
    int balance_fd;
    int server_fd;
    int device_id;
    QObject *parent;
    memory_pool_t *pool;
    VideoV4l2 videoDevice;
    MYSQL *mysql;
    //称重、状态显示读写锁
    QReadWriteLock *grw;
    //视频采集读写锁
    QReadWriteLock *video_lock;

    //手动模式
    volatile bool auto_mode;
    QString input_price;

    //加载摄像头图像定时器

    QTimer *timer_video;

    //商品类别识别定时器
    QTimer *timer_classify;
    //图像缓冲区
    uint8_t *orignal_rgb24,*resize_rgb24;

    //商品分类标志位
    volatile bool send_img;

};

#endif // WIDGET_H
