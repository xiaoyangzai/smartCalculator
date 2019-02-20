#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "common.h"
//================说明:无缓冲的输入输出函数=========================
ssize_t rio_readn(int fd,void *buf,size_t n)
{
	size_t nleft = n;
	ssize_t nread;
	char *bufp = (char *)buf;

    while(nleft > 0)
	{
		if((nread = read(fd,bufp,nleft)) < 0)
		{
			if(errno == EINTR)
				nread = 0;
			else
			    return -1;
		}
		else if(nread == 0)
			break;
		nleft -= nread;
		bufp += nread;
	}
	return (n - nleft);
}
ssize_t rio_writen(int fd,const void *buf,size_t n)
{
	size_t nleft = n;
	ssize_t nwritten;
	char *bufp = (char *)buf;
	while(nleft > 0)
	{
		if((nwritten = write(fd,bufp,nleft)) <= 0)
		{
			if(errno == EINTR)
				nwritten = 0;
			else
				return -1;
		}
		nleft -= nwritten;
		bufp += nwritten;
	}
	return n;
}
//==================带缓冲的输入输出函数==========================
void rio_readinitb(rio_t *rp,int fd)
{
	rp->rio_fd = fd;
	rp->rio_cnt = 0;
	rp->rio_bufptr = rp->rio_buf;
}



ssize_t rio_read(rio_t *rp,char *userbuf,size_t n)
{
	int cnt;
	//当缓冲区空时填满
	while(rp->rio_cnt <  RIO_BUFSIZE)
	{
		cnt = read(rp->rio_fd,rp->rio_bufptr,sizeof(rp->rio_buf) - rp->rio_cnt);
		if(cnt < 0)
		{
			if(errno == EINTR)
				return -1;
		}
		else if(cnt == 0)
			break;
		else
		{
			rp->rio_cnt += cnt;
			rp->rio_bufptr += cnt;
		}
	}
#ifdef DEBUG
	printf("rio_buff:\n");
	write(1,rp->rio_buf,rp->rio_cnt);
#endif
	if(rp->rio_cnt < (int)n)
		cnt = rp->rio_cnt;
	else
		cnt = n;
	memcpy(userbuf,rp->rio_buf,cnt);
	return rp->rio_cnt;
}
ssize_t rio_readlineb(rio_t *rp,void *userbuf,size_t maxlen)
{
	int n,rc;
	char c,*bufp = (char *)userbuf;

	for(n = 1;n < (int)maxlen; n++)
	{
		if((rc = rio_read(rp,&c,1)) == 1)
		{
			*bufp++ = c;
			if(c == '\n')
				break;
		}
		else if(rc == 0)
		{
			if(n == 1)
				return 0;		//EOF,no data was read
			else	
				break;			//EOF,some data was read;
		}
		else	
			return -1;			//error
	}
	*bufp ='\0';
	return n;
}

ssize_t rio_readnb(rio_t *rp,void *userbuf,size_t n)
{
	size_t nleft = n;
	ssize_t nread;
	char *bufp = (char *)userbuf;

	while(nleft > 0)
	{
		if((nread = rio_read(rp,bufp,nleft)) < 0)
		{
			if(errno == EINTR)
				nread = 0;	//重新读取
			else 
				return -1;
		}
		else if(nread == 0)
			break;			//EOF
		nleft -= nread;
		bufp += nread;
	}
	return (n - nleft);
}
//========================================================



//==========Timer (setitimer) function ==============

struct itimerval timeout,otimeout;
void (*osignalHandler)(int);
struct timer *gtimer;

static void signalHandler(int signo)
{
	gtimer->timerHandler(gtimer->arg);
}

void timerStart(struct timer *timer)
{
	gtimer = timer;
	//保存修改之前信号的处理函数
	osignalHandler = signal(SIGALRM,signalHandler);
	//清楚timeout
	CLEAR(timeout);
	//填充iteimerval定时结构体
	timeout.it_interval.tv_sec = timer->nextvalue;
	timeout.it_value.tv_sec = timer->expires;
	//安装SIGALRM信号的处理函数
	if(setitimer(ITIMER_REAL,&timeout,&otimeout) < 0)
		ERR("time start failed");
}

void timerStop(struct timer *time)
{
	
	//恢复SIGALRM信号的处理函数
	if(setitimer(ITIMER_REAL,&otimeout,NULL) < 0)
		ERR("time stop failed");
	signal(SIGALRM,osignalHandler);
	free(time);
}
struct timer *timerInit(int expires,int nextvalue,void(*fun)(void *),void *arg)
{
	struct timer *time =(struct timer *)malloc(sizeof(struct timer));
	time->nextvalue = nextvalue;
	time->expires = expires;
	time->timerHandler = fun;
	time->arg = arg;
	return time;
}

//IPC create
int create_semaphore(key_t key,int nums)
{
	int semid;
	if((semid = semget(key,nums,IPC_CREAT | IPC_EXCL|0666)) < 0)
		ERR("semget failed");
	return semid;
}

//IPC semaphore 
int open_semaphore(key_t key,int nums)
{
	int semid;
	if((semid = semget(key,nums,0666)) < 0)
		ERR("semget failed");
	return semid;
}

// P操作
int sem_P(int sem_id,int index)
{
	struct sembuf semarry;
	semarry.sem_num = index;
	semarry.sem_op = -1;
	semarry.sem_flg = SEM_UNDO;

	if(semop(sem_id,&semarry,1) < 0)
		ERR("sem P failed");
	return 0;
}

