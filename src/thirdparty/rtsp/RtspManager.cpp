#include "RtspManager.h"
// static RtspManager *g_RtspManager = NULL;
static pthread_mutex_t g_RtspManagerMutex;

static unsigned int g_dcmdArry[] = {};

SRtspGroupAttr g_sManager_VencCh0; // hong
SRtspGroupAttr g_sManager_VencCh1; // bai
#define VENC_CHN_NUM 1
#define STREAM_TYPE_CNT 3
#define RECV_STREAM_CLIENT_CNT 3
#define CAM_FRAME_SIZE_MAX 1 << 20

SRtspInitAttr rtsp;
SRtspGlobalAttr rtsp_global;

int stream_num = 0;

SRtspGroupAttr choice(unsigned int chn) // 判断返回哪个rtspManger
{
	SRtspGroupAttr ret;
	switch (chn)
	{
	case 0:
		ret = g_sManager_VencCh0;
		break;
	// case 1:
	//	ret = g_sManager_VencCh1;
	//	break;
	default:
		ret = g_sManager_VencCh0; // 找不到默认返回1
		break;
	}
	return ret;
}

HI_U32 SAMPLE_COMM_VENC_GetVencStream(HI_S32 nVencFd, VENC_CHN chn, HI_U64 *nPts, HI_S8 *pBuf, HI_U32 nBufLen, FILE *pFd)
{
	HI_S32 i, j;
	HI_S32 maxfd = -1;
	struct timeval TimeoutVal;
	fd_set read_fds;
	VENC_CHN_STATUS_S stStat;
	VENC_STREAM_S stStream;
	HI_S32 s32Ret;
	HI_U32 nLen = 0;
	/******************************************
	 step 2:  Start to get streams of each channel.
	******************************************/
	for (;;)
	{
		FD_ZERO(&read_fds);
		maxfd = -1;
		if (nVencFd < 0)
			continue;
		FD_SET(nVencFd, &read_fds);
		if (maxfd < nVencFd)
			maxfd = nVencFd;

		if (maxfd < 0)
			break;

		TimeoutVal.tv_sec = 0;
		TimeoutVal.tv_usec = 50 * 1000;
		s32Ret = select(maxfd + 1, &read_fds, NULL, NULL, &TimeoutVal);
		if (s32Ret < 0)
		{
			LOG_ERROR("%s","select failed!");
			break;
		}
		// else if (s32Ret == 0)
		// {
		// 	LOG_ERROR("%s","select  == failed!");
		// 	break;
		// }
		else
		{
			if (FD_ISSET(nVencFd, &read_fds))
			{
				memset(&stStream, 0, sizeof(stStream));
				s32Ret = HI_MPI_VENC_QueryStatus(0, &stStat);
				if (HI_SUCCESS != s32Ret)
				{
					LOG_ERROR("HI_MPI_VENC_Query chn[%d] failed with %#x!\n", 0, s32Ret);
					break;
				}
				if (0 == stStat.u32CurPacks)
				{
					LOG_ERROR("NOTE: Current  frame is NULL!\n");
					continue;
				}
				/*******************************************************
				 step 2.2 : malloc corresponding number of pack nodes.
				*******************************************************/
				stStream.pstPack = (VENC_PACK_S *)malloc(sizeof(VENC_PACK_S) * stStat.u32CurPacks);
				if (NULL == stStream.pstPack)
				{
					SAMPLE_PRT("malloc stream pack failed!\n");
					break;
				}
				/*******************************************************
				 step 2.3 : call mpi to get one-frame stream
				*******************************************************/
				stStream.u32PackCount = stStat.u32CurPacks;
				s32Ret = HI_MPI_VENC_GetStream(0, &stStream, HI_TRUE);
				if (HI_SUCCESS != s32Ret)
				{
					free(stStream.pstPack);
					stStream.pstPack = NULL;
					LOG_ERROR("HI_MPI_VENC_GetStream failed with %#x!\n",
							   s32Ret);
					break;
				}
				/*******************************************************
				 step 2.4 : save frame to file
				*******************************************************/
				for (j = 0; j < stStream.u32PackCount; j++)
				{
					if (nLen + stStream.pstPack[j].u32Len - stStream.pstPack[j].u32Offset <= nBufLen)
						memcpy(pBuf + nLen, stStream.pstPack[j].pu8Addr + stStream.pstPack[j].u32Offset, stStream.pstPack[j].u32Len - stStream.pstPack[j].u32Offset);
					nLen += stStream.pstPack[j].u32Len - stStream.pstPack[j].u32Offset;
					fwrite(stStream.pstPack[j].pu8Addr + stStream.pstPack[j].u32Offset,
					   stStream.pstPack[j].u32Len - stStream.pstPack[j].u32Offset, 1, pFd);

				fflush(pFd);
				}
				*nPts = stStream.pstPack[0].u64PTS;
				// lcc
				/*******************************************************
				 step 2.5 : release stream
				*******************************************************/
				s32Ret = HI_MPI_VENC_ReleaseStream(0, &stStream);
				if (HI_SUCCESS != s32Ret)
				{
					free(stStream.pstPack);
					stStream.pstPack = NULL;
					SAMPLE_PRT("HI_MPI_VENC_ReleaseStream failed with %#x!\n",
							   s32Ret);
					break;
				}
				/*******************************************************
				 step 2.6 : free pack nodes
				*******************************************************/
				free(stStream.pstPack);
				stStream.pstPack = NULL;
				break;
			}
		}
		break;
	}
	return nLen;
}

