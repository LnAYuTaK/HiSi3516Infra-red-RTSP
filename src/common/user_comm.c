#include "sample_comm.h"
#include <sys/select.h>
#include <unistd.h>
#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* End of #ifdef __cplusplus */
/**
 * @brief  保存H264视频流线程//
 * @param   
 * @return HI_VOID* 
 */
HI_VOID* User_VENC_GetStream_Save(HI_VOID* p)
{ 
    HI_S32 s32Ret;
    fd_set read_fds;
    HI_S32 VencFd,maxfd;
    FILE *pFile;
    // HI_S32 i;
    VENC_STREAM_S stStream;
    VENC_CHN_STATUS_S stStat;
    struct timeval TimeoutVal;
    // HI_CHAR aszFileName[64];
    VENC_THREAD_PARA_T *pstPara   = (VENC_THREAD_PARA_T *)p;
    VENC_CHN_ATTR_S stVencChnAttr  = pstPara->stVencChnAttr;
	// HI_S32 s32ChnTotal = pstPara->s32Cnt;
    VENC_CHN VencChn = pstPara->VeChn;
    // PAYLOAD_TYPE_E enPayLoadType = stVencChnAttr.stVencAttr.enType;
    char filename[256];
	memcpy(filename,pstPara->filepath,256);

	SIZE_S stPicSize;
	s32Ret = SAMPLE_COMM_SYS_GetPicSize(pstPara->enSize,&stPicSize);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Get picture size failed!\n");
        return HI_NULL;
    }
    s32Ret = HI_MPI_VENC_GetChnAttr(VencChn, &stVencChnAttr);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("HI_MPI_VENC_GetChnAttr chn[%d] failed with %#x!\n", \
        VencChn, s32Ret);
        return NULL;
    }
    //暂时定义H264
	char savefile[300]    = {0};
    sprintf(savefile, "%s/Ch%d%s",filename,VencChn,".h264");//
    pFile = fopen(savefile, "wb");
    if (!pFile)
    {
        SAMPLE_PRT("open file[%s] failed!\n",savefile);
        return NULL;
    }
    //获取编码器的文件描述符，用select来IO复用
    VencFd = HI_MPI_VENC_GetFd(VencChn);
    if (VencFd < 0)
    {
        SAMPLE_PRT("HI_MPI_VENC_GetFd failed with %#x!\n", VencFd);
        return NULL;
    }
    if (maxfd <= VencFd)
    {
        maxfd = VencFd;
    }
	while (HI_TRUE)
	{
		FD_ZERO(&read_fds);
		FD_SET(VencFd, &read_fds);
		TimeoutVal.tv_sec  = 2;
		TimeoutVal.tv_usec = 0;
		s32Ret = select(maxfd + 1, &read_fds, NULL, NULL, &TimeoutVal);
		if (s32Ret < 0)
		{
			SAMPLE_PRT("select failed!\n");
			break;
		}
		else if (s32Ret == 0)
		{
			SAMPLE_PRT("get venc stream time out, exit thread\n");
			continue;
		}
		else
		{
            if (FD_ISSET(VencFd, &read_fds))
            {
                memset(&stStream, 0, sizeof(stStream));
                //查询是否有码流，并将码流信息填充到stStat结构体中
                s32Ret = HI_MPI_VENC_QueryStatus(VencChn, &stStat);
                if (HI_SUCCESS != s32Ret)
                {
                    SAMPLE_PRT("HI_MPI_VENC_Query chn[%d] failed with %#x!\n",VencChn, s32Ret);
                    break;
                }

                if(0 == stStat.u32CurPacks)
                {
                    SAMPLE_PRT("NOTE: Current  frame is NULL!\n");
                    continue;
                }
                //分配内存以便保存码流包数据
                stStream.pstPack = (VENC_PACK_S*)malloc(sizeof(VENC_PACK_S) * stStat.u32CurPacks);
                if (NULL == stStream.pstPack)
                {
                    SAMPLE_PRT("malloc stream pack failed!\n");
                    break;
                }
                stStream.u32PackCount = stStat.u32CurPacks;
                //获取码流数据并保存到stStream结构体中
                s32Ret = HI_MPI_VENC_GetStream(VencChn, &stStream, HI_TRUE);
                if (HI_SUCCESS != s32Ret)
                {
                    free(stStream.pstPack);//获取失败则要释放前面分配的内存，否则会造成内存溢出
                    stStream.pstPack = NULL;
                    SAMPLE_PRT("HI_MPI_VENC_GetStream failed with %#x!\n", s32Ret);
                    break;
                }
                //写入到文件
                HI_S32 u32PackIndex;
                for (u32PackIndex= 0;u32PackIndex < stStream.u32PackCount; u32PackIndex++)
                {
                    fwrite(	stStream.pstPack[u32PackIndex].pu8Addr+stStream.pstPack[u32PackIndex].u32Offset,\
                    stStream.pstPack[u32PackIndex].u32Len-  stStream.pstPack[u32PackIndex].u32Offset, \
                            1, pFile);
                    fflush(pFile);
                }
                //保存后要释放码流
                s32Ret = HI_MPI_VENC_ReleaseStream(VencChn, &stStream);
                if (HI_SUCCESS != s32Ret)
                {
                     //获取失败则要释放前面分配的内存，否则会造成内存溢出
                    free(stStream.pstPack);
                    stStream.pstPack = NULL;
                    break;
                }
                free(stStream.pstPack);//释放码流后，也要释放分配的内存，避免内存溢出
                stStream.pstPack = NULL;
            }
		}
	}
	fclose(pFile);
	return NULL;
}

static HI_S32 gs_s32SnapCnt = 0;
//抓拍拍照线程函数
HI_VOID *User_VENC_GetJPG_Save(HI_VOID* p)
{
     
    VENC_THREAD_JPEG_PARA_T *para  = (VENC_THREAD_JPEG_PARA_T *)p;
    VENC_CHN VencChn  =  para->VencChn;
	HI_U32   SnapCnt  =  para->SnapCnt;
	HI_BOOL  bSaveJpg =  para->bSaveJpg;
	char filename[256];
	memcpy(filename,para->filepath,256);
    struct timeval TimeoutVal;
    fd_set read_fds;
    HI_S32 s32VencFd;
    VENC_CHN_STATUS_S stStat;
    VENC_STREAM_S stStream;
    HI_S32 s32Ret;
    VENC_RECV_PIC_PARAM_S  stRecvParam;
    MPP_CHN_S stDestChn;
    MPP_CHN_S stSrcChn;
    VPSS_CHN_ATTR_S stVpssChnAttr;
    HI_U32 i;
    //开始保存图片
    stRecvParam.s32RecvPicNum = SnapCnt;
    
    s32Ret = HI_MPI_VENC_StartRecvFrame(VencChn, &stRecvParam);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VENC_StartRecvPic faild with%#x!\n", s32Ret);
        return (void *)HI_FAILURE;
    }

    /******************************************
     step 3:  Start Recv Venc Pictures
    ******************************************/
    stDestChn.enModId  = HI_ID_VENC;
    stDestChn.s32DevId = 0;
    stDestChn.s32ChnId = VencChn;
    s32Ret = HI_MPI_SYS_GetBindbyDest(&stDestChn, &stSrcChn);
    if (s32Ret == HI_SUCCESS && stSrcChn.enModId == HI_ID_VPSS)
    {
        s32Ret = HI_MPI_VPSS_GetChnAttr(stSrcChn.s32DevId, stSrcChn.s32ChnId, &stVpssChnAttr);
        if (s32Ret == HI_SUCCESS && stVpssChnAttr.enCompressMode != COMPRESS_MODE_NONE)
        {
            s32Ret = HI_MPI_VPSS_TriggerSnapFrame(stSrcChn.s32DevId, stSrcChn.s32ChnId, SnapCnt);
            if (s32Ret != HI_SUCCESS)
            {
                SAMPLE_PRT("call HI_MPI_VPSS_TriggerSnapFrame Grp = %d, ChanId = %d, SnapCnt = %d return failed(0x%x)!\n",
                    stSrcChn.s32DevId, stSrcChn.s32ChnId, SnapCnt, s32Ret);

                return (void *)HI_FAILURE;
            }
        }
    }

    //同样的获取FD 用来IO 复用
    s32VencFd = HI_MPI_VENC_GetFd(VencChn);
    if (s32VencFd < 0)
    {
        SAMPLE_PRT("HI_MPI_VENC_GetFd faild with%#x!\n", s32VencFd);
        return (void *)HI_FAILURE;
    }
    //Select 
    for(i=0; i<SnapCnt; i++)
    {
        FD_ZERO(&read_fds);
        FD_SET(s32VencFd, &read_fds);
        TimeoutVal.tv_sec  = 10;
        TimeoutVal.tv_usec = 0;
        s32Ret = select(s32VencFd + 1, &read_fds, NULL, NULL, &TimeoutVal);
        if (s32Ret < 0)
        {
            SAMPLE_PRT("snap select failed!\n");
            return (void *)HI_FAILURE;
        }
        else if (0 == s32Ret)
        {
            SAMPLE_PRT("snap time out!\n");
            return (void *)HI_FAILURE;
        }
        else
        {
            if (FD_ISSET(s32VencFd, &read_fds))
            {
                s32Ret = HI_MPI_VENC_QueryStatus(VencChn, &stStat);
                if (s32Ret != HI_SUCCESS)
                {
                    SAMPLE_PRT("HI_MPI_VENC_QueryStatus failed with %#x!\n", s32Ret);
                    return (void *)HI_FAILURE;
                }
                if (0 == stStat.u32CurPacks)
                {
                    SAMPLE_PRT("NOTE: Current  frame is NULL!\n");
                    return (void *)HI_SUCCESS;
                }
                stStream.pstPack = (VENC_PACK_S*)malloc(sizeof(VENC_PACK_S) * stStat.u32CurPacks);
                if (NULL == stStream.pstPack)
                {
                    SAMPLE_PRT("malloc memory failed!\n");
                    return (void *)HI_FAILURE;
                }
                stStream.u32PackCount = stStat.u32CurPacks;
                s32Ret = HI_MPI_VENC_GetStream(VencChn, &stStream, -1);
                if (HI_SUCCESS != s32Ret)
                {
                    SAMPLE_PRT("HI_MPI_VENC_GetStream failed with %#x!\n", s32Ret);

                    free(stStream.pstPack);
                    stStream.pstPack = NULL;
                    return (void *)HI_FAILURE;
                }
                if(bSaveJpg)
                {
                    FILE* pFile;
					char savefile[300]    = {0};
                    snprintf(savefile,300, "%s/pic_%d.jpg",filename,gs_s32SnapCnt);
                    //这里保存图片
                    SAMPLE_PRT("GET JEPG: %s\n",savefile);
                    pFile = fopen(savefile, "wb");
                    if (pFile == NULL)
                    {
                        SAMPLE_PRT("open file err\n");
                        free(stStream.pstPack);
                        stStream.pstPack = NULL;
                        return (void *)HI_FAILURE;
                    }
                    s32Ret = SAMPLE_COMM_VENC_SaveStream(pFile, &stStream);
                    if (HI_SUCCESS != s32Ret)
                    {
                        SAMPLE_PRT("save snap picture failed!\n");
                        free(stStream.pstPack);
                        stStream.pstPack = NULL;
                        fclose(pFile);
                        return (void *)HI_FAILURE;
                    }
                    fclose(pFile);
                    gs_s32SnapCnt++;
                }

                s32Ret = HI_MPI_VENC_ReleaseStream(VencChn, &stStream);
                if (HI_SUCCESS != s32Ret)
                {
                    SAMPLE_PRT("HI_MPI_VENC_ReleaseStream failed with %#x!\n", s32Ret);
                    free(stStream.pstPack);
                    stStream.pstPack = NULL;
                    return (void *)HI_FAILURE;
                }

                free(stStream.pstPack);
                stStream.pstPack = NULL;
            }
        }
    }
    /******************************************
     step 5:  stop recv picture
    ******************************************/
    s32Ret = HI_MPI_VENC_StopRecvFrame(VencChn);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("HI_MPI_VENC_StopRecvPic failed with %#x!\n",  s32Ret);
        return (void *)HI_FAILURE;
    }
    return (void *)HI_SUCCESS;
}
#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */



