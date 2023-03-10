#ifndef _RTSP_MANAGER_H_
#define _RTSP_MANAGER_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <sys/mman.h>
#include <stdbool.h>
#include "circlebuf.h"
#include "sample_comm.h"
#include "CLOG.h"
#define RTSP_PORT_SERVER 554
#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* End of #ifdef __cplusplus */
typedef void* RtspHandle;
typedef enum
{
	FRAME_TYPE_P = 1, /*PSLICE types*/
	FRAME_TYPE_I = 5,  /*ISLICE types*/
	FRAME_TYPE_SEI = 6,
	FRAME_TYPE_SPS = 7,
	FRAME_TYPE_PPS = 8,	
}venc_frame_type_e;


typedef struct
{
	unsigned short nW;
	unsigned short nH;
	unsigned short nBitrate;//kbps
	unsigned short nFrameRate;
	unsigned int   nGop;
}SRtspStreamPara;
typedef struct
{
	//unsigned short nW;
	//unsigned short nH;
	unsigned int nStreamIndex;//No. of video chn
	unsigned int nRealLen;    //frame length, exclude the "00 00 00 01" in the head.
	unsigned long /*long*/ nTimeStamp;  //ms 
}SRtspFrameHead;

/*Note: pFrameData should has removed the 00 00 00 01, that is to say, the pFrameData[0] should be 0x67/0x68/0x06/0x65 ....*/
typedef int (*RtspServerPutOneFrame)(RtspHandle pRtspHandle, SRtspFrameHead *pFrameHead, char* pFrameData);

typedef struct
{
	char pServerIp[16]; //set pServerIp[0] = 0, it will use INADDR_ANY
	unsigned short nServerPort; //rtsp port, such as 554 ...
	unsigned char nUseTcp4Stream;//ignore this
	unsigned int nStreamTotal;   //total video chn num
	int (*pStartStream  )(unsigned int nStreamIndex); //start a video chn
	int (*pStopStream   )(unsigned int nStreamIndex); //stop a video chn
	int (*pGetStreamPara)(unsigned int nStreamIndex, SRtspStreamPara *pStreamPara);//get video chn's para
	int (*pApplyKeyFrame)(unsigned int nStreamIndex); //apply an I frame
	RtspServerPutOneFrame *pPutFrame; //register callback to user, use this interface to put a frame into this RTSP module
	//
}SRtspInitAttr;

typedef struct
{ 
	RtspServerPutOneFrame rtsp_put_frame;
	RtspHandle rtsp_handle;
}SRtspGlobalAttr;

typedef struct
{
	unsigned int rtsp_chn_started;
	unsigned int venc_enable_main;
	RtspServerPutOneFrame *pPutFrame; 
	int              venc_fd;
	VPSS_GRP         VpssGrp;
    VPSS_CHN         VpssChn;
	VENC_CHN         VencChn;
	int              rtsp_isworking;
	int              rtsp_toworking;
	SRtspStreamPara  rtspPara;
	PIC_SIZE_E	     enPicSize;//??????venc????????????
}SRtspGroupAttr;

typedef struct  VideoSaveConfig_S
{
	int venc_fd;
	VPSS_GRP       VpssGrp;
    VPSS_CHN       VpssChn;
	VENC_CHN       VencChn;
	PIC_SIZE_E	   enPicSize;//??????venc????????????
}VideoSaveMP4_t;

typedef struct{
	int chn;
	venc_frame_type_e type;
	unsigned long long pts;
	int rsv;
	unsigned int len;
	unsigned char *data;
}venc_stream_s;

#define REC_CHN_NUM 2//??????rtsp??????????????????????

extern RtspHandle RtspInit(SRtspInitAttr *pInitAttr);

/*Usage:
*Type " rtsp://ipaddr:port/liveX.type " in the rtsp client (VLC player etc.) to start rtsp video streaming.
For example:	
	If board's ip is 192.168.1.222, rtsp port is set554 , when client wants chn0 video(h264), type below in vlcPlayer,
	rtsp://192.168.1.222/live0.264  ,
*/

int Rtsp_Init(int num);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */
#endif 