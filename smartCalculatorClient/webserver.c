#include <openssl/sha.h>  
#include <openssl/buffer.h>  
#include <openssl/bio.h>  
#include <openssl/evp.h>
#include <string.h>
#include <unistd.h>
#include <jpeglib.h>
#include "common.h" 
#include "memory_pool.h"
#include "jpeg_util.h"
#include "client.h"
#include "websocketprotocl.h"
#include "webserver.h"


//doResponseWebSocekt处理websocket连接
int doResponseWebSocket(char *reqPtr,int fd,global_resource *gres)
{
	printf("=======  websocket request  =========\n");
	memory_pool_t *pool = gres->pool;
	char destString[MAXLINE] = "Sec-WebSocket-Key: ";
	char WebSocketHeader[MAXLINE] = {0};

	char *p = strstr(reqPtr,destString);
	
	p = p + strlen(destString);
	char *end = strstr(p,"\r\n");
	*end = '\0';
	shakeHands(p,WebSocketHeader);
	printf("=======WebSocketHeader========\n%s\n",WebSocketHeader);
	
	//发送响应消息头
	if(write(fd,WebSocketHeader,strlen(WebSocketHeader)) < 0)
		ERR("send header failed");

	printf("Carmer broadcast model.....\n");
	uint8_t* jpeg_data = NULL;
	uint64_t jpeg_len = 0;
	while(1)
	{

		pthread_rwlock_rdlock(&gres->rw_weight_mtx);
#ifdef DISPLAY_MODULE_DEBUG
		switch(gres->class_id)
		{
			case 0:
				printf("ClassID: %s\tWeight: %.3fkg\tPrice: %f\r","黄瓜",gres->weight,gres->price);
				break;
			case 1:
				printf("ClassID: %s\tWeight: %.3fkg\tPrice: %f\r","青椒",gres->weight,gres->price);
				break;
			case 255:
				printf("ClassID: %s\tWeight: %.3fkg\tPrice: %f\r","空",gres->weight,gres->price);
				break;
			default:
				printf("ClassID: %s\tWeight: %.3fkg\tPrice: %f\r","未知",gres->weight,gres->price);
				break;
		}
		fflush(stdout);
#endif
		pthread_rwlock_unlock(&gres->rw_weight_mtx);

		//采集图片
		pthread_rwlock_wrlock(&gres->rw_image_mtx);
		holder_next_frame(&(gres->camera),gres->rgb24);
		pthread_rwlock_unlock(&gres->rw_image_mtx);

		//解码为JPEG
		if(encode_jpeg(gres->rgb24,gres->camera.width,gres->camera.height,&jpeg_data,&jpeg_len,pool) != 0)
		{
			printf("transform from RGB24 to JPEG failed!!\n");		
			break;
		}

		//发送websoket协议头	
#ifdef DEBUG
		printf("Image size: %d\n",jpeg_len);
#endif
		sendWebSocketHeader(fd,BINARY_CODE,jpeg_len);

		//发送图像
		int n = write(fd,jpeg_data,jpeg_len);
		if(n < 0)
			ERR("write failed");
		if(n != jpeg_len)
			ERR("write failed");
		usleep(200*1000);
#ifdef DEBUG
		printf("image send successfully!!\n");
#endif
	}
	close(fd);
	return 0;
}

/** 
  +------------------------------------------------------------------------------ 
 * @desc            : 握手数据解析, 并返回校验数据 
 +------------------------------------------------------------------------------ 
 * @access          : public 
 * @author          : bottle<bottle@fridayws.com> 
 * @since           : 16-05-11 
 * @param           : const char* data 需要校验的数据 
 * @param           : char* request 可发送回客户端的已处理数据, 需要预先分配内存 
 * @return          : 0 
 +------------------------------------------------------------------------------ 
**/  
int shakeHands(const char* data, char* request)  
{
	uint8_t val[256] = {0};  
	memset(val, 0, 256);  
	strcat((char *)val,(char *)data);
	strcat((char *)val,"258EAFA5-E914-47DA-95CA-C5AB0DC85B11");  
	printf("val = %s\n",val);
	uint8_t mt[SHA_DIGEST_LENGTH] = {0};  
	char accept[256] = {0};  
	SHA1(val, strlen((char *)val), mt);  
	memset(accept, 0, 256);  
	base64_encode(mt,accept,20);  
	memset(request, 0, 1024);  
	sprintf(request, "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: %s\r\nSec-webSocket-Version: 13\r\nServer: Bottle-websocket-server\r\n\r\n",accept);  
	return 0;  
}


//doit函数处理一个HTTP事务,首先读取和解析请求行，使用rio_readlineb函数读取请求
void display_webserver(int fd,global_resource *gres)
{
	int is_static;
	struct stat sbuf;
	char buf[MAXLINE],method[MAXLINE],uri[MAXLINE],version[MAXLINE];

	char filename[MAXLINE],cgiargs[MAXLINE];

	if(0)
	{
		//发送响应报头给客户端
		sprintf(buf,"HTTP/1.1 200 OK\r\n");
		sprintf(buf,"%s Server: Tiny Web Server\r\n",buf);
		sprintf(buf,"%sContent-length: 0\r\n",buf);
		sprintf(buf,"%sAccess-Control-Allow-Origin: *\r\n",buf);
		sprintf(buf,"%sAccess-Control-Allow-Methods: POST, GET, OPTIONS\r\n",buf);
		sprintf(buf,"%sAccess-Control-Allow-Headers: X-PINGOTHER, Content-Type\r\n",buf);
		sprintf(buf,"%sAccess-Control-Max-Age: 86400\r\n",buf);
		sprintf(buf,"%sContent-type: text/plain\r\n\r\n",buf);
		rio_writen(fd,buf,strlen(buf));
		printf("first response to client!!\n");
	}

	memset(buf,0,sizeof(buf));
	memory_pool_t *pool = gres->pool;

	//读取请求和http头
	int n = 0;
	int total = 0;
	while(1)
	{
		n = read(fd,buf + total,1);
		if(n < 0)
			ERR("read failed");	
		if(n == 0)
			break;
		total++;
		if(total > 4 && strcmp(buf + total - 4,"\r\n\r\n") == 0)
			break;
	}
	if(total < 4)
	{
		printf("No data has recieved!! Client has offline\n");
		close(fd);		
		return;
	}
	printf("====== client request header ======= \n");
	write(1,buf,total);
	char *p = strstr(buf,"\n");
	*p = 0;
	sscanf(buf,"%s %s %s",method,uri,version);
	if(strcasecmp(method,"GET"))
	{
		clienterror(fd,method,"501","Not Implemented","Tiny does not implement this method");
		return;
	}
	if(strstr(p + 1,"websocket"))
	{
		printf("===== client websocket request =====\n%s\n",p + 1);
		//websoket请求
		doResponseWebSocket(p + 1,fd,gres);
		return;
	}

	printf("common connection request!!\n");
	//解析GET请求中的URI
	//根据解析确定请求的是否是静态内容还是动态内容
	is_static = parse_uri(uri,filename,cgiargs);
	
	if(stat(filename,&sbuf) < 0)
	{
		clienterror(fd,filename,"404","Not Found","Tiny couldn't read the file");
		return;
	}
	
	if(is_static)
	{
		//文件是否是普通文件，当前用户是否有可读权限
		if(!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode))
		{
			clienterror(fd,filename,"403","Forbidden","tiny couldn't read the file");
			return;
		}
		printf("response the request!!\n");
		serve_static(fd,filename,sbuf.st_size);
	}
	else
	{
		//文件是否是普通文件，当前用户是否有执行权限
		if(!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode))
		{
			clienterror(fd,filename,"403","Forbidden","Tiny couldn't run the CGI program");
			return;
		}
		serve_dynamic(fd,filename,cgiargs);
	}

}

void clienterror(int fd,const char *cause,const char *errnum,const char *shortmsg,const char *longmsg)
{
	char buf[MAXLINE],body[MAXLINE];

	//创建http body
	sprintf(body,"<html><title>Tiny Error</title>");
	sprintf(body,"%s<body bgcolor=""ffffff""> \r\n",body);
	sprintf(body,"%s%s:%s\r\n",body,errnum,shortmsg);
	sprintf(body,"%s<p>%s:%s\r\n",body,longmsg,cause);
	sprintf(body,"%s","<hr><em>The Tiny Web server</em>\r\n");

	//填充http请求包头
	sprintf(buf,"HTTP/1.0 %s %s \r\n",errnum,shortmsg);
	rio_writen(fd,buf,strlen(buf));

	sprintf(buf,"Content-type:text/html\r\n");
	sprintf(buf,"Content-length:%d\r\n\r\n",(int)strlen(body));
	rio_writen(fd,buf,strlen(buf));
	rio_writen(fd,body,strlen(body));

}

