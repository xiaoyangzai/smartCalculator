#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <time.h>
#include "common.h"
#include "memory_pool.h"
#include "jpeg_util.h"
#include "video_util.h"
#include "webserver.h"
#include "client.h"


int init_global_resource(global_resource * resource,const char *server_ip,short server_port,size_t pool_size,uint32_t resize_width,uint32_t resize_height,const char *camera_path,int frame_buff_count,const char *balance_path,short web_port,const char *mysql_ip)
{
	assert(resource != NULL);
	resource->pool = memory_pool_create(pool_size);
	assert(resource->pool != NULL);
	resource->class_id = 255;
	resource->price = 0.0;
	resource->resize_width = resize_width;
	resource->resize_height = resize_height;
	resource->resize_rgb24 = (uint8_t*)memory_pool_alloc(resource->pool,resize_width*resize_height*3);
	resource->camera.videofd = open(camera_path,O_RDWR);
	if(resource->camera.videofd < 0)
		ERR("open camera module failed\n");

	resource->balance_fd = open(balance_path,O_RDONLY);
	if(resource->balance_fd <  0)
		ERR("open balance module failed\n");

	resource->web_port = web_port;
	init_v4l2_device(&(resource->camera),frame_buff_count,resource->pool);

	resource->rgb24 = (uint8_t*)memory_pool_alloc(resource->pool,resource->camera.width*resource->camera.height*3);
	assert(resource->rgb24 != NULL);

	resource->server_ip = server_ip;
	resource->server_port = server_port;
	resource->accept_flag = 0;

	if(pthread_rwlock_init(&(resource->rw_image_mtx),NULL)!=0)
		ERR("pthread rwlock initialized failed");
	if(pthread_rwlock_init(&(resource->rw_weight_mtx),NULL)!=0)
		ERR("pthread rwlock initialized failed");
	if(pthread_rwlock_init(&(resource->rw_accept_mtx),NULL)!=0)
		ERR("pthread rwlock initialized failed");
	printf("Main Resource is initialized!\n");
	resource->websockfd = -1;

	//连接数据库
	printf("Mysql connect....\r");
	if(mysql_init(&resource->mysql) == NULL)
	{
		printf("mysql initialize failed: %s\n",mysql_error(&resource->mysql));
		exit(-1);
	}
	if(mysql_real_connect(&resource->mysql,mysql_ip,"root","123456","wangyang",0,NULL,0) == NULL)
	{
		printf("connect to mysql server failed: %s\n",mysql_error(&resource->mysql));
		exit(-1);
	}
	int ret = mysql_query(&resource->mysql,"set names utf8");
	if(ret != 0)
	{
		printf("set names utf8 failed: %s\n",mysql_error(&resource->mysql));
		return -1;
	}

	printf("Mysql connect ok!\n");
	return 0;
}
int release_global_resource(global_resource * resource)
{
	release_v4l2_device(&(resource->camera));
	close(resource->camera.videofd);
	close(resource->balance_fd);
	memory_pool_destroy(resource->pool);
	pthread_rwlock_destroy(&(resource->rw_image_mtx));
	pthread_rwlock_destroy(&(resource->rw_weight_mtx));
	mysql_close(&resource->mysql);
	printf("Main Resource has been cleaned up!\n");
	return 0;
}

void *display_module_handle(void *arg)
{
	global_resource *gres = (global_resource *)arg;	
	//建立web服务器
	int listenfd = open_listen("0.0.0.0",gres->web_port);	
	int connfd;
	struct sockaddr_in clientaddr ;
	socklen_t addrlen;
	addrlen = sizeof(clientaddr);
	printf("Display module starts!\n");
	while(1)
	{
#ifdef DISPLAY_MODULE_DEBUG
		printf("Waiting for connection from client.....\n");
#endif
		connfd = accept(listenfd,(struct sockaddr *)&clientaddr,&addrlen);
#ifdef DISPLAY_MODULE_DEBUG
		printf("New client has been coming!!\n");
#endif
		display_webserver(connfd,gres);
		close(connfd);
	}
	printf("Display module exits!\n");
	pthread_exit(NULL);
}

