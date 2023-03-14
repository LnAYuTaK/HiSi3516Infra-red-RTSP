#ifndef _CIRCLE_BUF_H_
#define _CIRCLE_BUF_H_


typedef unsigned long HandlerCirBuf ;
//typedef void* HandlerCirBuf ;

HandlerCirBuf CircleBufCreate(unsigned int nLen);//申请生成一个循环缓冲区,返回生成的缓冲区句柄
int CircleBufDestroy(HandlerCirBuf* handler);//销毁循环缓冲区
int CircleBufReset(HandlerCirBuf* handler);//复位循环缓冲区
int CircleBufWriteData(HandlerCirBuf* handler, char* data_addr, unsigned int length);//写数据
int CircleBufReadData(HandlerCirBuf* handler, char* data_addr, unsigned int length);//读数据
int CircleBufResumeToPast(HandlerCirBuf* handler, int nWr, int nRd);//恢复循环缓冲区至读/写之前的状态(nWr/nRd:0无效,非0有效; 由于循环,所以不保证能恢复成功)
int CircleBufSkip(HandlerCirBuf* handler, int nWr, unsigned int nWrSkipLen, int nRd, unsigned int nRdSkipLen);//跳动读/写的位置(nWr/nRd:0无效,非0有效; 用在抛弃数据时)
int CircleBufGetLen(HandlerCirBuf* handler, unsigned int* total, unsigned int* used);//获得循环缓冲区的总长度/已用长度


#endif //_CIRCLE_BUF_H_
