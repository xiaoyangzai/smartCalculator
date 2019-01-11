#include "common.h"
#include "websocketprotocl.h"

//获取数据长度
int getPocketDataSize(uint8_t *data,uint32_t *size)
{
	if(data == NULL)
		return -1;
	uint8_t frist = (data[1]) & 0x7F;
	if(frist < 126)
		*size = frist;
	else if(frist == 126)
		*size = ntohs(*((uint16_t *)(data + 1)));	
	else if(frist == 127)
		*size = ntohll(*((uint16_t *)(data + 1)));
#ifdef __PROTOCL_DEBUG__
	printf("data size: %d\n",*size);
#endif
	return 0;
}

//获取掩码值，并且存储到数组中
int getMaskKey(uint8_t *data,uint8_t maskKey[])
{
	int i = 0;
	uint8_t *mask = data + 2;
	for(i = 0;i < 4;i++)
		maskKey[i] = mask[i];
	
#ifdef __PROTOCL_DEBUG__
	printf("Mask Key: ");
	for(i = 0;i < 4;i++)
		printf(" %2x ",maskKey[i]);
	puts("\n");
#endif
	return 0;
}

//解掩码或掩码
//转换后的结果存储在data中
int transformData(uint8_t *data,uint32_t len,uint8_t maskKey[],uint8_t *outBuff)
{
	uint32_t i = 0,
		j = 0;
	data = data + 6;

	for(i = 0;i < len;i++)
	{
		j = i % 4;
		outBuff[i] = data[i] ^ maskKey[j];
	}
#ifdef __PROTOCL_DEBUG__
	printf("transate %d bytes data finish!!\n",i);
#endif
	return len;
}

int getPocketData(uint8_t *data,uint8_t *outBuff)
{
	//if mask??
	if(!IS_SETMASK(data[1]))
		return -1;
	if(OPCODE(data[0]) == CLOSE_CODE)
		return -1;
	uint32_t size = 0;
	getPocketDataSize(data,&size);
	uint8_t maskKey[4] = {0};
	getMaskKey(data,maskKey);
	return transformData(data,size,maskKey,outBuff);
}

int sendWebSocketHeader(int clientfd,uint8_t type,uint64_t len)
{
#ifdef DEBUG
	printf("send websocket header......\n");
#endif
	uint8_t pockHeader[MAXLINE] = {0};	
	int index = 0;
	pockHeader[index++] = 0x80 | type;
	if(len < 126)
		pockHeader[index++] = (uint8_t)len;
	else if(len <= 0xFFFF)
	{
		pockHeader[index++] = 126;
		*((uint16_t *)(pockHeader + index)) = htons((uint16_t)len);	
		index += 2;
	}
	else
	{
		pockHeader[index++] = 127;
		*((uint64_t *)(pockHeader + index)) = htonll(len);	
		index += 8;
	}
	if(write(clientfd,pockHeader,index) < 0)
		ERR("send pocket failed");
#ifdef DEBUG
	printf("send websocket header finish!!!\n");
#endif
	return 0;
	
}
//server send data pointed by data to client 
int sendPocketData(int clientfd,uint8_t *data,uint64_t len,int errorFlag)
{
	uint8_t pocket[MAXLINE] = {0};
	int index = 0;
	if(errorFlag)
		pocket[index++] = 0x80 | CLOSE_CODE;	//FIN : 1,OPCODE : 0x8,CLOSE STATUES
	else
		pocket[index++] = 0x80 | TEXT_CODE;	//FIN : 1,OPCODE : 0x8,CLOSE STATUES


	if(len < 126)
		pocket[index++] = (uint8_t)len;
	else if(len <= 0xFFFF)
	{
		pocket[index++] = 126;
		pocket[index++] = htons((uint16_t)len);
	}
	else
	{
		pocket[index++] = 127;
		pocket[index++] = htonll(len);
	}
	memcpy(pocket + index,data,len);
	index = index + len;
	if(write(clientfd,pocket,index) < 0)
		ERR("send pocket failed");
#ifdef __PROTOCL_DEBUG__
	printf("====== %d Bytes has sent to client =======\n",index);
	int i = 0;
	for(i = 0;i < index;i++)
		printf("%hhx ",pocket[i]);
	puts("\n");
#endif
	return index;
}