//V操作
int sem_V(int sem_id,int index)
{
	struct sembuf semarry;
	semarry.sem_num = index;
	semarry.sem_op = 1;
	semarry.sem_flg = SEM_UNDO;

	if(semop(sem_id,&semarry,1) < 0)
		ERR("semop V failed");
	return 0;
}
//delete semaphore
int del_sem(int sem_id)
{
	if(semctl(sem_id,0,IPC_RMID,0) < 0)
		ERR("semctl failed");
	return 0;
}
//set sem
int set_sem_val(int sem_id,int index,int setval)
{
	union semun arg;
	arg.val = setval;

	if(semctl(sem_id,index,SETVAL,arg) < 0)
		ERR("semctl failed");
	return 0;
}
//============socket========================
int open_listen(const char *ip,int port)
{
	//1、创建套接字
	int sockfd = socket(PF_INET,SOCK_STREAM,0);
	if(sockfd < 0)
		ERR("socket failed!");
	//2、填充地址结构体
	struct sockaddr_in seraddr;
	seraddr.sin_family = AF_INET;
	seraddr.sin_addr.s_addr = inet_addr(ip);
	seraddr.sin_port = htons(port);

	//开启socket心跳测试
	int opt = 1;
	if(setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE,&opt,sizeof(opt)) < 0)
		ERR("set socket failed");
	//设置地址复用，解决多个IP绑定在同个PORT上或处于TIME_WAIT状态的socket,实现socket复用
	opt = 1;
	if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,&opt,sizeof(opt)) < 0)
		ERR("set socket failed");

	//3、绑定套接字
	if(bind(sockfd,(struct sockaddr *)&seraddr,sizeof(struct sockaddr_in)) < 0)
		ERR("bind fialed");
//4、监听套接字
	if(listen(sockfd,10) < 0)
		ERR("listen failed");
	return sockfd;
}

const char * base64char = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

char * base64_encode( const unsigned char * bindata, char * base64, int binlength )
{
    int i, j;
    unsigned char current;

    for ( i = 0, j = 0 ; i < binlength ; i += 3 )
    {
        current = (bindata[i] >> 2) ;
        current &= (unsigned char)0x3F;
        base64[j++] = base64char[(int)current];

        current = ( (unsigned char)(bindata[i] << 4 ) ) & ( (unsigned char)0x30 ) ;
        if ( i + 1 >= binlength )
        {
            base64[j++] = base64char[(int)current];
            base64[j++] = '=';
            base64[j++] = '=';
            break;
        }
        current |= ( (unsigned char)(bindata[i+1] >> 4) ) & ( (unsigned char) 0x0F );
        base64[j++] = base64char[(int)current];

        current = ( (unsigned char)(bindata[i+1] << 2) ) & ( (unsigned char)0x3C ) ;
        if ( i + 2 >= binlength )
        {
            base64[j++] = base64char[(int)current];
            base64[j++] = '=';
            break;
        }
        current |= ( (unsigned char)(bindata[i+2] >> 6) ) & ( (unsigned char) 0x03 );
        base64[j++] = base64char[(int)current];

        current = ( (unsigned char)bindata[i+2] ) & ( (unsigned char)0x3F ) ;
        base64[j++] = base64char[(int)current];
    }
    base64[j] = '\0';
    return base64;
}
int base64_decode( const char * base64, unsigned char * bindata )
{
    int i, j;
    unsigned char k;
    unsigned char temp[4];
    for ( i = 0, j = 0; base64[i] != '\0' ; i += 4 )
    {
        memset( temp, 0xFF, sizeof(temp) );
        for ( k = 0 ; k < 64 ; k ++ )
        {
            if ( base64char[k] == base64[i] )
                temp[0]= k;
        }
        for ( k = 0 ; k < 64 ; k ++ )
        {
            if ( base64char[k] == base64[i+1] )
                temp[1]= k;
        }
        for ( k = 0 ; k < 64 ; k ++ )
        {
            if ( base64char[k] == base64[i+2] )
                temp[2]= k;
        }
        for ( k = 0 ; k < 64 ; k ++ )
        {
            if ( base64char[k] == base64[i+3] )
                temp[3]= k;
        }

        bindata[j++] = ((unsigned char)(((unsigned char)(temp[0] << 2))&0xFC)) |
                ((unsigned char)((unsigned char)(temp[1]>>4)&0x03));
        if ( base64[i+2] == '=' )
            break;

        bindata[j++] = ((unsigned char)(((unsigned char)(temp[1] << 4))&0xF0)) |
                ((unsigned char)((unsigned char)(temp[2]>>2)&0x0F));
        if ( base64[i+3] == '=' )
            break;

        bindata[j++] = ((unsigned char)(((unsigned char)(temp[2] << 6))&0xF0)) |
                ((unsigned char)(temp[3]&0x3F));
    }
    return j;
}

// host long 64 to network
uint64_t  htonll(uint64_t   host)   
{   
	uint64_t   ret = 0;   
	uint32_t   high,low;
	low   =   host & 0xFFFFFFFF;
	high   =  (host >> 32) & 0xFFFFFFFF;
	low   =   htonl(low);   
	high   =   htonl(high);   
	ret   =   low;
	ret   <<= 32;   
	ret   |=   high;   
	return   ret;   
}
//network to host long 64

uint64_t  ntohll(uint64_t   host)   
{   
	uint64_t   ret = 0;   
	uint32_t   high,low;
	low   =   host & 0xFFFFFFFF;
	high   =  (host >> 32) & 0xFFFFFFFF;
	low   =   ntohl(low);   
	high   =   ntohl(high);   

	ret   =   low;
	ret   <<= 32;   
	ret   |=   high;   
	return   ret;   
}
