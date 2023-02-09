/*
   Filename:     circlebuf.c
   Version:      1.0
   Author:       lituer Yu
   Started:      2012.8.14
   Last Updated: 2012.8.14
   Updated by:   lituer Yu
   Target O/S:   Linux (2.6.x)

   Description: Alloc one ring buffer, so you can write data to it and read data from it.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include "circlebuf.h"

typedef struct {
	//int nBufId;				//每个buf分配一个id
	char* nBufAddr;			//buf的首地址
	unsigned int nWrOffset;		//记录buf的写偏移量
	unsigned int nRdOffset;		//记录buf的读偏移量
	unsigned int nWrOffsetPast;//记录buf的上一次的写偏移量(恢复时用)
	unsigned int nRdOffsetPast;//记录buf的上一次的读偏移量
	unsigned int nLength;			//buf的总长度
	unsigned int nLenUsed;			//buf的已用长度
	sem_t sem;				//同步读写操作(半双工方式)
}SCircleBufInfo;


HandlerCirBuf CircleBufCreate(unsigned int nLen)//申请生成一个循环缓冲区,返回生成的缓冲区句柄
{
	if(nLen <= 0)
	{
		fprintf(stderr,"Invalid parameter for function %s !\n", __FUNCTION__);
		return (HandlerCirBuf)NULL;
	}
	
	SCircleBufInfo* info = NULL;
	info = (SCircleBufInfo*)calloc(sizeof(SCircleBufInfo), 1);
	if(NULL == info)
	{
		fprintf(stderr,"calloc error! function %s !\n", __FUNCTION__);
		return (HandlerCirBuf)NULL;
	}	
	info->nLength = nLen;
	
	char *head = NULL;
	head = (char *)calloc(info->nLength, 1);
	if(NULL == head)
	{
		fprintf(stderr,"Invalid pointer returned , malloc failed !\n");
		free(info);
		return (HandlerCirBuf)NULL;
	}
	
	info->nBufAddr = head;
	info->nWrOffset = 0;
	info->nRdOffset = 0;
	info->nWrOffsetPast = 0;
	info->nRdOffsetPast = 0;
	info->nLenUsed = 0;

	if(0 != sem_init(&info->sem, 0, 1))
	{
		fprintf(stderr,"sem_init failed ! function: %s\n", __FUNCTION__);
		free(head);
		free(info);
		return (HandlerCirBuf)NULL;
	}	
	
	return (HandlerCirBuf)info;
}

int CircleBufDestroy(HandlerCirBuf* handler)//销毁循环缓冲区
{
	SCircleBufInfo* info = (SCircleBufInfo*)*handler;
	
	if(NULL == info)
	{
		fprintf(stderr,"Invalid parameter for function %s !\n", __FUNCTION__);
		return -1;
	}

	sem_wait(&info->sem);

	if(info->nBufAddr)
	{
		free(info->nBufAddr);
	}
	//info->nBufAddr = NULL;
	
	sem_destroy(&info->sem);
	
	free(info);
	
	*handler = (HandlerCirBuf)NULL;
	
	return 0;
}

int CircleBufReset(HandlerCirBuf* handler)//复位循环缓冲区
{
	SCircleBufInfo* info = (SCircleBufInfo*)*handler;
	
	if(NULL == info)
	{
		fprintf(stderr,"Invalid parameter for function %s !\n", __FUNCTION__);
		return -1;
	}

	sem_wait(&info->sem);
	sem_destroy(&info->sem);
	info->nWrOffset = 0;
	info->nRdOffset = 0;
	info->nWrOffsetPast = 0;
	info->nRdOffsetPast = 0;
	info->nLenUsed = 0;
	if(0 != sem_init(&info->sem, 0, 1))
	{
		fprintf(stderr,"sem_init failed ! function: %s\n", __FUNCTION__);
		sem_post(&info->sem);
		return -2;
	}
	
	return 0;
}

int CircleBufWriteData(HandlerCirBuf* handler, char* data_addr, unsigned int length)//写数据
{
	SCircleBufInfo* info = (SCircleBufInfo*)*handler;
	
	if((NULL == info) || (NULL == data_addr) || (length <= 0))
	{
		fprintf(stderr,"%p,%p,%u\n", info, data_addr, length);
		fprintf(stderr,"Invalid parameter for function %s !\n", __FUNCTION__);
		return -1;
	}

	sem_wait(&info->sem);
	
	if(info->nLenUsed + length > info->nLength)
	{
		fprintf(stderr, "function: %s, The data you wanna write is too long, will not write buffer ! wanna[%u],used[%u],nLen[%u]\n",__FUNCTION__,length,info->nLenUsed, info->nLength);
		sem_post(&info->sem);
		return -2;
	}
	
	info->nWrOffsetPast = info->nWrOffset;
	
	if(info->nWrOffset + length >= info->nLength)
	{
		char* des_addr = info->nBufAddr + info->nWrOffset;
		unsigned int len = info->nLength - info->nWrOffset;
		memcpy(des_addr, data_addr, len);
		des_addr = info->nBufAddr;
		data_addr += len;
		len = length - len;
		memcpy(des_addr, data_addr, len);
		info->nWrOffset = (info->nWrOffset + length - info->nLength);
	}
	else
	{
		char* des_addr = info->nBufAddr + info->nWrOffset;
		memcpy(des_addr, data_addr, length);
		info->nWrOffset += length;
	}
	info->nLenUsed += length;
	
	sem_post(&info->sem);
	
	return 0;
}

int CircleBufReadData(HandlerCirBuf* handler, char* data_addr, unsigned int length)//读数据
{
	SCircleBufInfo* info = (SCircleBufInfo*)*handler;
	
	if((NULL == info) || (NULL == data_addr) || (length <= 0))
	{
		fprintf(stderr,"Invalid parameter for function %s !\n", __FUNCTION__);
		return -1;
	}
	
	//fprintf(stderr, "function: %s,  all[%u], wanna[%u]\n",__FUNCTION__,info->nLenUsed,length);

	sem_wait(&info->sem);
	
	if(info->nLenUsed < length)
	{
		fprintf(stderr, "function: %s, The data you wanna read is too long, will not read buffer ! all[%u], wanna[%u]\n",__FUNCTION__,info->nLenUsed,length);

		info->nLenUsed = 0; //是否得当? 值得考虑
		info->nWrOffset = 0;
		info->nRdOffset = 0;
		info->nWrOffsetPast = 0;
		info->nRdOffsetPast = 0;
	
		sem_post(&info->sem);
		return -2;
	}

	info->nRdOffsetPast = info->nRdOffset;
	
	if(info->nRdOffset + length >= info->nLength)
	{
		char* src_addr = info->nBufAddr + info->nRdOffset;
		unsigned int len = info->nLength - info->nRdOffset;
		memcpy(data_addr, src_addr, len);
		src_addr = info->nBufAddr;
		data_addr += len;
		len = length - len;
		memcpy(data_addr, src_addr, len);
		info->nRdOffset = (info->nRdOffset + length - info->nLength);
	}
	else
	{
		char* src_addr = info->nBufAddr + info->nRdOffset;
		memcpy(data_addr, src_addr, length);
		info->nRdOffset += length;
	}
	info->nLenUsed -= length;
	
	sem_post(&info->sem);
	
	return 0;
}

int CircleBufResumeToPast(HandlerCirBuf* handler, int nWr, int nRd)//恢复循环缓冲区至读/写之前的状态(nWr/nRd:0无效,非0有效; 由于循环,所以不保证能恢复成功)
{
	SCircleBufInfo* info = (SCircleBufInfo*)*handler;
	
	if(NULL == info)
	{
		fprintf(stderr,"Invalid parameter for function %s !\n", __FUNCTION__);
		return -1;
	}

	sem_wait(&info->sem);
	
	if(nWr)
	{
		unsigned int len = (info->nWrOffset < info->nWrOffsetPast) ? (info->nWrOffset + info->nLength - info->nWrOffsetPast) : (info->nWrOffset - info->nWrOffsetPast);
		if(info->nLenUsed < len)
		{
			printf("Warnning: [W] sth may be wrong! function: %s\n", __FUNCTION__);
			info->nWrOffset = (info->nWrOffset < info->nLenUsed) ? (info->nWrOffset + info->nLength - info->nLenUsed) : (info->nWrOffset - info->nLenUsed);
			info->nWrOffsetPast = info->nWrOffset;
			info->nLenUsed = 0;
		}
		else
		{
			info->nLenUsed -= len;
			info->nWrOffset = info->nWrOffsetPast;	
		}	
	}
	if(nRd)
	{
		unsigned int len = (info->nRdOffset < info->nRdOffsetPast) ? (info->nRdOffset + info->nLength - info->nRdOffsetPast) : (info->nRdOffset - info->nRdOffsetPast);
		if(info->nLenUsed + len > info->nLength)
		{
			printf("Warnning: [R] sth may be wrong! function: %s\n", __FUNCTION__);
			info->nLenUsed = info->nLength;
		}
		else
		{
			info->nLenUsed += len;
		}
		info->nRdOffset = info->nRdOffsetPast;
	}

	sem_post(&info->sem);

	return 0;
}

int CircleBufSkip(HandlerCirBuf* handler, int nWr, unsigned int nWrSkipLen, int nRd, unsigned int nRdSkipLen)//跳动读/写的位置(nWr/nRd:0无效,非0有效; 用在抛弃数据时)
{
	SCircleBufInfo* info = (SCircleBufInfo*)*handler;
	
	sem_wait(&info->sem);
	
	if((NULL == info) || (nWrSkipLen > info->nLength) || (nRdSkipLen > info->nLength))
	{
		fprintf(stderr,"Invalid parameter for function %s !\n", __FUNCTION__);
		sem_post(&info->sem);
		return -1;
	}

	if(((nWr) && (info->nLenUsed + nWrSkipLen > info->nLength))
			|| ((nRd) && (info->nLenUsed < nRdSkipLen)))
	{
		fprintf(stderr, "function: %s, The len skip len is too long! wanna[w/r:%u/%u]\n",__FUNCTION__,nWrSkipLen,nRdSkipLen);
		sem_post(&info->sem);
		return -2;
	}
	
	if(nWr)
	{
		info->nWrOffsetPast = info->nWrOffset;
		info->nLenUsed += nWrSkipLen;
 		if(info->nLength < info->nWrOffset + nWrSkipLen)
		{
			info->nWrOffset = (info->nWrOffset + nWrSkipLen - info->nLength);
		}
		else
		{
			info->nWrOffset += nWrSkipLen;	
		}
	}
	if(nRd)
	{
		info->nRdOffsetPast = info->nRdOffset;
		info->nLenUsed -= nRdSkipLen;
 		if(info->nLength < info->nRdOffset + nRdSkipLen)
		{
			info->nRdOffset = (info->nRdOffset + nRdSkipLen - info->nLength);
		}
		else
		{
			info->nRdOffset += nRdSkipLen;	
		}
	}

	sem_post(&info->sem);

	return 0;
}

int CircleBufGetLen(HandlerCirBuf* handler, unsigned int* total, unsigned int* used)//获得循环缓冲区的总长度/已用长度
{
	SCircleBufInfo* info = (SCircleBufInfo*)*handler;
	
	if((NULL == info) || (NULL == total) || (NULL == used))
	{
		fprintf(stderr,"Invalid parameter for function %s !\n", __FUNCTION__);
		return -1;
	}
	
	sem_wait(&info->sem);
	
	*total = info->nLength;
	*used = info->nLenUsed;
	
	sem_post(&info->sem);

	return 0;
}