int read_requesthdrs(rio_t *rp)
{
	char buf[MAXLINE];
	int webSocketFlag = 0;
	while(strcmp(buf,"\n"))
	{
		rio_readlineb(rp,buf,MAXLINE);
		if(strstr(buf,"websocket") != NULL)
			webSocketFlag = 1;
	}
	return webSocketFlag;
}
//解析一个HTTP URI
int parse_uri(char *uri,char *filename,char *cgiargs)
{
	char *ptr;
	if(!strstr(uri,"cgi-bin"))
	{
		//静态文本请求
		strcpy(cgiargs,"");
		//从当前目录中指定的子目录中打开
		strcpy(filename,"./www");
		strcat(filename,uri);
		if(uri[strlen(uri) - 1] == '/')
			strcat(filename,"index.html");
		return 1;
	}
	else
	{
		//动态页面
		ptr = index(uri,'?');
		if(ptr)
		{
			strcpy(cgiargs,ptr+1);
			*ptr = '\0';
		}
		else
			strcpy(cgiargs,"");
		strcpy(filename,".");
		strcat(filename,uri);
		return 0;
	}
}

//Tiny 提供四种不同类型的静态内容:HTML 文件、无格式的文本文件，编码为GIF和JPEG格式的图片.这些文件类型占据web上提供的绝大部分静态内容
//server_static函数发送一个HTTP响应，其主体包含一个本地文件的内容；首先通过检测文件名的后缀来判断文件的类型,并且发送响应行和响应报头给客户端,注意用一个空行终止报头

void serve_static(int fd,char *filename,int filesize)
{
	int srcfd;
	char filetype[MAXLINE],buf[MAXLINE];

	//发送响应报头给客户端
	get_filetype(filename,filetype);

	sprintf(buf,"HTTP/1.1 200 OK\r\n");
	sprintf(buf,"%s Server: Tiny Web Server\r\n",buf);
	sprintf(buf,"%sContent-length:%d\r\n",buf,filesize);
	sprintf(buf,"%sConnection: keep-alive\r\n",buf);
	sprintf(buf,"%sContent-type: %s\r\n\r\n",buf,filetype);

	rio_writen(fd,buf,strlen(buf));

	//发送HTTP内容给客户端
	srcfd = open(filename,O_RDONLY);
	if(srcfd < 0)
	{
		clienterror(fd,"Not found","404","thr page not fond","try again");
		return;
	}
	int n = 0;
	while((n = read(srcfd,buf,sizeof(buf))) > 0)
		write(fd,buf,n);
	close(fd);
	printf("response finished!!\n");
}

//获取文件的后缀名（文件类型）

void get_filetype(char *filename,char *filetype)
{
	if(strstr(filename,".html"))
		strcpy(filetype,"text/html");
	else if(strstr(filename,".gif"))
		strcpy(filetype,"image/gif");
	else if(strstr(filename,".jpg"))
		strcpy(filetype,"image/jpeg");
	else if(strstr(filename,".ico"))
		strcpy(filetype,"image/x-icon");
	else
		strcpy(filetype,"text/plain");
}

//serve_dynamic函数一开始就向客户端发送一个表明成功的响应行，同时还包括带有信息的server报头
//CGI程序负责发送响应的剩余部分。
void serve_dynamic(int fd,char *filename,char *cgiargs)
{
	char buf[MAXLINE],*emptylist[] = {NULL};

	//发送HTTP响应报头
	sprintf(buf,"HTTP/1.0 200 OK\r\n");
	rio_writen(fd,buf,strlen(buf));
	sprintf(buf,"Server:Tiny Web Server\r\n");
	rio_writen(fd,buf,strlen(buf));

	if(fork() == 0)
	{
		printf("child cgiargs = %s\n",cgiargs);
		if(setenv("QUERY_STRING",cgiargs,1) < 0)
			ERR("setenv failed");
		dup2(fd,STDOUT_FILENO);
		execve(filename,emptylist,environ);
	}
	wait(NULL);
}
 
