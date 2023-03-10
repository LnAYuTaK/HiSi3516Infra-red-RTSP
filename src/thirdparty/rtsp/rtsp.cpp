/*
Description: A simple RTSP server, VLC player can be used as a client to test this server.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <errno.h>
#include "rtsp.h"
#define RTP_USE_CIRCLE_BUF


#ifdef RTP_USE_CIRCLE_BUF
#include "circlebuf.h"
#endif

#define RTSP_STREAM_MAX_NUM 16

#define RTSP_MAX_LINKS (RTSP_STREAM_MAX_NUM<<1)

#define FRAME_SIZE_MAX (2<<20)

typedef struct
{
	SRtspInitAttr sInitAttr;
	int nRtspSockFd;
	unsigned short nRtspPort;
	unsigned short nStreamFlag;
	int nRtpSockFd;
	int nRtcpSockFd;
	unsigned int nSessionId;
	unsigned int nSSRC;
	struct
	{
		int nSockFd;
		char pDestIp[16];
		unsigned short nDestPort;
		unsigned short nStreamFlag;
		unsigned short nCmdStatus;//0:None 1:Get OPTIONS; 2:get Description; 3:get Setup; 4:get Play; 5:TearDown
		struct
		{
			unsigned int nUseTcp;
			unsigned short nRtpPort;
			unsigned short nRtcpPort;
			unsigned int nSession;
			unsigned int nTimeStamp;
			unsigned int nBasePts;
			unsigned int nSSRC;
			unsigned short nSeq;
			unsigned int nCurFrameLen;
		}sStreamRtpFlag[RTSP_STREAM_MAX_NUM];
	}sClientInfo[RTSP_MAX_LINKS];
	pthread_t nRtspMgrPid;
	pthread_t nRtspStreamPid;
	int nRunning;
	char pCmdBuf[1500];
	char pRtcpBuf[1500];
	char pRtpBuf[1500];
	char pRtcpBuf_Tcp[1504];
	char pRtpBuf_Tcp[1504];
	#ifdef RTP_USE_CIRCLE_BUF
	HandlerCirBuf pVencCircleBuf;
	sem_t nCircleBufSem;
	char* pFrameBuf;
	#endif
}SRtspManager;


#define RTSP_PORT_RTP    6970
#define RTSP_PORT_RTCP   (RTSP_PORT_RTP+1)

#define RTSP_METHOD_OPTIONS       "OPTIONS"
#define RTSP_METHOD_DESCRIBE      "DESCRIBE"
#define RTSP_METHOD_SETUP         "SETUP"
#define RTSP_METHOD_PLAY          "PLAY"
#define RTSP_METHOD_TEARDOWN      "TEARDOWN"
#define RTSP_METHOD_ANNOUNCE      "ANNOUNCE"
#define RTSP_METHOD_GET_PARAMETER "GET_PARAMETER"
#define RTSP_METHOD_PAUSE         "PAUSE"
#define RTSP_METHOD_RECORD        "RECORD"
#define RTSP_METHOD_REDIRECT      "REDIRECT"
#define RTSP_METHOD_SET_PARAMETER "SET_PARAMETER"

#define RTSP_RESULT_OK   "200 OK"
#define RTSP_RESULT_FAIL "501 Not Implemented"

typedef struct
{
	char pMethod[16];
	char pURL[128];
	char pVersion[16];
	char pResult[32];
	char pCSeq[16];
	char pDate[36];
	union
	{
		struct
		{
			char pPublic[256];
		}sOptions;
		struct
		{
			char pAccept[128];
			char pContentBase[128];
			char pContentType[32];
			char pContentLength[32];
			char pDescription[512];//sdp
		}sDescribe;
		struct
		{
			char pTransport[256];
			char pClientPort[32];
			char pServerPort[32];
			char pDestination[32];
			char pSource[32];
			char pSession[32];
		}sSetup;
		struct
		{
			char pSession[32];
			char pRange[32];	
			char pRtpInfo[256];
		}sPlay;
		struct
		{
			char pSession[32];
		}sTearDown;
	}uAttr;
}SRtspCmd;

typedef enum
{
	EM_RTCP_TYPE_SR   = 0b00001,
	EM_RTCP_TYPE_RR   = 0b00010,
	EM_RTCP_TYPE_SDES = 0b00100,
	EM_RTCP_TYPE_BYE  = 0b01000,
}EM_RTCP_TYPE;
#define RTCP_TYPE_SR 	 200
#define RTCP_TYPE_RR 	 201
#define RTCP_TYPE_SDES 202
#define RTCP_TYPE_BYE  203
typedef struct
{
	unsigned char nRecptReportCnt:5;
	unsigned char nPadding:1;
	unsigned char nVersion:2;
	unsigned char nPacketType;
	unsigned short nLength;
	unsigned int  nSSRC;
	unsigned int  nNTPMSW;
	unsigned int  nNTPLSW;
	unsigned int  nRTPTimeStamp;
	unsigned int  nSendPackCnt;
	unsigned int  nSendByteCnt;
	//unsigned char nRRBlock_Reserved[256];
}SRtcpSenderReport;
typedef struct
{
	unsigned char nSourceCnt:5;
	unsigned char nPadding:1;
	unsigned char nVersion:2;
	unsigned char nPacketType;
	unsigned short nLength;
	unsigned int  nSSRC;
	struct
	{
		unsigned char nType;
		unsigned char nLength;
		char          pText[255];
		unsigned char nEnd;
	}sItems[32];
}SRtcpSourceDescription;

#define RTP_PAYLOAD_LEN_MAX 1442//1500-20-8-12-2-16  IP:20 udp:8 rtp:12 nal:2 reserved:16
/****************************************************************** 
18.RTP_FIXED_HEADER 
19.0                   1                   2                   3 
20.0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 
21.+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ 
22.|V=2|P|X|  CC   |M|     PT      |       sequence number         | 
23.+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ 
24.|                           timestamp                           | 
25.+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ 
26.|           synchronization source (SSRC) identifier            | 
27.+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+ 
28.|            contributing source (CSRC) identifiers             | 
29.|                             ....                              | 
30.+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ 
31. 
32.******************************************************************/  
typedef struct   
{  
    /* byte 0 */  
    unsigned char csrc_len:4; /* CC expect 0 */  
    unsigned char extension:1;/* X  expect 1, see RTP_OP below */  
    unsigned char padding:1;  /* P  expect 0 */  
    unsigned char version:2;  /* V  expect 2 */  
    /* byte 1 */  
    unsigned char payload:7; /* PT  RTP_PAYLOAD_RTSP */  
    unsigned char marker:1;  /* M   expect 1 */  
        /* byte 2,3 */  
   unsigned short seq_no;   /*sequence number*/
    /* byte 4-7 */  

    unsigned  int  timestamp;  
    /* byte 8-11 */  
    unsigned int  ssrc; /* stream number is used here. */  
} RTP_HEADER;/*12 bytes*/  
  
/****************************************************************** 
52.NALU_HEADER 
53.+---------------+ 
54.|0|1|2|3|4|5|6|7| 
55.+-+-+-+-+-+-+-+-+ 
56.|F|NRI|  Type   | 
57.+---------------+ 
58.******************************************************************/  
typedef struct {  
    //byte 0  
    unsigned char TYPE:5;  
    unsigned char NRI:2;  
    unsigned char F:1;  
} NALU_HEADER; /* 1 byte */  
  
  
/****************************************************************** 
FU_INDICATOR 
+---------------+ 
|0|1|2|3|4|5|6|7| 
+-+-+-+-+-+-+-+-+ 
|F|NRI|  Type   | 
+---------------+ 
******************************************************************/  
typedef struct {  
    //byte 0  
    unsigned char TYPE:5;  
    unsigned char NRI:2;   
    unsigned char F:1;           
} FU_INDICATOR; /*1 byte */  
  
  
/****************************************************************** 
FU_HEADER 
+---------------+ 
|0|1|2|3|4|5|6|7| 
+-+-+-+-+-+-+-+-+ 
|S|E|R|  Type   | 
+---------------+ 
******************************************************************/  
typedef struct {  
    //byte 0  
    unsigned char TYPE:5;  
    unsigned char R:1;  
    unsigned char E:1;  
    unsigned char S:1;      
} FU_HEADER; /* 1 byte */  

