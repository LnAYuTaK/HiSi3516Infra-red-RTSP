#ifndef _RTSP_RTP_YZW_H_
#define _RTSP_RTP_YZW_H_

#ifdef __cplusplus
extern "C" {
#endif

#define RTSP_PORT_SERVER 554

typedef void* RtspHandle;

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

extern RtspHandle RtspInit(SRtspInitAttr *pInitAttr);

/*Usage:
*Type " rtsp://ipaddr:port/liveX.type " in the rtsp client (VLC player etc.) to start rtsp video streaming.
For example:	
	If board's ip is 192.168.1.222, rtsp port is set 554, when client wants chn0 video(h264), type below in vlcPlayer,
	rtsp://192.168.1.222/live0.264  ,
*/

#ifdef __cplusplus
}
#endif

#endif //_RTSP_RTP_YZW_H_
