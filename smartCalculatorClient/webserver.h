#ifndef __WEBSERVER_H__
#define __WEBSERVER_H__
#include <unistd.h>
#include "client.h"
extern char **environ;
//处理客户端请求
//参数：fd表示于客户端交互套接字

int doResponseWebSocket(char *reqPtr,int fd,global_resource *gres);
void display_webserver(int fd,global_resource *gres);
void clienterror(int fd,const char *cause,const char *errnum,const char *shortmsg,const char *longmsg);

int read_requesthdrs(rio_t *rp);
int parse_uri(char *uri,char *filename,char *cgiargs);
void serve_static(int fd,char *filename,int filesize);

void get_filetype(char *filename,char *filetype);
void serve_dynamic(int fd,char *filename,char *cgiargs);

int shakeHands(const char* data, char* request);
#endif