// IDR帧
int iray_venc_req_IDR(VENC_CHN chn)
{
	HI_MPI_VENC_RequestIDR(0, HI_FALSE);
	return 0;
}

// venc开始工作
static int venc_start(VENC_CHN chn)
{
	PAYLOAD_TYPE_E enType = PT_H264;
	SAMPLE_RC_E enRcMode = SAMPLE_RC_CBR;
	HI_U32 u32Profile = 0;
	HI_BOOL bRcnRefShareBuf = HI_FALSE;
	VENC_GOP_ATTR_S stGopAttr;
	PIC_SIZE_E enPicSize;
	int s32Ret;
	SRtspGroupAttr g_sManager = g_sManager_VencCh0;
	enPicSize = g_sManager.enPicSize; // 这里需要改一下
	stGopAttr.enGopMode = VENC_GOPMODE_SMARTP;
	stGopAttr.stSmartP.s32BgQpDelta = 7;
	stGopAttr.stSmartP.s32ViQpDelta = 2;
	stGopAttr.stSmartP.u32BgInterval = 1200;
	s32Ret = SAMPLE_COMM_VENC_Start(0, enType, enPicSize, enRcMode, u32Profile, bRcnRefShareBuf, &stGopAttr);
	if (HI_SUCCESS != s32Ret)
	{
		SAMPLE_PRT("Error: start venc failed. s32Ret: 0x%x !\n", s32Ret);
	}

   //Rtsp H264 Bind 
	s32Ret = SAMPLE_COMM_VPSS_Bind_VENC(g_sManager.VpssGrp, g_sManager.VpssChn, 0); // 20201010
	if (HI_SUCCESS != s32Ret)
	{
		SAMPLE_PRT("Error: RtspVenc bind Vpss failed. s32Ret: 0x%x !n", s32Ret);
		// to EXIT4;
	}
	g_sManager_VencCh0.venc_fd = HI_MPI_VENC_GetFd(0);
	LOG_INFO("g_sManager_VencCh0.venc_fd = %d", g_sManager_VencCh0.venc_fd);
	if (g_sManager.venc_fd < 0)
	{
		SAMPLE_COMM_VENC_Stop(0);
		return -1;
	}
	return 0;
}

static int  VencUnBindStop(VPSS_GRP   vpssGrp  , VPSS_CHN   vpssChn ,   VENC_CHN vencChn)
{
	SAMPLE_COMM_VPSS_UnBind_VENC(vpssGrp, vpssChn ,vencChn);
	int s32Ret = SAMPLE_COMM_VENC_Stop(vencChn);
    if (HI_SUCCESS != s32Ret)
	{
		LOG_ERROR("%s","venc stop failed");
		return -1;
	}
	return  0;

}
// 停止venc   需要修改
static int venc_stop(VENC_CHN chn)
{
	int s32Ret;
	SRtspGroupAttr g_sManager = g_sManager_VencCh0;
	SAMPLE_COMM_VPSS_UnBind_VENC(g_sManager.VpssGrp, g_sManager.VpssChn, 0);
	s32Ret = SAMPLE_COMM_VENC_Stop(0);
	if (HI_SUCCESS != s32Ret)
	{
		LOG_ERROR("%s","venc stop failed");
		return -1;
	}
	return 0;
}

static unsigned int venc_read(unsigned char *in_buf, unsigned int in_len, venc_stream_s *pout_stream, VENC_CHN chn, FILE *pFd)
{
	HI_U32 nLen = 0;
	if (NULL == in_buf || NULL == pout_stream)
	{
		SAMPLE_PRT("param is valid.\n");
		return 0;
	};
	nLen = SAMPLE_COMM_VENC_GetVencStream(g_sManager_VencCh0.venc_fd, 0, &pout_stream->pts, (HI_S8 *)in_buf, in_len, pFd);
	if (nLen > in_len)
	{
		printf("Venc buf size is too small!! %u\n", nLen);
		nLen = in_len;
	}
	pout_stream->len = nLen;
	pout_stream->data = in_buf;

	return nLen;
}

static void *host_encoding(void *attr)
{
	time_t start = time(0), now;
	int iframe[VENC_CHN_NUM] = {0}, pframe[VENC_CHN_NUM] = {0};
	char *buf = NULL;
	int ret, flag, i;
	SRtspFrameHead rtsp;
	venc_stream_s vstream;
	unsigned int pts, index, streamReqCntPast[VENC_CHN_NUM], streamReqCntCur[VENC_CHN_NUM];
	memset(streamReqCntPast, 0, sizeof(streamReqCntPast));
	buf = (char *)malloc(CAM_FRAME_SIZE_MAX);
	if (buf == NULL)
	{
		printf("malloc buffer failed!!\n");
		return NULL;
	}
	SRtspGroupAttr *g_sManager = (SRtspGroupAttr *)attr;
	LOG_INFO("%s","start venc");
	if (0 == venc_start(g_sManager->VencChn))
	{
		g_sManager->rtsp_isworking = 1;
		LOG_INFO("start encoder chn[%d] OK\n", i);
	}
	else
	{
		LOG_ERROR("start encoder chn[%d] failed!!\n", i);
	}
	char stream_name[256];
	sprintf(stream_name, "/sd/data/%d/stream_%d.h264",stream_num, stream_num);
	FILE *pFd = fopen(stream_name, "wb");
	if (!pFd)
	{
		LOG_ERROR("%s","open file err!");
		return NULL;
	}
	while (1)
	{
		index = 0;
		memset(streamReqCntCur, 0, sizeof(streamReqCntCur));
		if (rtsp_global.rtsp_handle)
		{
			for (i = 0; i < VENC_CHN_NUM; i++)
			{
				if (g_sManager->rtsp_toworking == 1 && g_sManager->rtsp_isworking == 1)
				{
					streamReqCntCur[i]++;
				}
			}
		}
		streamReqCntPast[0] = streamReqCntCur[0];

		/*Read and Send Stream  这里还没改*/
		flag = 0;
		if (g_sManager->rtsp_isworking)
		{
			ret = venc_read((unsigned char *)buf + 14, CAM_FRAME_SIZE_MAX, &vstream, g_sManager->VencChn, pFd);
			if (ret > 0)
			{
				pts = (vstream.pts / 1000) & 0xffffffff;
#ifdef CAM_SKIP_NO_REF_FRAME
				if ((buf[14 + 4] & 0x1f) == 0x61 || (buf[14 + 4] & 0x1f) != 1)
				{
#endif
					/*RTSP SERVER*/
					if (rtsp_global.rtsp_handle)
					{
						rtsp.nRealLen = vstream.len - 4;
						rtsp.nStreamIndex = (g_sManager->VencChn * STREAM_TYPE_CNT) + 0;
						rtsp.nTimeStamp = pts;
						rtsp_global.rtsp_put_frame(rtsp_global.rtsp_handle, &rtsp, buf + 14 + 4);
					}

#ifdef CAM_SKIP_NO_REF_FRAME
				}
#endif
				/*statistics*/
				if ((buf[14 + 4] & 0x1f) == 1)
				{
					pframe[vstream.chn]++;
				}
				else if ((buf[14 + 4] & 0x1f) == 7) // 0x67 0x68 0x06 0x65
				{
					iframe[vstream.chn]++;
				}
				time(&now);
				if (now - start >= 10 || now < start)
				{
					for (i = 0; i < VENC_CHN_NUM; i++)
					{
						printf("main ch%d send %d I and %d P in 10s.\n", i, iframe[i], pframe[i]);
						start = now;
						iframe[i] = 0;
						pframe[i] = 0;
					}
				}
			}
			flag++;
		}
		if (0 == flag)
		{
			usleep(30000);
		}
	}
	if (buf)
		free(buf);
	fclose(pFd);
	return NULL;
}
static int RtspStartStream(unsigned int chn)
{
	LOG_INFO("RtspStartStream  chn = %d\n", chn);
	g_sManager_VencCh0.rtsp_toworking = 1;
	return 0;
}
static int RtspStopStream(unsigned int chn)
{
	LOG_INFO("RtspStopStream  chn = %d\n", chn);
	g_sManager_VencCh0.rtsp_toworking = 0;
	return 0;
}
static int RtspGetStreamPara(unsigned int nStreamIndex, SRtspStreamPara *pStreamPara)
{
	// SRtspGroupAttr g_sManager = g_sManager_VencCh0;
	pStreamPara->nW = g_sManager_VencCh0.rtspPara.nW;
	pStreamPara->nH = g_sManager_VencCh0.rtspPara.nH;
	pStreamPara->nBitrate = g_sManager_VencCh0.rtspPara.nBitrate;
	pStreamPara->nFrameRate = g_sManager_VencCh0.rtspPara.nFrameRate;
	pStreamPara->nGop = g_sManager_VencCh0.rtspPara.nGop;
	return 0;
}
static int RtspApplyKeyFrame(unsigned int  chn)
{
	iray_venc_req_IDR(0);
	return 0;
}

int RtspInit_demo(void)
{
	/*Start RTSP server*/
	//////////////////红外//////////////////
	g_sManager_VencCh0.rtsp_chn_started = 0;
	g_sManager_VencCh0.VpssGrp = 0; // 1
	g_sManager_VencCh0.VpssChn = 0; // 2
	g_sManager_VencCh0.VencChn = 0;
	g_sManager_VencCh0.enPicSize = PIC_1280x1024; // 1280*720

	g_sManager_VencCh0.rtsp_isworking = 0;
	g_sManager_VencCh0.rtsp_toworking = 0;

	g_sManager_VencCh0.rtspPara.nH = 1024;
	g_sManager_VencCh0.rtspPara.nW = 1280;
	g_sManager_VencCh0.rtspPara.nGop = 60;
	g_sManager_VencCh0.rtspPara.nFrameRate = 30;
	g_sManager_VencCh0.rtspPara.nBitrate = (1 << 10);
	///////////////////////////////////////

	////////////////可见光////////////////////
	//	g_sManager_VencCh1.rtsp_chn_started = 1;
	//	g_sManager_VencCh1.VpssGrp = 0;
	//	g_sManager_VencCh1.VpssChn = 1;
	//	g_sManager_VencCh1.VencChn = 1;
	//	g_sManager_VencCh1.enPicSize = PIC_720P;//1280*720

	//	g_sManager_VencCh1.rtsp_isworking = false;
	//	g_sManager_VencCh1.rtsp_toworking = false;
	//
	//	g_sManager_VencCh1.rtspPara.nH = 720;
	//	g_sManager_VencCh1.rtspPara.nW = 1280;
	//	g_sManager_VencCh1.rtspPara.nGop = 60;
	//	g_sManager_VencCh1.rtspPara.nFrameRate = 30;
	//	g_sManager_VencCh1.rtspPara.nBitrate = (1<<10);
	///////////////////////////////////////

	memset(&rtsp, 0, sizeof(SRtspInitAttr));
	rtsp.nServerPort = RTSP_PORT_SERVER;
	rtsp.nStreamTotal = VENC_CHN_NUM * STREAM_TYPE_CNT;
	rtsp.pStartStream = RtspStartStream;
	rtsp.pStopStream = RtspStopStream;
	rtsp.pGetStreamPara = RtspGetStreamPara;
	rtsp.pApplyKeyFrame = RtspApplyKeyFrame;
	rtsp.pPutFrame = &rtsp_global.rtsp_put_frame;
	if (NULL == (rtsp_global.rtsp_handle = RtspInit(&rtsp)))
	{
		LOG_ERROR("%s","Init Rtsp failed!");
		return -1;
	}
	pthread_t pid1;	// 红外
	if (0 != pthread_create(&pid1, NULL, (void *(*)(void *))host_encoding, (void *)(&g_sManager_VencCh0))) // 开启通道0的rtsp
	{
		LOG_ERROR("%s","can't hong create thread host encode!!\n");
		return -1;
	}
	return 0;
}

int Rtsp_Init(int num)
{
	stream_num = num;
	RtspInit_demo(); // 开启一个rtsp流
}
