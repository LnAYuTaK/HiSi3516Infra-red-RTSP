#ifndef _CIRCLE_BUF_H_
#define _CIRCLE_BUF_H_


typedef unsigned long HandlerCirBuf ;
//typedef void* HandlerCirBuf ;

HandlerCirBuf CircleBufCreate(unsigned int nLen);//��������һ��ѭ��������,�������ɵĻ��������
int CircleBufDestroy(HandlerCirBuf* handler);//����ѭ��������
int CircleBufReset(HandlerCirBuf* handler);//��λѭ��������
int CircleBufWriteData(HandlerCirBuf* handler, char* data_addr, unsigned int length);//д����
int CircleBufReadData(HandlerCirBuf* handler, char* data_addr, unsigned int length);//������
int CircleBufResumeToPast(HandlerCirBuf* handler, int nWr, int nRd);//�ָ�ѭ������������/д֮ǰ��״̬(nWr/nRd:0��Ч,��0��Ч; ����ѭ��,���Բ���֤�ָܻ��ɹ�)
int CircleBufSkip(HandlerCirBuf* handler, int nWr, unsigned int nWrSkipLen, int nRd, unsigned int nRdSkipLen);//������/д��λ��(nWr/nRd:0��Ч,��0��Ч; ������������ʱ)
int CircleBufGetLen(HandlerCirBuf* handler, unsigned int* total, unsigned int* used);//���ѭ�����������ܳ���/���ó���


#endif //_CIRCLE_BUF_H_
