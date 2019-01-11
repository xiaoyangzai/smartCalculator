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

#include <pthread.h>
#include <asm/types.h>        
#include <linux/videodev2.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "common.h"
#include "memory_pool.h"
#include "client.h"
#include "jpeg_util.h"
#include "video_util.h"

int main(int argc,char *argv[])
{
	if(argc != 6)
	{
		printf("usage: ./client [video device path] [IP] [Port] [Balance device path] [8888]\n");
		return -1;
	}
	//1. Main resource initilization.
	global_resource gres;
	init_global_resource(&gres,argv[2],atoi(argv[3]),1024*1024,224,224,argv[1],4,argv[4],atoi(argv[5]));

	pthread_t balance_pid,display_pid,control_pid;
	//2. Create balance module pthread.
	pthread_create(&display_pid,NULL,display_module_handle,(void *)&gres);
	//pthread_create(&balance_pid,NULL,balance_module_handle,(void *)&gres);
	pthread_join(display_pid,NULL);
	//pthread_join(balance_pid,NULL);
	//3. Release main resource.
	release_global_resource(&gres);	
	printf("Main process exits!\n");
	return 0;
}