#if 0
static unsigned int LoopSend(int nSockFd, char* pBuf, unsigned int nLen)
{
	unsigned int nRemain = nLen;
	unsigned int nSent = 0;
	int ret = 0;
	while(nRemain)
	{
		ret= write(nSockFd, pBuf+nSent, nRemain);
		if(ret < 0)
		{
			break;
		}
		nSent += ret;
		nRemain -= ret;
	}
	return nSent;
}

#else 
static unsigned int LoopSend(int nSockFd, char* pBuf, unsigned int nLen)
{
	unsigned int nRemain = nLen;
	unsigned int nSent = 0;
	int ret = 0;
	int cnt = 0, err_cnt = 0;
	while(nRemain)
	{
        //if(nSockFd < 0)
        //    return nLen; /**/
		ret= write(nSockFd, pBuf+nSent, nRemain);
		if(ret <= 0)
		{
            if(ret <0)
            {
                printf("error %d\n", errno);
                err_cnt++;
            }
            else
                cnt++;
			if((err_cnt+cnt)>=200)
			{
				printf("write error or zero more than %d %d\n",err_cnt,cnt);
				break;
			}
            printf("write error or zero more than %d %d\n",err_cnt,cnt);
			usleep(5000);
			continue;
		}
		cnt = err_cnt = 0;
		nSent += ret;
		nRemain -= ret;
	}
	return nSent;
}
#endif


static int UdpSendPacket(int sockfd, struct sockaddr_in* server, char* packet, unsigned int len)
{
	int ret;
	unsigned int udpsendbuflen = 64<<10;
	static sem_t sem;
	static int flag = 1;
	struct timespec ts;
	if(flag)
	{
		sem_init(&sem, 0, 0);
		flag = 0;
	}	
	static unsigned int sendlen = 0;
	if(sendlen + len > udpsendbuflen)
	{
		//usleep(2000);
		ts.tv_sec = 0;
		ts.tv_nsec = 500*1000;
		sem_timedwait(&sem, &ts);
		sendlen = 0;
	}
	sendlen += len;
	ret = sendto(sockfd,packet,len,0/*MSG_CONFIRM*/,
			(struct sockaddr*)server,sizeof(struct sockaddr_in));
	return ret==len?0:-1;
}

static int RefreshStreamStatus(SRtspManager *pMgr, int nClientIdx, int nStreamIdx, int nEnable)
{
	int nPast, nThis, i;

	if(NULL == pMgr || nStreamIdx < 0 || nStreamIdx >= pMgr->sInitAttr.nStreamTotal 
		 || nClientIdx < 0 || nClientIdx >= sizeof(pMgr->sClientInfo)/sizeof(pMgr->sClientInfo[0]))
		 return -1;
	
	nPast = (pMgr->nStreamFlag >> nStreamIdx) & 1;

	if(((pMgr->sClientInfo[nClientIdx].nStreamFlag >> nStreamIdx) & 1) && 0 == nEnable)
	{
		pMgr->sClientInfo[nClientIdx].nStreamFlag &= ~(1 << nStreamIdx);
	}
	else if(0 == ((pMgr->sClientInfo[nClientIdx].nStreamFlag >> nStreamIdx) & 1) && nEnable)
	{
		pMgr->sClientInfo[nClientIdx].nStreamFlag |= (1 << nStreamIdx);
	}
	else
	{
		printf("Should rarely be here! %s %s %d\n", __FILE__,__FUNCTION__,__LINE__);
		return 0;
	}
	
	nThis = 0;
	for(i = 0; i < sizeof(pMgr->sClientInfo)/sizeof(pMgr->sClientInfo[0]); i++)
	{
		if(/*pMgr->sClientInfo[i].nSockFd >= 0 && */((pMgr->sClientInfo[i].nStreamFlag>>nStreamIdx)&1))
		{
			nThis = 1;
			break;
		}
	}
	if(nThis == 0 && nPast)
	{
		pMgr->nStreamFlag &= ~(1 << nStreamIdx);
		pMgr->sInitAttr.pStopStream(nStreamIdx);
	}
	else if(nThis && nPast == 0)
	{
		pMgr->nStreamFlag |= (1 << nStreamIdx);
		pMgr->sInitAttr.pStartStream(nStreamIdx);        
        SRtspStreamPara StreamPara;
        pMgr->sInitAttr.pGetStreamPara(nStreamIdx,&StreamPara);
        printf("Rtsp frame %d %d\n",StreamPara.nFrameRate,(1000*1000)/(StreamPara.nFrameRate));
		for(i = 0; i < 2; i++)
		{
			usleep((2000*1000)/(StreamPara.nFrameRate));
			//usleep(500000);
			pMgr->sInitAttr.pApplyKeyFrame(nStreamIdx);
		}
	}
	else if(nThis && nPast)
	{
		if(nEnable)
		{
			//usleep(150000);
			pMgr->sInitAttr.pApplyKeyFrame(nStreamIdx);
			SRtspStreamPara StreamPara;
            pMgr->sInitAttr.pGetStreamPara(nStreamIdx,&StreamPara);
    	    for(i = 0; i < 1; i++)
    		{
    			//usleep(60000);
    			usleep((2000*1000)/(StreamPara.nFrameRate));
    			pMgr->sInitAttr.pApplyKeyFrame(nStreamIdx);
    		}
		}
	}

	return 0;
}



