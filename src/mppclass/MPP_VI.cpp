
#include "MPP_VI.h"
#include "CLOG.h"
MPP_VI::MPP_VI(/* args */)
{

}
/***********************************************************/
MPP_VI::~MPP_VI()
{

}
/*************************************************************************/
HI_S32 MPP_VI::Init(SAMPLE_VI_CONFIG_S *pstViConfig ,bool useDefaltConfig)
{
    HI_S32 s32Ret = HI_SUCCESS;
    SAMPLE_VI_CONFIG_S stViConfig;
    PIC_SIZE_E enPicSize;
    SIZE_S stSize;
    SAMPLE_COMM_VI_GetSensorInfo(&stViConfig);
    HI_S32 s32WorkSnsId = 0;
    stViConfig.s32WorkingViNum = 1;
    stViConfig.as32WorkingViId[0] = 0;
    stViConfig.astViInfo[s32WorkSnsId].stSnsInfo.MipiDev = 0;
    stViConfig.astViInfo[s32WorkSnsId].stSnsInfo.s32BusId = 0;
    stViConfig.astViInfo[s32WorkSnsId].stDevInfo.ViDev = 0;
    stViConfig.astViInfo[s32WorkSnsId].stDevInfo.enWDRMode =WDR_MODE_NONE;
    stViConfig.astViInfo[s32WorkSnsId].stPipeInfo.enMastPipeMode = VI_ONLINE_VPSS_OFFLINE;
    stViConfig.astViInfo[s32WorkSnsId].stPipeInfo.aPipe[0] = 0;
    stViConfig.astViInfo[s32WorkSnsId].stPipeInfo.aPipe[1] = -1;
    stViConfig.astViInfo[s32WorkSnsId].stChnInfo.ViChn = 0;
    stViConfig.astViInfo[s32WorkSnsId].stChnInfo.enPixFormat = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
    stViConfig.astViInfo[s32WorkSnsId].stChnInfo.enDynamicRange = DYNAMIC_RANGE_SDR8;
    stViConfig.astViInfo[s32WorkSnsId].stChnInfo.enVideoFormat = VIDEO_FORMAT_LINEAR;
    stViConfig.astViInfo[s32WorkSnsId].stChnInfo.enCompressMode = COMPRESS_MODE_NONE;
    s32Ret = SAMPLE_COMM_VI_GetSizeBySensor(stViConfig.astViInfo[s32WorkSnsId].stSnsInfo.enSnsType, &enPicSize);
    if (HI_SUCCESS != s32Ret)
    {
        LOG_ERROR("%s","get picture size by sensor failed!");
        return s32Ret;
    }
    s32Ret = SAMPLE_COMM_SYS_GetPicSize(enPicSize, &stSize);
    if (stViConfig.astViInfo[s32WorkSnsId].stSnsInfo.enSnsType == BT656_2M_30FPS_8BIT)
    {
        stSize.u32Width = 720;
        stSize.u32Height = 288;
    }
    s32Ret = SAMPLE_COMM_VI_StartVi(&stViConfig);
    return s32Ret;
}
/*************************************************************************/
HI_VOID MPP_VI::GetSensorInfo(SAMPLE_VI_CONFIG_S *pstViConfig)
{
    HI_S32 i;

    for (i = 0; i < VI_MAX_DEV_NUM; i++)
    {
        pstViConfig->astViInfo[i].stSnsInfo.s32SnsId = i;
        pstViConfig->astViInfo[i].stSnsInfo.s32BusId = i;
        pstViConfig->astViInfo[i].stSnsInfo.MipiDev  = i;
        hi_memset(&pstViConfig->astViInfo[i].stSnapInfo, sizeof(SAMPLE_SNAP_INFO_S), 0, sizeof(SAMPLE_SNAP_INFO_S));
        pstViConfig->astViInfo[i].stPipeInfo.bMultiPipe = HI_FALSE;
        pstViConfig->astViInfo[i].stPipeInfo.bVcNumCfged = HI_FALSE;
    }
    pstViConfig->astViInfo[0].stSnsInfo.enSnsType = SENSOR0_TYPE;
}
/*************************************************************************/
HI_S32  MPP_VI::GetSizeBySensor(SAMPLE_SNS_TYPE_E enMode, PIC_SIZE_E *penSize)
{
    HI_S32 s32Ret = HI_SUCCESS;
    if (!penSize)
    {
        return HI_FAILURE;
    }
    switch (enMode)
    {
        case BT1120_2M_30FPS_8BIT:
        case BT656_2M_30FPS_8BIT:
        case BT601_2M_30FPS_8BIT:
        case SONY_IMX327_MIPI_2M_30FPS_12BIT:
        case SONY_IMX327_MIPI_2M_30FPS_12BIT_WDR2TO1:
            //*penSize = PIC_1080P;
            *penSize = PIC_1280x1024;
            break;

        case SONY_IMX307_MIPI_2M_30FPS_12BIT:
        case SONY_IMX307_MIPI_2M_30FPS_12BIT_WDR2TO1:
            *penSize = PIC_1080P;
            break;

        case SMART_SC2231_MIPI_2M_30FPS_10BIT:
        case SOI_JXF37_MIPI_2M_30FPS_10BIT:
        case SMART_SC2235_DC_2M_30FPS_10BIT:
            *penSize = PIC_1080P;
            break;

        case GALAXYCORE_GC2053_MIPI_2M_30FPS_10BIT:
        case GALAXYCORE_GC2053_MIPI_2M_30FPS_10BIT_FORCAR:
            *penSize = PIC_1080P;
            break;

        case SONY_IMX335_MIPI_5M_30FPS_12BIT:
        case SONY_IMX335_MIPI_5M_30FPS_10BIT_WDR2TO1:
            *penSize = PIC_2592x1944;
            break;

        case SONY_IMX335_MIPI_4M_30FPS_12BIT:
        case SONY_IMX335_MIPI_4M_30FPS_10BIT_WDR2TO1:
            *penSize = PIC_2592x1520;
            break;

        case SMART_SC4236_MIPI_3M_30FPS_10BIT:
        case SMART_SC4236_MIPI_3M_20FPS_10BIT:
        case SMART_SC3235_MIPI_3M_30FPS_10BIT:
            *penSize = PIC_2304x1296;
            break;
        case OMNIVISION_OS05A_MIPI_5M_30FPS_12BIT:
            *penSize = PIC_2688x1944;
            break;
        case OMNIVISION_OS05A_MIPI_4M_30FPS_12BIT:
        case OMNIVISION_OS05A_MIPI_4M_30FPS_10BIT_WDR2TO1:
            *penSize = PIC_2688x1536;
            break;

        default:
            *penSize = PIC_1080P;
            break;
    }  
    return s32Ret;
}
