#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <getopt.h>           
#include <fcntl.h>            
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <asm/types.h>        
#include <linux/videodev2.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "memory_pool.h"
#include "jpeg_util.h"
#include "video_util.h"

#define SYS_ERR(s) do{    \
	fprintf(stderr,"[%s:%d] %s:%s\n",__FILE__,__LINE__,s,strerror(errno));\
	exit(-1);\
}while(0)


#pragma pack(1)
typedef struct _protocol{
	uint8_t head;
	uint8_t type;
	int length;
}calculatorProtocol;
#pragma pack()


int main(int argc,char *argv[])
{
	if(argc != 4)
	{
		printf("usage: ./client [video device path] [IP] [Port]\n");
		return -1;
	}
	memory_pool_t *pool = memory_pool_create(10*1024*1024);
	if(pool == NULL)
	{
		printf("create memory pool failed\n");
		return -1;
	}
	VideoV4l2 video;
	video.videofd= open(argv[1],O_RDWR);
	if(video.videofd < 0)
		SYS_ERR("open failed");

	init_v4l2_device(&video,4,pool);
	printf("init video Ok!\n");	
	int sockfd = socket(PF_INET,SOCK_STREAM,0);
	if(sockfd < 0)
		SYS_ERR("socket failed");

	struct sockaddr_in serverInfo;
	serverInfo.sin_family = AF_INET;
	serverInfo.sin_port = htons(atoi(argv[3]));
	serverInfo.sin_addr.s_addr = inet_addr(argv[2]);

	uint8_t *rgb24 = (uint8_t *)memory_pool_alloc(pool,video.width*video.height*3);
	uint8_t *resizedata = (uint8_t *)memory_pool_alloc(pool,224*224*3);
	if(rgb24 == NULL || resizedata == NULL)
	{
		printf("memory pool allocate failed\n");
		exit(-1);
	}
	printf("hold the next frame....\n");
	holder_next_frame(&video,rgb24);

	//连接远程服务器
	printf("connect to server....\n");
	if(connect(sockfd,(struct sockaddr *)&serverInfo,sizeof(serverInfo)) < 0)
		SYS_ERR("connect failed");
	printf("connect to server....OK\n");

	calculatorProtocol *pack = (calculatorProtocol *)memory_pool_alloc(pool,sizeof(calculatorProtocol));


	while(1)
	{

		pack->head = 0xEF;
		pack->type = 0x01;
		pack->length = htonl(224*224*3);
		if(write(sockfd,pack,sizeof(calculatorProtocol)) != sizeof(calculatorProtocol))
			SYS_ERR("send package failed");

		holder_next_frame(&video,rgb24);
		scale_rgb24(rgb24,resizedata,video.width,video.height,224,224);
		
		//uint32_t w,h,bpp;
		//decode_jpeg("./1.jpg",resizedata, &w,&h,&bpp,pool);
		int n = 0;
		if((n = write(sockfd,resizedata,224 * 224*3)) < 0)
			SYS_ERR("send image data failed");
		printf("send %d byets!\n",n);
		//保存发送的图片到本地
#ifdef DEBUG
		write_JPEG_file ("1.jpg", rgb24,video.width,video.height,100);
#endif
		break;
	}
	int n = read(sockfd,pack,sizeof(calculatorProtocol));
	if(n < 0)
		SYS_ERR("read failed");
	if(n == 0)
		printf("server offlin\n");
	else
	{
		printf("recv %d bytes\n",n);
		printf("class: %d\n",pack->type);
		printf("head: %x\n",pack->head);
	}
	release_memory(&video);
	memory_pool_destroy(pool);
	close(sockfd);
	close(video.videofd);
	return 0;
}