static int SendRtpPackToClients(SRtspManager *pMgr, SRtspFrameHead *pFrameHead, unsigned int nLen)
{
	RTP_HEADER *rtp_hdr;  
	int i;
    int ret = 0;

	rtp_hdr =(RTP_HEADER*)pMgr->pRtpBuf;
	
	for(i = 0; i < RTSP_MAX_LINKS; i++)
	{
		if(pMgr->sClientInfo[i].nStreamFlag & (1 << pFrameHead->nStreamIndex))
		{
			if(pMgr->sClientInfo[i].sStreamRtpFlag[pFrameHead->nStreamIndex].nCurFrameLen != pFrameHead->nRealLen 
				  || pMgr->sClientInfo[i].sStreamRtpFlag[pFrameHead->nStreamIndex].nTimeStamp != pFrameHead->nTimeStamp)
			{
				if(pMgr->sClientInfo[i].sStreamRtpFlag[pFrameHead->nStreamIndex].nTimeStamp == 0)//first frame
				{
					pMgr->sClientInfo[i].sStreamRtpFlag[pFrameHead->nStreamIndex].nBasePts = 0;
				}
				else
				{
					if(pMgr->sClientInfo[i].sStreamRtpFlag[pFrameHead->nStreamIndex].nTimeStamp <= pFrameHead->nTimeStamp
                        && pMgr->sClientInfo[i].sStreamRtpFlag[pFrameHead->nStreamIndex].nTimeStamp > pFrameHead->nTimeStamp - 1000)
					//if(pMgr->sClientInfo[i].sStreamRtpFlag[pFrameHead->nStreamIndex].nTimeStamp < pFrameHead->nTimeStamp)
					{
						pMgr->sClientInfo[i].sStreamRtpFlag[pFrameHead->nStreamIndex].nBasePts += //(90000/60);
							(pFrameHead->nTimeStamp - pMgr->sClientInfo[i].sStreamRtpFlag[pFrameHead->nStreamIndex].nTimeStamp) * 90;
					}
				}
                //printf("%lu,%lu\n", pFrameHead->nTimeStamp, pMgr->sClientInfo[i].sStreamRtpFlag[pFrameHead->nStreamIndex].nTimeStamp);
				pMgr->sClientInfo[i].sStreamRtpFlag[pFrameHead->nStreamIndex].nTimeStamp = pFrameHead->nTimeStamp;
				pMgr->sClientInfo[i].sStreamRtpFlag[pFrameHead->nStreamIndex].nCurFrameLen = pFrameHead->nRealLen;
				//printf("%lu,%lu,%lu\n", pFrameHead->nTimeStamp, pMgr->sClientInfo[i].sStreamRtpFlag[pFrameHead->nStreamIndex].nTimeStamp,pMgr->sClientInfo[i].sStreamRtpFlag[pFrameHead->nStreamIndex].nBasePts);
			}
			rtp_hdr->seq_no = htons(pMgr->sClientInfo[i].sStreamRtpFlag[pFrameHead->nStreamIndex].nSeq++);
			rtp_hdr->ssrc = htonl(pMgr->sClientInfo[i].sStreamRtpFlag[pFrameHead->nStreamIndex].nSSRC);
			/*calc the pts*/
			rtp_hdr->timestamp = htonl(pMgr->sClientInfo[i].sStreamRtpFlag[pFrameHead->nStreamIndex].nBasePts);
			/*send out*/
			if(pMgr->sClientInfo[i].sStreamRtpFlag[pFrameHead->nStreamIndex].nUseTcp)
			{
				pMgr->pRtpBuf_Tcp[0] = '$';
				pMgr->pRtpBuf_Tcp[1] = (pMgr->sClientInfo[i].sStreamRtpFlag[pFrameHead->nStreamIndex].nRtpPort&0xff);
				*(unsigned short*)(pMgr->pRtpBuf_Tcp+2) = htons(nLen);
				memcpy(pMgr->pRtpBuf_Tcp+4, pMgr->pRtpBuf, nLen);
				ret = LoopSend(pMgr->sClientInfo[i].nSockFd, pMgr->pRtpBuf_Tcp, nLen+4);
                if(ret < (nLen+4))
                {
                    int j =0;
					for(j = 0; j < pMgr->sInitAttr.nStreamTotal; j++)
					{
						if((pMgr->sClientInfo[i].nStreamFlag>>j)&1)
							RefreshStreamStatus(pMgr, i, j, 0);
					}
					close(pMgr->sClientInfo[i].nSockFd);
					pMgr->sClientInfo[i].nSockFd = -1;
                    //dbgprint(LOG_LEVEL_RTSP,"######Send Tcp Error!!!Disconnect client %s:%u! Due to failure when write!\n", pMgr->sClientInfo[i].pDestIp,pMgr->sClientInfo[i].nDestPort);
                }
			}
			else
			{
				struct sockaddr_in send_addr;
				memset(&send_addr, 0, sizeof(send_addr));
				send_addr.sin_family = AF_INET;
				send_addr.sin_port = htons(pMgr->sClientInfo[i].sStreamRtpFlag[pFrameHead->nStreamIndex].nRtpPort);
				send_addr.sin_addr.s_addr = inet_addr(pMgr->sClientInfo[i].pDestIp);
				ret = UdpSendPacket(pMgr->nRtpSockFd, &send_addr, pMgr->pRtpBuf, nLen);
				if(ret < 0)
				{
					printf("UdpSendPacket err!\n");
				}
				//printf("%s %d\n",__FUNCTION__,UdpSendPacket(pMgr->nRtpSockFd, &send_addr, pMgr->pRtpBuf, nLen));
			}
			//if((pMgr->pRtpBuf[12]&0x1f)==0x7) printf("SPS...\n");
		}
	}
	return 0;
}
static int RtpSend(SRtspManager *pMgr, SRtspFrameHead *pFrameHead, char* pFrameData)
{  
	RTP_HEADER *rtp_hdr;  
	FU_INDICATOR *fu_ind;  
	FU_HEADER *fu_hdr;
	unsigned int sendcnt, first_packet, leftcnt;
	
	memset(pMgr->pRtpBuf, 0, sizeof(RTP_HEADER));

	rtp_hdr =(RTP_HEADER*)pMgr->pRtpBuf;         
	rtp_hdr->version = 2;
	rtp_hdr->marker  = 0;
	rtp_hdr->payload = 96;

    //printf("%s %d\n",__FUNCTION__,pFrameHead->nRealLen);
	
	if(pFrameHead->nRealLen -1 <= RTP_PAYLOAD_LEN_MAX)
	{
		if((pFrameData[0] & 0x1f) == 5 || (pFrameData[0] & 0x1f) == 1)
			rtp_hdr->marker = 1;// 1: frame end
		memcpy(&pMgr->pRtpBuf[12], pFrameData, pFrameHead->nRealLen);
		/*check all clients and send */
		SendRtpPackToClients(pMgr, pFrameHead, pFrameHead->nRealLen + 12);
	}
	else
	{
		fu_ind = (FU_INDICATOR*)&pMgr->pRtpBuf[12];
		fu_ind->F = (pFrameData[0] & 0x80)>>7;// 1 bit
		fu_ind->NRI = (pFrameData[0] & 0x60)>>5;// 2 bit
		fu_ind->TYPE = 28;
		fu_hdr = (FU_HEADER*)&pMgr->pRtpBuf[13];  
		fu_hdr->TYPE = pFrameData[0] & 0x1f;
		fu_hdr->R = 0;
		leftcnt = pFrameHead->nRealLen - 1;
		pFrameData += 1;
		first_packet = 1;
		while(leftcnt != 0)
		{
			sendcnt = leftcnt > RTP_PAYLOAD_LEN_MAX ? RTP_PAYLOAD_LEN_MAX : leftcnt;
			if(first_packet)//the first packet
			{
				first_packet = 0;
				//
				fu_hdr->S = 1;
				fu_hdr->E = 0;
			}
			else if(leftcnt == sendcnt)//the last packet
			{
				fu_hdr->S = 0;
				fu_hdr->E = 1;
				rtp_hdr->marker = 1;
			}
			else
			{
				fu_hdr->S = 0;
				fu_hdr->E = 0;
			}
			memcpy(&pMgr->pRtpBuf[14], pFrameData, sendcnt);
			
			/*check all clients and send */
			SendRtpPackToClients(pMgr, pFrameHead, sendcnt + 14);
			
			pFrameData += sendcnt;
			leftcnt -= sendcnt;
		}
	} 
	return 0;
}  
static int RtspPutOneFrame(RtspHandle pRtspHandle, SRtspFrameHead *pFrameHead, char* pFrameData)
{
	#ifdef RTP_USE_CIRCLE_BUF
	unsigned int total, used;
	SRtspManager *pMgr = (SRtspManager*)pRtspHandle;
	sem_wait(&pMgr->nCircleBufSem);
	CircleBufGetLen(&pMgr->pVencCircleBuf, &total, &used);
	if(total-used < sizeof(SRtspFrameHead)+pFrameHead->nRealLen)
	{
		printf("rtsp ignore one frame! circlebuf full\n");
	}
	else
	{
		CircleBufWriteData(&pMgr->pVencCircleBuf, (char*)pFrameHead,sizeof(SRtspFrameHead));
		CircleBufWriteData(&pMgr->pVencCircleBuf, (char*)pFrameData,pFrameHead->nRealLen);
	}
	sem_post(&pMgr->nCircleBufSem);
	return 0;
	#else
	return RtpSend((SRtspManager*)pRtspHandle, pFrameHead, pFrameData);
	#endif
}

