#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/wait.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <stdint.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/mman.h>
#include <netinet/in.h>

#include <sys/select.h>




#define SHA_DIGEST_LENGTH 20
#define ERR(s) do{fprintf(stderr,"[%s:%d]%s : %s\n",__FILE__,__LINE__,s,strerror(errno));exit(-1);}while(0)

//不带缓冲的输入输出函数
ssize_t rio_readn(int fd,void *buf,size_t n);
ssize_t rio_writen(int fd,const void *buf,size_t len);

//带缓冲的输入输出函数

//问题：编写一个程序看来计算文本文件中文本行的数量该如何来实现？
//答案1：用read函数一次一个字节的从文件中读取到进程中，检查每个字节来查找换行符；缺点是效率不高，每读取文件中的一个字节都要陷入内核
//答案2：调用一个包装函数在一个缓冲区中拷贝一个文本行，当缓冲区为空，会自动调用read函数填满缓冲区.



//函数名:rio_readinitb
//描述：创建一个空的读缓冲区，并且将一个打开的文件描述符和这个缓冲区联系起来

#define RIO_BUFSIZE 8192
typedef struct {
	int rio_fd; 				//将要与缓冲区建立联系的文件描述符
	int rio_cnt;				//缓冲区中未读取的字节数
	char *rio_bufptr;			//下次读取字节的位置
	char rio_buf[RIO_BUFSIZE];	//内部缓冲区
}rio_t;

void rio_readinitb(rio_t *rp,int fd);

//函数名:rio_readlineb
//描述：从rp所指向的缓冲区中读取一个文本行，userbuf表示用户存储区域地址，maxlen表示可接收最大字节数
ssize_t rio_readlineb(rio_t *rp,void *userbuf,size_t maxlen);

//函数名rio_readnb
//描述：从缓冲区中读取n个字节，存储到userbuf所指向的空间
ssize_t rio_readnb(rio_t  *rp,void *userbuf,size_t n);

//函数名:rio_read
//描述：从缓冲区读取文本文件函数
ssize_t rio_read(rio_t *rp,char *uerbuf,size_t n);

//函数名:rio_readnb
//描述：从缓冲区中读取n个字节
ssize_t rio_readnb(rio_t *rp,void *userbuf,size_t n);

union semun{
	int val;
};

#define MAXLINE 1024

#define CLEAR(n) memset(&n,0,sizeof(n))


//==================Timer setitimer===================
struct timer{
	void (*timerHandler)(void *);	//保存处理函数的地址
	void *arg;		//超时处理函数的参数
	int expires;	//第一次定时间
	int nextvalue;	//超时之后间隔时间
};

struct timer *timerInit(int expires,int nextvalue,void(*fun)(void *),void *arg);
void timerStart(struct timer *timer);
void timeStop(struct timer * timer);

int create_semaphore(key_t key,int nums);
int open_semaphore(key_t key,int nums);
int sem_P(int sem_id,int index);
int sem_V(int sem_id,int index);
int del_sem(int sem_id);
int set_sem_val(int sem_id,int index,int setval);

//网络通信

int open_listen(const char *ip,int port);


//加密算法

char * base64_encode( const unsigned char * bindata, char * base64, int binlength );
//解密算法
int base64_decode( const char * base64, unsigned char * bindata );
#endif


uint64_t  ntohll(uint64_t   host);
uint64_t  htonll(uint64_t   host);
