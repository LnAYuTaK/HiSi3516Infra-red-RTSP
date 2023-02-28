#include "sample_comm.h"
#include "mp4v2/mp4v2.h"
typedef  struct     VENC_THREAD_PARA_S
{
        VENC_CHN VeChn; //
        HI_S32  s32Cnt;
		PIC_SIZE_E enSize;
        VENC_CHN_ATTR_S stVencChnAttr;
}VENC_THREAD_PARA_T;

typedef struct _MP4ENC_NaluUnit
{
	int type;
 	int size;
 	unsigned char *data;
}MP4ENC_NaluUnit;

typedef struct _MP4ENC_INFO
{
 	unsigned int u32FrameRate;
	unsigned int u32Width;
 	unsigned int u32Height;
 	unsigned int u32Profile;
}MP4ENC_INFO;

HI_S32 Sample_MP4_WRITE(MP4FileHandle hFile, MP4TrackId *pTrackId,VENC_STREAM_S *pstStream, MP4ENC_INFO *stMp4Info);
HI_VOID* User_VENC_GetStream_Save(HI_VOID* p);