#ifdef RTP_USE_CIRCLE_BUF
static void* RtspStreamFxn(void* pAttr)
{
	int ret;
	unsigned int total, used;
	SRtspFrameHead sFrameHead;
	SRtspManager *pMgr = (SRtspManager*)pAttr;
	if(NULL == pMgr)
		return NULL;
	
	pMgr->pFrameBuf = (char*)calloc(1, FRAME_SIZE_MAX);
	if(pMgr->pFrameBuf == NULL)
		return NULL;
	pMgr->pVencCircleBuf = CircleBufCreate(10<<20);
	sem_init(&pMgr->nCircleBufSem, 0, 1);
	
	while(pMgr->nRunning)
	{
		CircleBufGetLen(&pMgr->pVencCircleBuf, &total, &used);
		if(used <= sizeof(SRtspFrameHead))
		{
			//printf("should rarely be here!\n");
			usleep(5000);
			continue;
		}
		ret = -1;
		//sem_wait(&pMgr->nCircleBufSem);
		if(0 == CircleBufReadData(&pMgr->pVencCircleBuf, (char*)&sFrameHead, sizeof(sFrameHead)))
		{
			if(sFrameHead.nRealLen <= FRAME_SIZE_MAX)
			{
				if(0 == CircleBufReadData(&pMgr->pVencCircleBuf, pMgr->pFrameBuf, sFrameHead.nRealLen))
					ret = 0;
			}
			else
			{
				printf("frame len(%d) is too big!Skip it!", sFrameHead.nRealLen);
				CircleBufSkip(&pMgr->pVencCircleBuf, 0,0,1,sFrameHead.nRealLen);
			}
		}
		//sem_post(&pMgr->nCircleBufSem);
		if(0 == ret)
		{
			if((used > (5<<20)) && ((pMgr->pFrameBuf[0]&0x1f)==1) && (pMgr->pFrameBuf[0]!=0x61))
			{
				printf("Rtsp discard a NonKeyFrame\n");
				continue;
			}
			RtpSend(pMgr, &sFrameHead, pMgr->pFrameBuf);
			//usleep(5000);
		}
	}
	if(pMgr->pFrameBuf)
		free(pMgr->pFrameBuf);
	return NULL;
}
#endif


static unsigned long long NTPtime64 (void)
{
	struct timespec ts;
    memset(&ts, 0, sizeof(ts));
	//struct timeval tv;
	//gettimeofday (&tv, NULL);
	//ts.tv_sec = tv.tv_sec;
	//ts.tv_nsec = tv.tv_usec * 1000;
	/* Convert nanoseconds to 32-bits fraction (232 picosecond units) */
	unsigned long long t = (unsigned long long)(ts.tv_nsec) << 32;
	t /= 1000000000;
	/* There is 70 years (incl. 17 leap ones) offset to the Unix Epoch.
	* No leap seconds during that period since they were not invented yet.
	*/
	t |= ((70LL * 365 + 17) * 24 * 60 * 60 + ts.tv_sec) << 32;
	return t;
}
static int RtcpSend(unsigned int nMask, SRtspManager* pMgr, int nClientIdx, int nStreamIdx)
{
	SRtcpSenderReport sSendReport;
	SRtcpSourceDescription sSDES;
	int offset = 0, t;
	unsigned long long ntp;

	if(NULL == pMgr || nStreamIdx < 0 || nStreamIdx >= pMgr->sInitAttr.nStreamTotal 
		 || nClientIdx < 0 || nClientIdx >= sizeof(pMgr->sClientInfo)/sizeof(pMgr->sClientInfo[0]))
		 return -1;
	
	memset(&sSendReport, 0, sizeof(sSendReport));
	memset(&sSDES, 0, sizeof(sSDES));

	if(nMask & EM_RTCP_TYPE_SR)
	{
		sSendReport.nVersion = 2;
		sSendReport.nPadding = 0;
		sSendReport.nRecptReportCnt= 0;
		sSendReport.nPacketType = RTCP_TYPE_SR;
		sSendReport.nLength = htons((sizeof(SRtcpSenderReport)-4)/4);//Count after the item of nLength, by 4bytes
		sSendReport.nSSRC = htonl(pMgr->sClientInfo[nClientIdx].sStreamRtpFlag[nStreamIdx].nSSRC);
		ntp = NTPtime64();
		sSendReport.nNTPMSW = htonl(ntp>>32);
		sSendReport.nNTPLSW = htonl(ntp&0xffffffff);
		sSendReport.nRTPTimeStamp = htonl(pMgr->sClientInfo[nClientIdx].sStreamRtpFlag[nStreamIdx].nBasePts/*nTimeStamp*/);
        //printf("%s %lu,%lu\n",__FUNCTION__,sSendReport.nRTPTimeStamp,pMgr->sClientInfo[nClientIdx].sStreamRtpFlag[nStreamIdx].nBasePts);
		t = (ntohs(sSendReport.nLength)*4)+4;
		memcpy(pMgr->pRtcpBuf+offset, &sSendReport, t);
		offset += t;
	}
	if(nMask & EM_RTCP_TYPE_SDES)
	{
		sSDES.nVersion = 2;
		sSDES.nPadding = 0;
		sSDES.nSourceCnt = 1;
		sSDES.nPacketType = RTCP_TYPE_SDES;
		sSDES.nSSRC = htonl(pMgr->sClientInfo[nClientIdx].sStreamRtpFlag[nStreamIdx].nSSRC);
		sSDES.sItems[0].nType = 1;//CNAME
		sSDES.sItems[0].nLength = 6;
		strcpy(sSDES.sItems[0].pText, "(none)");
		sSDES.nLength = htons((4+1+1+sSDES.sItems[0].nLength+1+3)/4);//by 4bytes
		//
		t = (ntohs(sSDES.nLength)*4)+4;
		memcpy(pMgr->pRtcpBuf+offset, &sSDES, t);
		offset += t;
	}

	/*send out*/
	if(pMgr->sClientInfo[nClientIdx].sStreamRtpFlag[nStreamIdx].nUseTcp)
	{
		pMgr->pRtcpBuf_Tcp[0] = '$';
		pMgr->pRtcpBuf_Tcp[1] = (pMgr->sClientInfo[nClientIdx].sStreamRtpFlag[nStreamIdx].nRtcpPort&0xff);
		*(unsigned short*)(pMgr->pRtcpBuf_Tcp+2) = htons(offset);
		memcpy(pMgr->pRtcpBuf_Tcp+4, pMgr->pRtcpBuf, offset);
		LoopSend(pMgr->sClientInfo[nClientIdx].nSockFd, pMgr->pRtcpBuf_Tcp, offset+4);
	}
	else
	{
		struct sockaddr_in send_addr;
		memset(&send_addr, 0, sizeof(send_addr));
		send_addr.sin_family = AF_INET;
		send_addr.sin_port = htons(pMgr->sClientInfo[nClientIdx].sStreamRtpFlag[nStreamIdx].nRtcpPort);
		send_addr.sin_addr.s_addr = inet_addr(pMgr->sClientInfo[nClientIdx].pDestIp);
		UdpSendPacket(pMgr->nRtcpSockFd, &send_addr, pMgr->pRtcpBuf, offset);
	}
	return 0;
}

