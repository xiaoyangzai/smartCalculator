#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include "common.h"
#include "memory_pool.h"
#include "jpeg_util.h"
#include "video_util.h"
#include "webserver.h"
#include "client.h"


int init_global_resource(global_resource * resource,const char *server_ip,short server_port,size_t pool_size,uint32_t resize_width,uint32_t resize_height,const char *camera_path,int frame_buff_count,const char *balance_path,short web_port)
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
	printf("Main Resource is initialized!\n");
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
	addrlen = sizeof(clientaddr);
	printf("Waiting for connection from client.....\n");
	connfd = accept(listenfd,(struct sockaddr *)&clientaddr,&addrlen);
	printf("New client has been coming!!\n");

	display_webserver(connfd,gres);
	close(connfd);


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
	float base = 0.00248;
	while(1)
	{
		weight_delay = 10;
		//称重

		//printf("start to balance something....\n");
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
				printf("server offline!\n");
				break;
			}
			pthread_rwlock_wrlock(&gres->rw_weight_mtx);
			gres->weight = tmp_weight*base/1000;
			pthread_rwlock_unlock(&gres->rw_weight_mtx);
			//printf("%fg\n",tmp_weight*base);
			usleep(20*1000);
			fflush(stdout);
		}
		if(gres->weight <= 0.0010)
			continue;
#ifdef BALANCE_MODULE_DEBUG
		printf("Final weight:  %fkg\n",gres->weight);
#endif
		if(gres->accept_flag )
			continue;

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
			printf("server offline\n");
			break;
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
			gres->price =ntohl(pack->length)/100.0; 
			pthread_rwlock_unlock(&gres->rw_weight_mtx);
			//printf("ClassID: %d\tPrice: %.3fRMB\t Weight: %.3fkg\n",gres->class_id,gres->price,gres->weight);
		}
		fflush(stdout);
	}
	printf("balance module will exit!!\n");
	close(sockfd);
	pthread_exit(NULL);
}
