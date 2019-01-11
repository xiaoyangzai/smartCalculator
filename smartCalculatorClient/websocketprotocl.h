#ifndef __WEBSOCKETPROTOCL_H__
#define __WEBSOCKETPROTOCL_H__

#include <stdint.h>

//是否结束数据包
#define IS_LASTPOCKET(code) (code & 0x80)

//获取数据包类型
#define OPCODE(code)		(code & 0x0F)

#define ADDITION_CODE		0x0
#define TEXT_CODE			0x1
#define BINARY_CODE			0x2
#define CLOSE_CODE			0x8
#define PING_CODE			0x9
#define PONG_CODE			0xA

//是否数据经过掩码
#define IS_SETMASK(code)	(code & 0x80)
//发送协议包头部
int sendWebSocketHeader(int clientfd,uint8_t type,uint64_t len);
//获取数据长度
int getPocketDataSize(uint8_t *data,uint32_t *size);

//获取掩码值，并且存储到数组中
int getMaskKey(uint8_t *mask,uint8_t maskKey[]);

//解掩码或掩码
//转换后的结果存储在data中
int transformData(uint8_t *data,uint32_t len,uint8_t maskKey[],uint8_t *outBuff); 

//get data
int getPocketData(uint8_t *data,uint8_t *outBuff);


//server  send data to client 
int sendPocketData(int clientfd,uint8_t *data,uint64_t len,int errorFlag);

#endif