static int ParseRtspCmd(char* pCmd, SRtspCmd* pRtspCmd)
{
	char *p = NULL, *q = NULL, *s = NULL;
	char t = 0x20;
	time_t now;
	struct tm tm;
	
	if(NULL==pCmd || NULL==pRtspCmd)
		return -1;
	memset(pRtspCmd, 0, sizeof(SRtspCmd));

	if((NULL == (p = strchr(pCmd, t)))
		  || (NULL == (q = strchr(p+1, t)))
		  || (NULL == (s = strstr(q, "\r\n"))))
		return -1;
	printf("\n##Get RTSP CMD:\n");
	strncpy(pRtspCmd->pMethod, pCmd, p-pCmd);
	strncpy(pRtspCmd->pURL, p+1, q-p-1);
	strncpy(pRtspCmd->pVersion, q+1, s-q-1);
	printf("%s %s %s\n", pRtspCmd->pMethod,pRtspCmd->pURL,pRtspCmd->pVersion);
	p = s+2;
	strcpy(pRtspCmd->pResult, RTSP_RESULT_OK);
	if(0 == strcasecmp(pRtspCmd->pMethod, RTSP_METHOD_OPTIONS))
	{
		/*do nothing, Only Get "CSeq"*/
		while(NULL != (s = strstr(p, "\r\n")) && p!=s)//p==s is the end "\r\n\r\n"
		{
			if(0 == strncasecmp(p, "CSeq:", 5))
			{
				strncpy(pRtspCmd->pCSeq, p, s-p);
				printf("%s\n", pRtspCmd->pCSeq);
			}
			p = s+2;
		}
	}
	else if(0 == strcasecmp(pRtspCmd->pMethod, RTSP_METHOD_DESCRIBE))
	{
		/*Get "Accept"*/
		while(NULL != (s = strstr(p, "\r\n")) && p!=s)//p==s is the end "\r\n\r\n"
		{
			if(0 == strncasecmp(p, "Accept:", 7))
			{
				strncpy(pRtspCmd->uAttr.sDescribe.pAccept, p, s-p);
				printf("%s\n", pRtspCmd->uAttr.sDescribe.pAccept);
				if((NULL != (q = strstr(p, "application/sdp"))) && (q < s))
				{
					//
				}
				else
				{
					strcpy(pRtspCmd->pResult, RTSP_RESULT_FAIL);
				}
			}
			else if(0 == strncasecmp(p, "CSeq:", 5))
			{
				strncpy(pRtspCmd->pCSeq, p, s-p);
				printf("%s\n", pRtspCmd->pCSeq);
			}
			p = s+2;
		}
	}
	else if(0 == strcasecmp(pRtspCmd->pMethod, RTSP_METHOD_SETUP))
	{
		/*Get "Transport"*/
		while(NULL != (s = strstr(p, "\r\n")) && p!=s)//p==s is the end "\r\n\r\n"
		{
			if(0 == strncasecmp(p, "Transport:", 10))
			{
				strncpy(pRtspCmd->uAttr.sSetup.pTransport, p, s-p);
				printf("%s\n", pRtspCmd->uAttr.sSetup.pTransport);
				if((NULL != (q = strstr(p, "RTP/AVP"))) && (q < s)
					 && (NULL != (q = strstr(p, "client_port="))) && (q < s))
				{
					s = strchr(q, ';');
					if(NULL==s)
						s = strchr(q, 0x0d);//'\r'
					if(NULL==s)
						s = strchr(q, ' ');
					if(NULL==s)
						strcpy(pRtspCmd->pResult, RTSP_RESULT_FAIL);
					else
						strncpy(pRtspCmd->uAttr.sSetup.pClientPort, q, s-q);
				}
				else if((NULL != (q = strstr(p, "RTP/AVP/TCP"))) && (q < s)
					 && (NULL != (q = strstr(p, "interleaved="))) && (q < s))
				{
					s = strchr(q, ';');
					if(NULL==s)
						s = strchr(q, 0x0d);//'\r'
					if(NULL==s)
						s = strchr(q, ' ');
					if(NULL==s)
						strcpy(pRtspCmd->pResult, RTSP_RESULT_FAIL);
					else
						strncpy(pRtspCmd->uAttr.sSetup.pClientPort, q, s-q);
				}
				else
				{
					strcpy(pRtspCmd->pResult, RTSP_RESULT_FAIL);
				}
			}
			else if(0 == strncasecmp(p, "CSeq:", 5))
			{
				strncpy(pRtspCmd->pCSeq, p, s-p);
				printf("%s\n", pRtspCmd->pCSeq);
			}
			p = s+2;
		}
	}
	else if(0 == strcasecmp(pRtspCmd->pMethod, RTSP_METHOD_PLAY))
	{
		/*Get "Session" & "Range"*/
		while(NULL != (s = strstr(p, "\r\n")) && p!=s)//p==s is the end "\r\n\r\n"
		{
			if(0 == strncasecmp(p, "Session:", 8))
			{
				strncpy(pRtspCmd->uAttr.sPlay.pSession, p, s-p);
				printf("%s\n", pRtspCmd->uAttr.sPlay.pSession);
			}
			else if(0 == strncasecmp(p, "Range:", 6))
			{
				strncpy(pRtspCmd->uAttr.sPlay.pRange, p, s-p);
				printf("%s\n", pRtspCmd->uAttr.sPlay.pRange);
			}
			else if(0 == strncasecmp(p, "CSeq:", 5))
			{
				strncpy(pRtspCmd->pCSeq, p, s-p);
				printf("%s\n", pRtspCmd->pCSeq);
			}
			p = s+2;
		}
	}
	else if(0 == strcasecmp(pRtspCmd->pMethod, RTSP_METHOD_TEARDOWN))
	{
		/*Get "Session"*/
		while(NULL != (s = strstr(p, "\r\n")) && p!=s)//p==s is the end "\r\n\r\n"
		{
			if(0 == strncasecmp(p, "Session:", 8))
			{
				strncpy(pRtspCmd->uAttr.sTearDown.pSession, p, s-p);
				printf("%s\n", pRtspCmd->uAttr.sTearDown.pSession);
			}
			else if(0 == strncasecmp(p, "CSeq:", 5))
			{
				strncpy(pRtspCmd->pCSeq, p, s-p);
				printf("%s\n", pRtspCmd->pCSeq);
			}
			p = s+2;
		}
	}
	else
	{
		printf("Unsupport This CMD!\n");
		strcpy(pRtspCmd->pResult, RTSP_RESULT_FAIL);
	}
	/*Set Date&Time*/
	time(&now);
	memcpy(&tm, localtime(&now), sizeof(tm));
	snprintf(pRtspCmd->pDate, sizeof(pRtspCmd->pDate), "Date: %s", asctime(&tm));
	strcpy(&pRtspCmd->pDate[strlen(pRtspCmd->pDate)-1], " GMT");
	return 0;
}
static int DealRtspCmd(SRtspManager* pMgr, int nClientSockFd, SRtspCmd* pRtspCmd)
{
	unsigned int nRemain, nSent = 0, nSessionID=0;
	int ret, cnt = 0, i, clientIdx;
	char *p = "0.0.0.0", *q, *s;
	char src[64];
	char path[16];
	int chn = -1, streamIdx, refreshFlag = 0;
	unsigned short nClientRtpPort, nClientRtcpPort;

	if(NULL==pMgr || NULL==pRtspCmd || nClientSockFd<0)
		return -1;
	memset(pMgr->pCmdBuf, 0, sizeof(pMgr->pCmdBuf));
	
	/*Get SrcIP and Video path client applied*/
	memset(src, 0, sizeof(src));
	memset(path, 0, sizeof(path));
	if((NULL != (q = strstr(pRtspCmd->pURL, "//"))) 
		  && (NULL != (s = strchr(q+2, '/'))))
	{
		strncpy(src, q+2, s-q-2);
		if(NULL != (q = strcasestr(s, "live")))
		{
			strncpy(path, q, sizeof(path)-1);
			sscanf(path+4, "%d", &chn);
		}
		else
			src[0] = 0;
	}	
	if(chn < 0 || chn >= pMgr->sInitAttr.nStreamTotal)
	{
		strcpy(pRtspCmd->pResult, RTSP_RESULT_FAIL);
		printf("Parse URL failed! %s\n", pRtspCmd->pURL);
		/*Do not ack this wrong cmd*/
		return -1;
	}

	if(0 == strcmp(pRtspCmd->pResult, RTSP_RESULT_OK))
	{
		/*Pack the respond packet*/
		if(0 == strcasecmp(pRtspCmd->pMethod, RTSP_METHOD_OPTIONS))
		{
			/*Set "Public"*/
			sprintf(pRtspCmd->uAttr.sOptions.pPublic, "Public: %s %s %s %s %s", 
							RTSP_METHOD_OPTIONS,RTSP_METHOD_DESCRIBE,RTSP_METHOD_SETUP,RTSP_METHOD_PLAY,RTSP_METHOD_TEARDOWN);
			/*Set Respond*/
			sprintf(pMgr->pCmdBuf, "%s %s\r\n%s\r\n%s\r\n%s\r\n\r\n", 
							pRtspCmd->pVersion,pRtspCmd->pResult,pRtspCmd->pCSeq,pRtspCmd->pDate,pRtspCmd->uAttr.sOptions.pPublic);
		}
		else if(0 == strcasecmp(pRtspCmd->pMethod, RTSP_METHOD_DESCRIBE))
		{
			/*Set "SDP"*/
			sprintf(pRtspCmd->uAttr.sDescribe.pDescription, 
							"%s\r\n%s\r\n%s\r\n%s\r\n%s\r\n%s\r\n%s\r\n",
							"v=0",
							"o=- 1 1 IN IP4 0.0.0.0",
							"s=H.264 Video, streamed by DAYU",
							"t=0 0",
							"m=video 0 RTP/AVP 96",
							"a=rtpmap:96 H264/90000",
							"a=control:track1");
			/*Set "Content"*/
			sprintf(pRtspCmd->uAttr.sDescribe.pContentBase, "Content-Base: %s/", pRtspCmd->pURL);
			sprintf(pRtspCmd->uAttr.sDescribe.pContentType, "Content-type: application/sdp");
			sprintf(pRtspCmd->uAttr.sDescribe.pContentLength, "Content-Length: %d", strlen(pRtspCmd->uAttr.sDescribe.pDescription));
			/*Set Respond*/
			sprintf(pMgr->pCmdBuf, "%s %s\r\n%s\r\n%s\r\n%s\r\n%s\r\n%s\r\n\r\n%s", 
							pRtspCmd->pVersion,pRtspCmd->pResult,pRtspCmd->pCSeq,pRtspCmd->pDate,pRtspCmd->uAttr.sDescribe.pContentBase,pRtspCmd->uAttr.sDescribe.pContentType,pRtspCmd->uAttr.sDescribe.pContentLength,pRtspCmd->uAttr.sDescribe.pDescription);
		}
		else if(0 == strcasecmp(pRtspCmd->pMethod, RTSP_METHOD_SETUP))
		{
			/*Save the client port and sessionID*/
			if((NULL != (q = strchr(pRtspCmd->uAttr.sSetup.pClientPort, '='))) 
				  /*&& (NULL != (s = strchr(q+1, '-')))*/)
			{
				sscanf(q+1, "%u", &nClientRtpPort);
				nClientRtcpPort = nClientRtpPort+1;
			}
			pMgr->nSessionId++;
			pMgr->nSSRC++;
			/*Set "Transport"*/
			for(i = 0; i < RTSP_MAX_LINKS; i++)
			{
				if(pMgr->sClientInfo[i].nSockFd == nClientSockFd)
				{
					p = pMgr->sClientInfo[i].pDestIp;
					pMgr->sClientInfo[i].sStreamRtpFlag[chn].nUseTcp = nClientRtpPort<4 ? 1 : 0;
					pMgr->sClientInfo[i].sStreamRtpFlag[chn].nRtpPort = nClientRtpPort;
					pMgr->sClientInfo[i].sStreamRtpFlag[chn].nRtcpPort = nClientRtcpPort;
					pMgr->sClientInfo[i].sStreamRtpFlag[chn].nSession = pMgr->nSessionId;
					pMgr->sClientInfo[i].sStreamRtpFlag[chn].nSSRC = pMgr->nSSRC;
					pMgr->sClientInfo[i].sStreamRtpFlag[chn].nSeq = 0;
					pMgr->sClientInfo[i].sStreamRtpFlag[chn].nBasePts = 0;
					pMgr->sClientInfo[i].sStreamRtpFlag[chn].nTimeStamp = 0;
					pMgr->sClientInfo[i].sStreamRtpFlag[chn].nCurFrameLen = 0;
					//
					if(0 == pMgr->sClientInfo[i].sStreamRtpFlag[chn].nUseTcp)
					{
						sprintf(pRtspCmd->uAttr.sSetup.pDestination, "destination=%s", p);
						sprintf(pRtspCmd->uAttr.sSetup.pSource, "source=%s", src);
						sprintf(pRtspCmd->uAttr.sSetup.pServerPort, "server_port=%d-%d", RTSP_PORT_RTP,RTSP_PORT_RTCP);
						sprintf(pRtspCmd->uAttr.sSetup.pTransport, 
										"Transport: RTP/AVP;unicast;%s;%s;%s;%s",
										pRtspCmd->uAttr.sSetup.pDestination,
										pRtspCmd->uAttr.sSetup.pSource,
										pRtspCmd->uAttr.sSetup.pClientPort,
										pRtspCmd->uAttr.sSetup.pServerPort);
					}
					else
					{
						sprintf(pRtspCmd->uAttr.sSetup.pTransport, 
										"Transport: RTP/AVP/TCP;unicast;%s",
										pRtspCmd->uAttr.sSetup.pClientPort);
					}
					break;
				}
			}			
			/*Set "Session"*/
			sprintf(pRtspCmd->uAttr.sSetup.pSession, "Session: %x", pMgr->nSessionId);
			/*Set Respond*/
			sprintf(pMgr->pCmdBuf, "%s %s\r\n%s\r\n%s\r\n%s\r\n%s\r\n\r\n", 
							pRtspCmd->pVersion,pRtspCmd->pResult,pRtspCmd->pCSeq,pRtspCmd->pDate,pRtspCmd->uAttr.sSetup.pTransport,pRtspCmd->uAttr.sSetup.pSession);
		}
		else if(0 == strcasecmp(pRtspCmd->pMethod, RTSP_METHOD_PLAY))
		{
			/*check session ID*/
			ret = 0;
			if(NULL != (q = strchr(pRtspCmd->uAttr.sPlay.pSession, ':')))
			{
				q++;
				if(*q == ' ')
					q++;
				sscanf(q, "%x", &nSessionID);
				for(i = 0; i < RTSP_MAX_LINKS; i++)
				{
					if(pMgr->sClientInfo[i].nSockFd == nClientSockFd)
					{
						if(pMgr->sClientInfo[i].sStreamRtpFlag[chn].nSession != nSessionID)
						{
							strcpy(pRtspCmd->pResult, RTSP_RESULT_FAIL);
							printf("Session ID not matched! %x, %x\n", pMgr->sClientInfo[i].sStreamRtpFlag[chn].nSession,nSessionID);
						}
						else
							ret = 1;
						break;
					}
				}
			}
			if(ret)
			{
				/*Send RTCP cmd SR & SDES*/
				RtcpSend(EM_RTCP_TYPE_SR|EM_RTCP_TYPE_SDES, pMgr, i, chn);

				/*Refresh Stream Flag*/
				refreshFlag = 1;
				streamIdx = chn;
				clientIdx = i;
				
				/*Set "RTP-Info"*/
				sprintf(pRtspCmd->uAttr.sPlay.pRtpInfo, "RTP-Info: url=%s;seq=%u;rtptime=%u",
								pRtspCmd->pURL,pMgr->sClientInfo[i].sStreamRtpFlag[chn].nSeq,pMgr->sClientInfo[i].sStreamRtpFlag[chn].nTimeStamp);
				/*Set Respond*/
				sprintf(pMgr->pCmdBuf, "%s %s\r\n%s\r\n%s\r\n%s\r\n%s\r\n%s\r\n\r\n", 
								pRtspCmd->pVersion,pRtspCmd->pResult,pRtspCmd->pCSeq,pRtspCmd->pDate,pRtspCmd->uAttr.sPlay.pRange,pRtspCmd->uAttr.sPlay.pSession,pRtspCmd->uAttr.sPlay.pRtpInfo);
			}
		}
		else if(0 == strcasecmp(pRtspCmd->pMethod, RTSP_METHOD_TEARDOWN))
		{
			for(i = 0; i < RTSP_MAX_LINKS; i++)
			{
				if(pMgr->sClientInfo[i].nSockFd == nClientSockFd)
				{
					RefreshStreamStatus(pMgr, i, chn, 0);
					break;
				}
			}
			
			/*Set Respond*/
			sprintf(pMgr->pCmdBuf, "%s %s\r\n%s\r\n%s\r\n\r\n", 
							pRtspCmd->pVersion,pRtspCmd->pResult,pRtspCmd->pCSeq,pRtspCmd->pDate);
		}
		else
		{
			printf("Should not be here! Unsupport This CMD!\n");
			return -1;
		}	
	}
	/*Wrong CMD*/
	if(0 != strcmp(pRtspCmd->pResult, RTSP_RESULT_OK))
	{
		sprintf(pMgr->pCmdBuf, "%s %s\r\n%s\r\n%s\r\n\r\n", pRtspCmd->pVersion,pRtspCmd->pResult,pRtspCmd->pCSeq,pRtspCmd->pDate);
	}
	/*Sent via TCP*/
	nRemain = strlen(pMgr->pCmdBuf);
	while(nRemain)
	{
		ret = write(nClientSockFd, pMgr->pCmdBuf+nSent, nRemain);
		if(ret < 0)
			break;
		nSent += ret;
		nRemain -= ret;
	}
	printf("\n##Respond Rtsp CMD:\n%s\n", pMgr->pCmdBuf);

	if(refreshFlag)
	{
		usleep(100000);
		RefreshStreamStatus(pMgr, clientIdx, streamIdx, 1);
	}
	
	return 0;
}
static void* RtspMgrFxn(void* pAttr)
{
	SRtspManager *pMgr = (SRtspManager*)pAttr;
	if(NULL == pMgr)
		return NULL;
	int ret = 0, i = 0, j, nMaxFd, nLen = 0, sockfd = -1, flag,offset;
	struct sockaddr_in addr;
	fd_set nFdSet;
	struct timeval tv;
	unsigned int nRcv;
	SRtspCmd sRtspCmd;

	while(pMgr->nRunning)
	{
		FD_ZERO(&nFdSet);
		nMaxFd = pMgr->nRtspSockFd > pMgr->nRtcpSockFd ? pMgr->nRtspSockFd : pMgr->nRtcpSockFd;		
		FD_SET(pMgr->nRtspSockFd, &nFdSet);
		FD_SET(pMgr->nRtcpSockFd, &nFdSet);
		for(i = 0; i < RTSP_MAX_LINKS; i++)
		{
			if(pMgr->sClientInfo[i].nSockFd >= 0)
			{
				FD_SET(pMgr->sClientInfo[i].nSockFd, &nFdSet);
				nMaxFd = (nMaxFd > pMgr->sClientInfo[i].nSockFd) ? nMaxFd : pMgr->sClientInfo[i].nSockFd;
			}
		}
		tv.tv_sec = 2;
		tv.tv_usec = 0;
		ret = select(nMaxFd+1, &nFdSet, NULL, NULL, &tv);
		if(ret < 0)
		{
			printf("select error!!!\n");
			usleep(1000*1000);
		}
		else if(ret == 0)
		{
			//printf("select timeout! no rtsp request!\n");
		}
		else
		{
			if(FD_ISSET(pMgr->nRtspSockFd, &nFdSet))
			{
				//printf("Enter Rtsp server socket...\n");
				memset(&addr, 0, sizeof(addr));
				nLen = sizeof(struct sockaddr_in);
				sockfd = accept(pMgr->nRtspSockFd, (struct sockaddr *)&addr, (socklen_t *)(&nLen));
				if(sockfd < 0)
				{
					printf("accept failed!\n");
				}
				else
				{
					int optval = 1;
					if(-1 == setsockopt(sockfd, IPPROTO_TCP/*SOL_SOCKET*/, TCP_NODELAY, (char*)&optval, sizeof(optval)))
					{
						printf("SetSockLinkOpt:set nodelay failed!\n");
					}
					struct linger m_sLinger;
					m_sLinger.l_onoff = 1;
					m_sLinger.l_linger = 1;
					if(-1 == setsockopt(sockfd,SOL_SOCKET,SO_LINGER,(char*)&m_sLinger,sizeof(struct linger)))
					{
						printf("SetSockLinkOpt:set linger failed!\n");
					}
					optval = fcntl(sockfd, F_GETFL, 0);
					fcntl(sockfd, F_SETFL, optval|O_NONBLOCK);
					
					for(i = 0; i < RTSP_MAX_LINKS; i++)
					{
						if(pMgr->sClientInfo[i].nSockFd < 0)
						{
							break;
						}
					}
					if(i >= RTSP_MAX_LINKS)
					{
						printf("RTSP arrives max link num!\n");
						close(sockfd);
					}
					else
					{
						strcpy(pMgr->sClientInfo[i].pDestIp, (char*)inet_ntoa(addr.sin_addr));
						pMgr->sClientInfo[i].nDestPort = ntohs(addr.sin_port);
						pMgr->sClientInfo[i].nSockFd = sockfd;
						//printf("##Receive a rtsp link! Client:%s:%u\n", pMgr->sClientInfo[i].pDestIp,pMgr->sClientInfo[i].nDestPort);
						//dbgprint(LOG_LEVEL_RTSP,"##Receive a rtsp link! Client:%s:%u\n", pMgr->sClientInfo[i].pDestIp,pMgr->sClientInfo[i].nDestPort);
					}
				}
				//printf("Leave Rtsp server socket...\n");
			}

			if(FD_ISSET(pMgr->nRtcpSockFd, &nFdSet))
			{
				//printf("Enter Rtcp socket...\n");
				memset(&addr, 0, sizeof(addr));
				nLen = sizeof(struct sockaddr_in);
				ret = recvfrom(pMgr->nRtcpSockFd, pMgr->pCmdBuf, sizeof(pMgr->pCmdBuf), 0, 
					            (struct sockaddr*)&addr, (socklen_t*)&nLen);
				//printf("##Receive RTCP Msg (From %s:%d). Len:%d\n%s\n\n",
				//				(char*)inet_ntoa(addr.sin_addr), ntohs(addr.sin_port), ret, pMgr->pCmdBuf);
				//printf("Leave Rtcp socket...\n");
			}
			
			for(i = 0; i < RTSP_MAX_LINKS; i++)
			{
				if(pMgr->sClientInfo[i].nSockFd >= 0)
				{
					if(FD_ISSET(pMgr->sClientInfo[i].nSockFd, &nFdSet))
					{
						flag = 0;
						offset = 0;
						while(1)
						{
							nRcv = read(pMgr->sClientInfo[i].nSockFd,pMgr->pCmdBuf+offset, 1);
							//printf("nRcv=%d\n",nRcv);
							if(nRcv == 1)
							{
								if(flag == 0)//
								{
									if(*(pMgr->pCmdBuf+offset)==0xd)
										flag = 1;
								}
								else if(flag == 1)//has 1 0x0d
								{
									if(*(pMgr->pCmdBuf+offset)==0xa)
										flag = 2;
									else if(*(pMgr->pCmdBuf+offset)!=0xd)
										flag = 0;
								}
								else if(flag == 2)//has 1 0x0d 0x0a
								{
									if(*(pMgr->pCmdBuf+offset)==0xd)
										flag = 3;
									else
										flag = 0;
								}
								else if(flag == 3)//has 1 0x0d 0x0a 0x0d
								{
									if(*(pMgr->pCmdBuf+offset)==0xa)
										flag = 4;
									else if(*(pMgr->pCmdBuf+offset)==0xd)
										flag = 1;
									else
										flag = 0;
								}
								offset += 1;
								//printf("offset=%d, flag=%d\n",offset,flag);
							}
							else
                                break;
							if(flag==4)
								break;
							if(offset == sizeof(pMgr->pCmdBuf)-1)
							{
								printf("rtsp receive buf too small!\n");
								break;
							}
						}
						pMgr->pCmdBuf[offset] = 0;
						//printf("receive %d bytes\n%s",offset,pMgr->pCmdBuf);
						if(offset == 0)//close client socket
						{
							for(j = 0; j < pMgr->sInitAttr.nStreamTotal; j++)
							{
								if((pMgr->sClientInfo[i].nStreamFlag>>j)&1)
									RefreshStreamStatus(pMgr, i, j, 0);
							}
							close(pMgr->sClientInfo[i].nSockFd);
							pMgr->sClientInfo[i].nSockFd = -1;
							//printf("\n######Disconnect client %s:%u! Due to failure when read!\n\n", pMgr->sClientInfo[i].pDestIp,pMgr->sClientInfo[i].nDestPort);
                            //dbgprint(LOG_LEVEL_RTSP,"######Disconnect client %s:%u! Due to failure when read!\n", pMgr->sClientInfo[i].pDestIp,pMgr->sClientInfo[i].nDestPort);
                            continue;
						}
						if(flag != 4)//ignore
						{
							continue;
						}

						if(0 == ParseRtspCmd(pMgr->pCmdBuf, &sRtspCmd))
							DealRtspCmd(pMgr, pMgr->sClientInfo[i].nSockFd, &sRtspCmd);
					}
				}
			}
		}
	}
	return NULL;
}

