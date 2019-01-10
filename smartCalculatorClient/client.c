#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include "common.h"
#include "jpeg_util.h"
#include "client.h"


int init_global_resource(global_resource * resource,const char *server_ip,short server_port,size_t pool_size,uint8_t sem_numbs,uint32_t resize_width,uint32_t resize_height,key_t sem_key,const char *camera_path,int frame_buff_count,const char *balance_path)
{
	assert(resource != NULL);
	resource->pool = memory_pool_create(pool_size);
	assert(resource->pool != NULL);
	resource->class_id = 255;
	resource->price = 0.0;
	resource->resize_width = resize_width;
	resource->resize_height = resize_height;
	resource->resize_rgb24 = (uint8_t*)memory_pool_alloc(resource->pool,resize_width*resize_height*3);
	resource->sem_key = sem_key;
	resource->sem_id = create_semaphore(resource->sem_key,sem_numbs);

	resource->camera.videofd = open(camera_path,O_RDWR);
	if(resource->camera.videofd < 0)
		ERR("open camera module failed\n");

	resource->balance_fd = open(balance_path,O_RDONLY);
	if(resource->balance_fd <  0)
		ERR("open balance module failed\n");
	init_v4l2_device(&(resource->camera),frame_buff_count,resource->pool);

	resource->rgb24 = (uint8_t*)memory_pool_alloc(resource->pool,resource->camera.width*resource->camera.height*3);
	assert(resource->rgb24 != NULL);

	resource->server_ip = server_ip;
	resource->server_port = server_port;
	printf("Main Resource is initialized!\n");
	return 0;
}
int release_global_resource(global_resource * resource)
{
	del_sem(resource->sem_id);
	release_v4l2_device(&(resource->camera));
	close(resource->camera.videofd);
	close(resource->balance_fd);
	memory_pool_destroy(resource->pool);
	printf("Main Resource has been cleaned up!\n");
	return 0;
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

	while(1)
	{

		pack->head = 0xEF;
		pack->type = 0x01;
		pack->length = htonl(gres->resize_width*gres->resize_height*3);
		if(write(sockfd,pack,sizeof(calculatorProtocol)) != sizeof(calculatorProtocol))
			ERR("send package failed");

		holder_next_frame(&(gres->camera),gres->rgb24);
		scale_rgb24(gres->rgb24,gres->resize_rgb24,gres->camera.width,gres->camera.height,gres->resize_width,gres->resize_height);
		
		int n = 0;
		if((n = write(sockfd,gres->resize_rgb24,gres->resize_width*gres->resize_height*3)) < 0)
			ERR("send image data failed");
		printf("send %d byets!\n",n);
		//保存发送的图片到本地
#ifdef DEBUG
		write_JPEG_file ("1.jpg", gres->rgb24,gres->camera.width,gres->camera.height,100);
#endif
		break;
	}
	int n = read(sockfd,pack,sizeof(calculatorProtocol));
	if(n < 0)
		ERR("read failed");
	if(n == 0)
		printf("server offlin\n");
	else
	{
		printf("recv %d bytes\n",n);
		printf("class: %d\n",pack->type);
		printf("head: %x\n",pack->head);
	}
	close(sockfd);
	pthread_exit(NULL);
}