void *balance_module_handle(void *arg)
{
	global_resource *gres = (global_resource *)arg;	
	int sockfd = socket(PF_INET,SOCK_STREAM,0);
	if(sockfd < 0)
		ERR("socket failed");

	struct sockaddr_in serverInfo;
	serverInfo.sin_family = AF_INET;
	serverInfo.sin_port = htons(gres->server_port);
	serverInfo.sin_addr.s_addr = inet_addr(gres->server_ip);

	//连接远程服务器
	printf("connect to server....\n");
	if(connect(sockfd,(struct sockaddr *)&serverInfo,sizeof(serverInfo)) < 0)
		ERR("connect failed");
	printf("connect to server....OK\n");

	calculatorProtocol *pack = (calculatorProtocol *)memory_pool_alloc(gres->pool,sizeof(calculatorProtocol));

	int tmp_weight = 0;
	int n = 0;
	int weight_delay = 10; 
	float base = 0.00294;
	printf("Balance module start!\n");
	while(1)
	{
		//称重
		weight_delay = 10;

#ifdef BALANCE_MODULE_DEBUG
		printf("start to balance something....\n");
#endif
		while(weight_delay--)
		{
			n = read(gres->balance_fd,&tmp_weight,sizeof(tmp_weight));
			if(n < 0)
			{
				if(errno == EAGAIN)
				{
					weight_delay++;
					continue;
				}
				else
					ERR("read balance failed");
			}
			if(n == 0)
			{
				printf("read balance no content!\n");
				break;
			}
			pthread_rwlock_wrlock(&gres->rw_weight_mtx);
			gres->weight = tmp_weight*base/1000;
			pthread_rwlock_unlock(&gres->rw_weight_mtx);
#ifdef BALANCE_MODULE_DEBUG
			printf("%fg\r",tmp_weight*base);
#endif
			fflush(stdout);
		}
		if(gres->weight <= 0.0050)
		{

			pthread_rwlock_wrlock(&gres->rw_weight_mtx);
			gres->class_id = 255;	
			pthread_rwlock_unlock(&gres->rw_weight_mtx);
			continue;
		}
#ifdef BALANCE_MODULE_DEBUG
		printf("Final weight:  %fkg\n",gres->weight);
#endif

		pthread_rwlock_rdlock(&gres->rw_accept_mtx);
		int accept_flag = gres->accept_flag;
		pthread_rwlock_unlock(&gres->rw_accept_mtx);
		if(accept_flag)
		{
			pthread_rwlock_wrlock(&gres->rw_accept_mtx);
			gres->accept_flag = 0;
			pthread_rwlock_unlock(&gres->rw_accept_mtx);
			char time_buf[20];
			char buf[100];
			float weight = gres->weight;
			float price = gres->price;
			int classid = gres->class_id;
			if(classid == 255)
				continue;
			//插入数据库
			time_t tt;
			time(&tt);
			struct tm *nt = localtime(&tt);
			sprintf(time_buf,"%d-%d-%d %d:%d:%d",nt->tm_year+1900,nt->tm_mon + 1,nt->tm_mday,nt->tm_hour,nt->tm_min,nt->tm_sec);
			sprintf(buf,"insert into orders(vegetable_id,weights,price,order_time) values(%d,%.3f,%.3f,\"%s\")",classid,weight,price,time_buf);

#ifdef BALANCE_MODULE_DEBUG
			printf("%s\n",buf);
#endif

			int ret = mysql_query(&gres->mysql,buf);
			if(ret != 0)
			{
				printf("insert failed: %s\n",mysql_error(&gres->mysql));
				exit(-1);
			}
#ifdef BALANCE_MODULE_DEBUG
			printf("Order info, classid: %d\tweight: %.3f\tprice: %.3f\ttotal: %.3f\n",gres->class_id,gres->weight,gres->price,gres->weight*gres->price);
#endif

			pthread_rwlock_wrlock(&gres->rw_weight_mtx);
			gres->class_id = 255;
			pthread_rwlock_unlock(&gres->rw_weight_mtx);
		}
#ifdef BALANCE_MODULE_DEBUG
		printf("Capture the image....\n");
#endif
		//发送到服务器
		pack->head = 0xEF;
		pack->type = 0x01;
		pack->length = htonl(gres->resize_width*gres->resize_height*3);
		if(write(sockfd,pack,sizeof(calculatorProtocol)) != sizeof(calculatorProtocol))
			ERR("send package failed");


		//缩放图像
		pthread_rwlock_rdlock(&gres->rw_image_mtx);
		scale_rgb24(gres->rgb24,gres->resize_rgb24,gres->camera.width,gres->camera.height,gres->resize_width,gres->resize_height);
		pthread_rwlock_unlock(&gres->rw_image_mtx);
#ifdef BALANCE_MODULE_DEBUG
		printf("send the image....\n");
#endif
		n = write(sockfd,gres->resize_rgb24,gres->resize_width*gres->resize_height*3);
		if(n < 0)
			ERR("send image data failed");

#ifdef BALANCE_MODULE_DEBUG
		printf("send image[%d bytes] OK! Waiting for result....\n",n);
#endif
		fflush(stdout);
		//接收分析结果
		n = read(sockfd,pack,sizeof(calculatorProtocol));
		if(n < 0)
			ERR("read failed");
		if(n == 0)
		{
#ifdef BALANCE_MODULE_DEBUG
			printf("detection server is offline\n");
#endif
			pthread_rwlock_wrlock(&gres->rw_weight_mtx);
			pack->length = 0;
			pack->type = 255;
			pthread_rwlock_unlock(&gres->rw_weight_mtx);
		}
		else
		{
#ifdef BALANCE_MODULE_DEBUG
			printf("recv %d bytes\n",n);
			printf("class: %d\n",pack->type);
			printf("head: %x\n",pack->head);
#endif
			pthread_rwlock_wrlock(&gres->rw_weight_mtx);
			gres->class_id = pack->type;
			pthread_rwlock_unlock(&gres->rw_weight_mtx);
			char buf[256];
			sprintf(buf,"select price from vegetable where id = %d",gres->class_id);

#ifdef BALANCE_MODULE_DEBUG
			printf("%s\n",buf);
#endif

			int ret = mysql_query(&gres->mysql,buf);
			if(ret != 0)
			{
				printf("query failed: %s\n",mysql_error(&gres->mysql));
				exit(-1);
			}
			MYSQL_RES	*result = NULL;
			result = mysql_store_result(&gres->mysql);
			if(result == NULL)
			{
				printf("mysql store result failed: %s\n",mysql_error(&gres->mysql));
				exit(-1);
			}
			MYSQL_ROW row = mysql_fetch_row(result);
			pthread_rwlock_wrlock(&gres->rw_weight_mtx);
			gres->price =atof(row[0]); 
			pthread_rwlock_unlock(&gres->rw_weight_mtx);
			mysql_free_result(result);

#ifdef BALANCE_MODULE_DEBUG
			printf("price: %.3f\n",gres->price);
#endif
		}
		fflush(stdout);
	}
	printf("balance module will exit!!\n");
	close(sockfd);
	pthread_exit(NULL);
}

//Control module pthread function.
void *control_module_handle(void *arg)
{
	global_resource *gres = (global_resource *)arg;
	int fd = gres->websockfd;
	char buf[256] = {0};
	int n = 0;
	printf("Control module starts!\n");
	while(1)
	{
#ifdef CONTROL_MODULE_DEBUG
		printf("wait for data....\n");
#endif
		n = read(fd,buf,sizeof(buf));
		if(n == 0)
		{
			printf("server has closed!\n");
			break;
		}
		if(n < 0)
			ERR("read failed");
		printf("[Control module]ensure order!\n");

		pthread_rwlock_wrlock(&gres->rw_accept_mtx);
		gres->accept_flag = 1;
		pthread_rwlock_unlock(&gres->rw_accept_mtx);

	}
	printf("Control module will exit!\n");
	pthread_exit(NULL);
}
