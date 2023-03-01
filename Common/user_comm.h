#include "sample_comm.h"
// #include "mp4v2/mp4v2.h"


typedef struct VENC_THREAD_PARA_S
{
	VENC_CHN	VeChn;//VENC通道
	HI_S32	    s32Cnt;
	PIC_SIZE_E	enSize;
	VENC_CHN_ATTR_S	stVencChnAttr;//
	char        filepath[256];
}VENC_THREAD_PARA_T;

typedef struct  VENC_THREAD_JPEG_PARA_S
{
	VENC_CHN VencChn;   //VENC通道
	HI_U32   SnapCnt;   //抓拍照片数量
	HI_BOOL  bSaveJpg;  //是否保存JPG
	char     filepath[256];
}VENC_THREAD_JPEG_PARA_T;

HI_VOID* User_VENC_GetStream_Save(HI_VOID* p);



HI_VOID *User_VENC_GetJPG_Save(HI_VOID* p);


