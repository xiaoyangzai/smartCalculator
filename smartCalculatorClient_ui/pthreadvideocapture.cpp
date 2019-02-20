#include <QImage>
#include "common.h"
#include "jpeg_util.h"
#include "video_util.h"
#include "widget.h"
#include "pthreadvideocapture.h"

pthreadVideoCapture::pthreadVideoCapture(Widget *pw ,QObject *parent) :
    QThread(parent)
{
    this->pw = pw;
}
void pthreadVideoCapture::run()
{
    while(1)
    {
        //采集图像
        if(pw->videoDevice.videofd > 0)
        {
            //采集一帧图像
            holder_next_frame(&(pw->videoDevice),pw->orignal_rgb24);
            //解码为JPEG
            pw->video_lock->lockForWrite();
            scale_rgb24(pw->orignal_rgb24,pw->resize_rgb24,pw->videoDevice.width,pw->videoDevice.height,224,224);
            pw->video_lock->unlock();
        }
        ::usleep(20*1000);
    }
}
