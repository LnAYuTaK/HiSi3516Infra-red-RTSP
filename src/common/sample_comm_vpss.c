
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
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <math.h>
#include <unistd.h>
#include <signal.h>

#include "sample_comm.h"


/*****************************************************************************
* function : start vpss grp.
*****************************************************************************/
HI_S32 SAMPLE_COMM_VPSS_Start(VPSS_GRP VpssGrp, HI_BOOL* pabChnEnable, VPSS_GRP_ATTR_S* pstVpssGrpAttr, VPSS_CHN_ATTR_S* pastVpssChnAttr)
{
    VPSS_CHN VpssChn;
    VPSS_CROP_INFO_S stCropInfo;
    HI_S32 s32Ret;
    HI_S32 j;

    s32Ret = HI_MPI_VPSS_CreateGrp(VpssGrp, pstVpssGrpAttr);

    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("HI_MPI_VPSS_CreateGrp(grp:%d) failed with %#x!\n", VpssGrp, s32Ret);
        return HI_FAILURE;
    }
    

    for (j = 0; j < VPSS_MAX_PHY_CHN_NUM; j++)
    {
        if(HI_TRUE == pabChnEnable[j])
        {
            VpssChn = j;
            s32Ret = HI_MPI_VPSS_GetGrpCrop(0, &stCropInfo);
            if (s32Ret != HI_SUCCESS)
            {
                SAMPLE_PRT("HI_MPI_VPSS_GetGrpCrop failed with %#x\n", s32Ret);
                return HI_FAILURE;
            }
            stCropInfo.bEnable = 1;
            stCropInfo.enCropCoordinate = VPSS_CROP_ABS_COOR;
            stCropInfo.stCropRect.s32X = 0;
            stCropInfo.stCropRect.s32Y = 0;
            stCropInfo.stCropRect.u32Width = 1280/2;
            stCropInfo.stCropRect.u32Height =1024/2;   
            s32Ret = HI_MPI_VPSS_SetGrpCrop(0, &stCropInfo);
            if(s32Ret != HI_SUCCESS)
            {
                printf("Set Crop fail, s32Ret: 0x%x.\n", s32Ret);
                return s32Ret;
            }
            s32Ret = HI_MPI_VPSS_SetChnAttr(VpssGrp, VpssChn, &pastVpssChnAttr[VpssChn]);

            if (s32Ret != HI_SUCCESS)
            {
                SAMPLE_PRT("HI_MPI_VPSS_SetChnAttr failed with %#x\n", s32Ret);
                return HI_FAILURE;
            }
            s32Ret = HI_MPI_VPSS_EnableChn(VpssGrp, VpssChn);

            if (s32Ret != HI_SUCCESS)
            {
                SAMPLE_PRT("HI_MPI_VPSS_EnableChn failed with %#x\n", s32Ret);
                return HI_FAILURE;
            }
        }
    }
    s32Ret = HI_MPI_VPSS_StartGrp(VpssGrp);

    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("HI_MPI_VPSS_StartGrp failed with %#x\n", s32Ret);
        return HI_FAILURE;
    }

    return HI_SUCCESS;
}

/*****************************************************************************
* function : start vpss grp, surport wrap.
*****************************************************************************/
HI_S32 SAMPLE_COMM_VPSS_WRAP_Start(VPSS_GRP VpssGrp, HI_BOOL* pabChnEnable, VPSS_GRP_ATTR_S* pstVpssGrpAttr, VPSS_CHN_ATTR_S* pastVpssChnAttr, VPSS_CHN_BUF_WRAP_S* pstVpssChnBufWrap)
{
    VPSS_CHN VpssChn;
    HI_S32 s32Ret;
    HI_S32 j;

    s32Ret = HI_MPI_VPSS_CreateGrp(VpssGrp, pstVpssGrpAttr);

    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("HI_MPI_VPSS_CreateGrp(grp:%d) failed with %#x!\n", VpssGrp, s32Ret);
        return HI_FAILURE;
    }

    for (j = 0; j < VPSS_MAX_PHY_CHN_NUM; j++)
    {
        if(HI_TRUE == pabChnEnable[j])
        {
            VpssChn = j;
            s32Ret = HI_MPI_VPSS_SetChnAttr(VpssGrp, VpssChn, &pastVpssChnAttr[VpssChn]);

            if (s32Ret != HI_SUCCESS)
            {
                SAMPLE_PRT("HI_MPI_VPSS_SetChnAttr failed with %#x\n", s32Ret);
                return HI_FAILURE;
            }

            if (VPSS_CHN0 == VpssChn)
            {
                s32Ret = HI_MPI_VPSS_SetChnBufWrapAttr(VpssGrp, VpssChn, pstVpssChnBufWrap);
                if (s32Ret != HI_SUCCESS)
                {
                    SAMPLE_PRT("HI_MPI_VPSS_SetChnBufWrapAttr failed with %#x\n", s32Ret);
                    return HI_FAILURE;
                }
            }

            s32Ret = HI_MPI_VPSS_EnableChn(VpssGrp, VpssChn);

            if (s32Ret != HI_SUCCESS)
            {
                SAMPLE_PRT("HI_MPI_VPSS_EnableChn failed with %#x\n", s32Ret);
                return HI_FAILURE;
            }
        }
    }

    s32Ret = HI_MPI_VPSS_StartGrp(VpssGrp);

    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("HI_MPI_VPSS_StartGrp failed with %#x\n", s32Ret);
        return HI_FAILURE;
    }

    return HI_SUCCESS;
}

/*****************************************************************************
* function : stop vpss grp
*****************************************************************************/
HI_S32 SAMPLE_COMM_VPSS_Stop(VPSS_GRP VpssGrp, HI_BOOL* pabChnEnable)
{
    HI_S32 j;
    HI_S32 s32Ret = HI_SUCCESS;
    VPSS_CHN VpssChn;

    for (j = 0; j < VPSS_MAX_PHY_CHN_NUM; j++)
    {
        if(HI_TRUE == pabChnEnable[j])
        {
            VpssChn = j;
            s32Ret = HI_MPI_VPSS_DisableChn(VpssGrp, VpssChn);

            if (s32Ret != HI_SUCCESS)
            {
                SAMPLE_PRT("failed with %#x!\n", s32Ret);
                return HI_FAILURE;
            }
        }
    }

    s32Ret = HI_MPI_VPSS_StopGrp(VpssGrp);

    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("failed with %#x!\n", s32Ret);
        return HI_FAILURE;
    }

    s32Ret = HI_MPI_VPSS_DestroyGrp(VpssGrp);

    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("failed with %#x!\n", s32Ret);
        return HI_FAILURE;
    }

    return HI_SUCCESS;
}



#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */
