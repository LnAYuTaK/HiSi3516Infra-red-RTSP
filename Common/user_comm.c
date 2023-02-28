
#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* End of #ifdef __cplusplus */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <sys/select.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <math.h>
#include <unistd.h>
#include <signal.h>
#include <sys/prctl.h>
#include "sample_comm.h"
#include  "user_comm.h"

static HI_S32 Sample_MP4_ReadNalu(HI_U8 *pPack, HI_U32 nPackLen, unsigned int offSet, MP4ENC_NaluUnit *pNaluUnit)
{
	int i = offSet;
	while (i < nPackLen)
	{
		if (pPack[i++] == 0x00 && pPack[i++] == 0x00 && pPack[i++] == 0x00 && pPack[i++] == 0x01)// 开始码
		{
			int pos = i;
			while (pos < nPackLen)
			{
				if (pPack[pos++] == 0x00 && pPack[pos++] == 0x00 && pPack[pos++] == 0x00 && pPack[pos++] == 0x01)
					break;
			}
			if (pos == nPackLen)
				pNaluUnit->size = pos - i;
			else
				pNaluUnit->size = (pos - 4) - i;
				
			pNaluUnit->type = pPack[i] & 0x1f;
			pNaluUnit->data = (unsigned char *)&pPack[i];
			return (pNaluUnit->size + i - offSet);
		}
	}
	return 0;
}

HI_S32 Sample_MP4_WRITE(MP4FileHandle hFile, MP4TrackId *pTrackId,VENC_STREAM_S *pstStream, MP4ENC_INFO *stMp4Info)
{
	int i = 0;
	for (i = 0; i < pstStream->u32PackCount; i++)
	{
		HI_U8 *pPack = pstStream->pstPack[i].pu8Addr + pstStream->pstPack[i].u32Offset;
		HI_U32 nPackLen = pstStream->pstPack[i].u32Len - pstStream->pstPack[i].u32Offset;

		MP4ENC_NaluUnit stNaluUnit;
		memset(&stNaluUnit, 0, sizeof(stNaluUnit));
		int nPos = 0, nLen = 0;
		while ((nLen = Sample_MP4_ReadNalu(pPack, nPackLen, nPos, &stNaluUnit)) != 0)
		{
			switch (stNaluUnit.type)
			{
			case H264E_NALU_SPS:
			if (*pTrackId == MP4_INVALID_TRACK_ID)
     			{
      				*pTrackId = MP4AddH264VideoTrack(hFile, 90000, 90000 / stMp4Info->u32FrameRate, stMp4Info->u32Width, stMp4Info->u32Height, stNaluUnit.data[1], stNaluUnit.data[2], stNaluUnit.data[3], 3);
      				if (*pTrackId == MP4_INVALID_TRACK_ID)
     				{
       					return HI_FAILURE;
     				}
      				MP4SetVideoProfileLevel(hFile, stMp4Info->u32Profile);
      				MP4AddH264SequenceParameterSet(hFile,*pTrackId,stNaluUnit.data,stNaluUnit.size);
     			}
     			break;
			case H264E_NALU_PPS:
    			if (*pTrackId == MP4_INVALID_TRACK_ID)
     			{
      				break;
     			}
			MP4AddH264PictureParameterSet(hFile,*pTrackId,stNaluUnit.data,stNaluUnit.size);
			break;
			case H264E_NALU_IDRSLICE:
			case H264E_NALU_PSLICE:
			{
			if (*pTrackId == MP4_INVALID_TRACK_ID)
      			{
       				break;
      			}
			int nDataLen = stNaluUnit.size + 4;
      			unsigned char *data = (unsigned char *)malloc(nDataLen);
      			data[0] = stNaluUnit.size >> 24;
      			data[1] = stNaluUnit.size >> 16;
      			data[2] = stNaluUnit.size >> 8;
      			data[3] = stNaluUnit.size & 0xff;
      			memcpy(data + 4, stNaluUnit.data, stNaluUnit.size);
			if (!MP4WriteSample(hFile, *pTrackId, data, nDataLen, MP4_INVALID_DURATION, 0, 1))
      			{
       				free(data);
       				return HI_FAILURE;
      			}
			free(data);
			}
			break;
			default :
			break;
		}
		nPos += nLen;
		}
	}
	return HI_SUCCESS;
}

/**
 * @brief  通过Mp4V2 原生转换H264码流成mp4
 * @param p 
 * @return HI_VOID* 
 */
HI_VOID* User_VENC_GetStream_Save(HI_VOID* p)
{ 
    HI_S32 s32Ret;
    fd_set read_fds;
    HI_S32 VencFd,maxfd;
    FILE *pFile;
    HI_S32 i;
    VENC_STREAM_S stStream;
    VENC_CHN_STATUS_S stStat;
    struct timeval TimeoutVal;
    HI_CHAR aszFileName[64];
    VENC_THREAD_PARA_T *pstPara   = (VENC_THREAD_PARA_T *)p;
    VENC_CHN_ATTR_S stVencChnAttr  = pstPara->stVencChnAttr;
	HI_S32 s32ChnTotal = pstPara->s32Cnt;
    VENC_CHN VencChn = pstPara->VeChn;
    PAYLOAD_TYPE_E enPayLoadType = stVencChnAttr.stVencAttr.enType;
	SIZE_S stPicSize;
	s32Ret = SAMPLE_COMM_SYS_GetPicSize(pstPara->enSize,&stPicSize);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Get picture size failed!\n");
        return HI_FAILURE;
    }
    s32Ret = HI_MPI_VENC_GetChnAttr(VencChn, &stVencChnAttr);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("HI_MPI_VENC_GetChnAttr chn[%d] failed with %#x!\n", \
        VencChn, s32Ret);
        return NULL;
    }
    //暂时定义H264
    sprintf(aszFileName, "stream_chn0%d%s",  VencChn, ".h264");//
    pFile = fopen(aszFileName, "wb");
    if (!pFile)
    {
        SAMPLE_PRT("open file[%s] failed!\n", aszFileName);
        return NULL;
    }
    VencFd = HI_MPI_VENC_GetFd(VencChn);//获取编码器的文件描述符，以便后面能用select来IO复用
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
                HI_S32 u32PackIndex;
                for (u32PackIndex= 0;u32PackIndex < stStream.u32PackCount; u32PackIndex++)
                {
                    fwrite(	stStream.pstPack[u32PackIndex].pu8Addr+stStream.pstPack[u32PackIndex].u32Offset,\
                    stStream.pstPack[u32PackIndex].u32Len-  stStream.pstPack[u32PackIndex].u32Offset, \
                            1, pFile);
                    fflush(pFile);
                }
                s32Ret = HI_MPI_VENC_ReleaseStream(VencChn, &stStream);//保存后要释放码流
                if (HI_SUCCESS != s32Ret)
                {
                    free(stStream.pstPack);//获取失败则要释放前面分配的内存，否则会造成内存溢出
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

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */








