#ifndef __SAMPLE_VIO_H__
#define __SAMPLE_VIO_H__

#include "hi_common.h"
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
#include <sys/time.h>
#include "Utils/ImageProcessing/imageprocess.h"
#include "Common/sample_comm.h"
#include "Utils/Rtsp/RtspManager.h"
#include "Utils/CircleBuf/circlebuf.h"
#include "Utils/Led/led.h"
#include "Utils/Log/log.h"
#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* End of #ifdef __cplusplus */

#ifndef SAMPLE_PRT
#define SAMPLE_PRT(fmt...)   \
    do {\
        printf("[%s]-%d: ", __FUNCTION__, __LINE__);\
        printf(fmt);\
    }while(0)
#endif

#ifndef PAUSE
#define PAUSE()  do {\
        printf("---------------press Enter key to exit!---------------\n");\
        getchar();\
    } while (0)
#endif


#define   H264_SAVE_ENBLE 
#define   JPG_SAVE_ENBEL

typedef struct iray_vpss_picture
{
    char *pVBufVirt_Y;
    char *pVBufVirt_C;
    char *rgb;
    char *pMemContent;
    char *pMem_VUContent;
    char *pUserPageAddr[2];
    HI_U64 phy_addr;
    HI_U32 u32UvHeight;
    HI_U32 u32Size;
    PIXEL_FORMAT_E enPixelFormat;
    HI_BOOL bUvInvert;
    HI_U16 width;
    HI_U16 heigth;
} IRAY_VPSS_PICTURE;

typedef struct iray_vi_picture
{
    char *pVBufVirt_Y;
    char *pVBufVirt_C;
    char *pMemContent;
    char *pMem_VUContent;
    char *pUserPageAddr[2];
    HI_U64 phy_addr;
    HI_U32 u32UvHeight;
    HI_U32 u32Size;
    PIXEL_FORMAT_E enPixelFormat;
    VIDEO_FORMAT_E enVideoFormat;
    HI_BOOL bUvInvert;
    HI_U32 u32Width;
    HI_U32 u32Height;
} IRAY_VI_PICTURE;

typedef struct thread_data
{
    int picnum;
    char pwd[128];
    char Y[1310720];
    char UV[655360];
    // VIDEO_FRAME_INFO_S send_stFrame;
} THREAD_DATA;

void SAMPLE_VIO_HandleSig(HI_S32 signo);
HI_S32 APP(HI_U32 u32VoIntfType);

HI_S32 SAMPLE_VIO_WDR_LDC_DIS_SPREAD(HI_U32 u32VoIntfType);
HI_S32 SAMPLE_VIO_ViDoublePipeRoute(HI_U32 u32VoIntfType);
HI_S32 SAMPLE_VIO_ViWdrSwitch(HI_U32 u32VoIntfType);
HI_S32 SAMPLE_VIO_ViVpssLowDelay(HI_U32 u32VoIntfType);
HI_S32 SAMPLE_VIO_Rotate(HI_U32 u32VoIntfType);
HI_S32 SAMPLE_VIO_FPN(HI_U32 u32VoIntfType);
HI_S32 SAMPLE_VIO_ResoSwitch(HI_U32 u32VoIntfType);
HI_S32 SAMPLE_VIO_ViDoubleWdrPipe(HI_U32 u32VoIntfType);
HI_S32 SAMPLE_VIO_SetUsrPic(HI_U32 u32VoIntfType);


#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */

#endif /* End of #ifndef __SAMPLE_VIO_H__*/