RtspHandle RtspInit(SRtspInitAttr *pInitAttr)
{
	SRtspManager *pMgr = NULL;
	int i, nReUse = 1;
	struct sockaddr_in sServerAddr;

	if(NULL == pInitAttr || NULL == pInitAttr->pApplyKeyFrame
		  || /*NULL == pInitAttr->pGetStreamPara || */NULL == pInitAttr->pStartStream 
		  || NULL == pInitAttr->pStopStream || NULL == pInitAttr->pPutFrame)
	{
		printf("Invalid para!\n");
		return NULL;
	}
	if(pInitAttr->nStreamTotal > RTSP_STREAM_MAX_NUM)
	{
		printf("Warning: Rtsp supports only %d streams!\n", RTSP_STREAM_MAX_NUM);
		pInitAttr->nStreamTotal = RTSP_STREAM_MAX_NUM;
	}
	if(NULL == (pMgr = (SRtspManager*)calloc(1, sizeof(SRtspManager))))
	{
		printf("Calloc failed!\n");
		return NULL;
	}
	*(pInitAttr->pPutFrame) = RtspPutOneFrame;
	memcpy(&pMgr->sInitAttr, pInitAttr, sizeof(SRtspInitAttr));
	for(i = 0; i < RTSP_MAX_LINKS; i++)
	{
		pMgr->sClientInfo[i].nSockFd = -1;
	}

	/*server socket init*/
    pMgr->nRtspSockFd = socket(AF_INET, SOCK_STREAM, 0);
	if(pMgr->nRtspSockFd < 0)
	{
		printf("Create rtsp socket fails ! ! !\n");
		goto ERR;
	}
	setsockopt(pMgr->nRtspSockFd, SOL_SOCKET, SO_REUSEADDR, (char*)&nReUse, sizeof(nReUse));
	memset(&sServerAddr, 0, sizeof(sServerAddr));
	sServerAddr.sin_family = AF_INET;
	sServerAddr.sin_port = htons(pMgr->sInitAttr.nServerPort);
	if(pMgr->sInitAttr.pServerIp[0])
		sServerAddr.sin_addr.s_addr = inet_addr(pMgr->sInitAttr.pServerIp);
	else
		sServerAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	if(0 != bind(pMgr->nRtspSockFd, (struct sockaddr*)&sServerAddr, sizeof(sServerAddr)))
	{
		printf("Bind rtsp socket failed!\n");
		goto ERR;
	}
	if(-1 == listen(pMgr->nRtspSockFd, 5))
	{
		printf("Listen rtsp socket failed!");
		goto ERR;
	}
	/*rtp socket init*/
    pMgr->nRtpSockFd = socket(AF_INET, SOCK_DGRAM, 0);
	if(pMgr->nRtpSockFd < 0)
	{
		printf("Create rtp socket fails ! ! !\n");
		goto ERR;
	}
	setsockopt(pMgr->nRtpSockFd, SOL_SOCKET, SO_REUSEADDR, (char*)&nReUse, sizeof(nReUse));
	sServerAddr.sin_port = htons(RTSP_PORT_RTP);
	if(0 != bind(pMgr->nRtpSockFd, (struct sockaddr*)&sServerAddr, sizeof(sServerAddr)))
	{
		printf("Bind rtp socket failed!\n");
		goto ERR;
	}
	/*rtcp socket init*/
  pMgr->nRtcpSockFd = socket(AF_INET, SOCK_DGRAM, 0);
	if(pMgr->nRtcpSockFd < 0)
	{
		printf("Create rtcp socket fails ! ! !\n");
		goto ERR;
	}
	setsockopt(pMgr->nRtcpSockFd, SOL_SOCKET, SO_REUSEADDR, (char*)&nReUse, sizeof(nReUse));
	sServerAddr.sin_port = htons(RTSP_PORT_RTCP);
	if(0 != bind(pMgr->nRtcpSockFd, (struct sockaddr*)&sServerAddr, sizeof(sServerAddr)))
	{
		printf("Bind rtcp socket failed!\n");
		goto ERR;
	}

	/*Run Tasks*/
	pMgr->nRunning = 1;
	if(0 != pthread_create(&pMgr->nRtspMgrPid, 0, RtspMgrFxn, (void*)pMgr)
		 #ifdef RTP_USE_CIRCLE_BUF
		 || 0 != pthread_create(&pMgr->nRtspStreamPid, 0, RtspStreamFxn, (void*)pMgr)
		 #endif
		 )
	{
		printf("Create rtsp thread fails ! ! !\n");
		goto ERR;
	}
	return (RtspHandle)pMgr;
	
	ERR:	
	if(pMgr)
	{
		pMgr->nRunning = 0;
		if(pMgr->nRtspMgrPid || pMgr->nRtspStreamPid)
			sleep(2);
		if(pMgr->nRtspSockFd > 0)
			close(pMgr->nRtspSockFd);
		if(pMgr->nRtpSockFd > 0)
			close(pMgr->nRtpSockFd);
		if(pMgr->nRtcpSockFd > 0)
			close(pMgr->nRtcpSockFd);
		free(pMgr);	
	}
	return NULL;
}

