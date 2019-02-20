#include<sys/ioctl.h>
#include "pthreadbalance.h"

PthreadBalance::PthreadBalance(int balance_fd,QObject *parent) :
    QThread(parent)
{
    this->balance_fd = balance_fd;
}
void PthreadBalance::run()
{
    int weight = 0;
    float base = 0.00248;
    int n = 0;
    while(1)
    {
         n = ::read(balance_fd,&weight,sizeof(weight));
         if(weight*base != 0.0)
         {
             //1: 降低称重值,0: 增加称重值
             if(weight *base > 0.0)
                ioctl(balance_fd,1,200);
             else
                 ioctl(balance_fd,0,200);
             emit signal_update_weight(weight*base);
         }
         else
             break;
    }
    while(1)
    {
        n = ::read(balance_fd,&weight,sizeof(weight));
        if(n < 0)
            emit signal_msg(QString("Balance read failed"));
        else
            emit signal_update_weight(weight*base);
    }
}
