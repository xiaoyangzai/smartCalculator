#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

int main(int argc,char *argv[])
{
	if(argc != 2)
	{
		printf("usage: ./test [/dev/hx711]\n");
		return -1;
	}
	int fd = open(argv[1],O_RDONLY);
	if(fd < 0)
	{
		fprintf(stderr,"open failed: %s\n",strerror(errno));
		return -1;
	}
	printf("open successfully!!\n");
	int weight = 0;
	int n = 0;
	float base = 0.00248;
	int count = 20;
	while(count--)
	{
		printf("waiting something.....\n");
		n = read(fd,&weight,sizeof(weight));
		if(n < 0)
		{
			fprintf(stderr,"read failed: %s\n",strerror(errno));
			return -1;
		}
		printf("weight: %fg\n",(weight*base));
	}
	printf("final weight: %fg\n",(weight*base));
	close(fd);
	return 0;
}
