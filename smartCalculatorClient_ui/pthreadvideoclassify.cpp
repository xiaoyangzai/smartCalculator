#include <cstdio>
#include "mysql/mysql.h"
#include "common.h"
#include "jpeg_util.h"
#include "video_util.h"
#include "widget.h"
#include "pthreadvideoclassify.h"

pthreadVideoClassify::pthreadVideoClassify(Widget* pw,QObject *parent) :
    QThread(parent)
{
    this->pw = pw;
}
void pthreadVideoClassify::run()
{
    calculatorProtocol pack;
    ::usleep(100*1000);
	while(1)
	{
		pack.head = 0xEF;
		pack.type = 0x01;
		pack.length = htonl(224*224*3);
		//发送商品图像，接收检测结果
		if(pw->auto_mode && pw->send_img)
		{
			//发送协议头
			if(::write(pw->server_fd,&pack,sizeof(pack)) < 0 )
			{
				pw->auto_mode = false;
				continue;
			}
			//发送将要识别的图像
			pw->video_lock->lockForRead();
			if(::write(pw->server_fd,pw->resize_rgb24,224*224*3) < 0)
			{
				pw->video_lock->unlock();
				pw->auto_mode = false;
				continue;
			}
			pw->video_lock->unlock();
			//接收解析结果
			if(::read(pw->server_fd,&pack,sizeof(pack)) <= 0)
			{
				pw->auto_mode = false;
                pw->setStatue(QString(QObject::tr("服务器离线\n手动模式已开启")));
				::close(pw->server_fd);
				pw->server_fd = -1;
				continue;
			}
			//显示类别名称与单价

			char buf[256];
			sprintf(buf,"select name,price from vegetable where id = %d",pack.type);

			int ret = mysql_query(pw->mysql,buf);
			if(ret != 0)
			{
				pw->auto_mode = false;
                pw->setStatue(QString(QObject::tr("数据库服务器离线\n手动模式已开启")));

				continue;
			}
			MYSQL_RES *result = mysql_store_result(pw->mysql);
			if(result == NULL)
			{
				pw->auto_mode = false;
                pw->setStatue(QString(QObject::tr("数据库服务器离线\n手动模式已开启")));
				continue;
			}
			MYSQL_ROW row = mysql_fetch_row(result);
			pw->grw->lockForWrite();
            pw->setClassLcd(row[0]);
            pw->setPriceLcd(QString(row[1]).toFloat());
			pw->grw->unlock();
		}
	}
}
