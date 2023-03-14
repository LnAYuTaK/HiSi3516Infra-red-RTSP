/**
 * @file MPP_VPSS.cpp
 * @author LnAYuTaK (807874484@qq.com)
 * @brief 
 * @version 0.1
 * @date 2023-03-06
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#include "MPP_VPSS.h"
#include "CLOG.h"
MPP_VPSS::MPP_VPSS(/* args */)
{

}
/***********************************************************/
MPP_VPSS::~MPP_VPSS()
{


}
/***********************************************************/
HI_S32 MPP_VPSS::Init(VPSS_GRP VpssGrp, HI_BOOL *pabChnEnable, 
                VPSS_GRP_ATTR_S *pstVpssGrpAttr, 
                VPSS_CHN_ATTR_S *pastVpssChnAttr,bool useDefalutConfig)
{
    HI_S32 s32Ret = HI_SUCCESS;
    if(useDefalutConfig)
    {
        VPSS_GRP VpssGrp = 0;
        VPSS_CHN VpssChn = VPSS_CHN0;

        //定义VPSS GROUP属性  
        VPSS_GRP_ATTR_S stVpssGrpAttr;
        hi_memset(&stVpssGrpAttr, sizeof(VPSS_GRP_ATTR_S), 0, sizeof(VPSS_GRP_ATTR_S));
        stVpssGrpAttr.stFrameRate.s32SrcFrameRate = -1;
        stVpssGrpAttr.stFrameRate.s32DstFrameRate = -1;
        stVpssGrpAttr.enDynamicRange = DYNAMIC_RANGE_SDR8;
        stVpssGrpAttr.enPixelFormat = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        stVpssGrpAttr.u32MaxW = 1280;
        stVpssGrpAttr.u32MaxH = 1024;
        stVpssGrpAttr.bNrEn = HI_TRUE;
        stVpssGrpAttr.stNrAttr.enCompressMode = COMPRESS_MODE_FRAME;
        stVpssGrpAttr.stNrAttr.enNrMotionMode = NR_MOTION_MODE_NORMAL;

        //设置VPSS 属性
        VPSS_CHN_ATTR_S astVpssChnAttr[VPSS_MAX_PHY_CHN_NUM];
        astVpssChnAttr[VpssChn].u32Width = 1280;
        astVpssChnAttr[VpssChn].u32Height = 1024;
        astVpssChnAttr[VpssChn].enChnMode = VPSS_CHN_MODE_USER;
        astVpssChnAttr[VpssChn].enCompressMode = COMPRESS_MODE_NONE;
        astVpssChnAttr[VpssChn].enDynamicRange = DYNAMIC_RANGE_SDR8;
        astVpssChnAttr[VpssChn].enVideoFormat  = VIDEO_FORMAT_LINEAR;
        astVpssChnAttr[VpssChn].enPixelFormat  = PIXEL_FORMAT_YVU_SEMIPLANAR_420;
        astVpssChnAttr[VpssChn].stFrameRate.s32SrcFrameRate = -1;
        astVpssChnAttr[VpssChn].stFrameRate.s32DstFrameRate = -1;
        astVpssChnAttr[VpssChn].u32Depth = 4;
        astVpssChnAttr[VpssChn].bMirror = HI_FALSE;
        astVpssChnAttr[VpssChn].bFlip = HI_FALSE;
        astVpssChnAttr[VpssChn].stAspectRatio.enMode = ASPECT_RATIO_NONE;
        HI_BOOL abChnEnable[VPSS_MAX_PHY_CHN_NUM]= {HI_FALSE,HI_FALSE,HI_FALSE};
        abChnEnable   [VpssChn] = HI_TRUE;
        //VPSS 开启
        if (HI_SUCCESS!= SAMPLE_COMM_VPSS_Start(VpssGrp,abChnEnable,&stVpssGrpAttr,astVpssChnAttr))
        {
          LOG_ERROR("%s","VPSS Start ERROR");
          return HI_FAILURE;
        }

        return s32Ret;
    } 
    else
    {
        s32Ret = SAMPLE_COMM_VPSS_Start(VpssGrp,pabChnEnable,pstVpssGrpAttr,pastVpssChnAttr);
    }
    return s32Ret;
}
/**********************************************************************/
HI_S32 MPP_VPSS::UpInfraredZoom()
{
    if(InfraredZoomLevel >= 8)
    {
       InfraredZoomLevel = 8;
       LOG_ERROR("%s","Set Zoom OverFlow");
    }
    else
    {
        InfraredZoomLevel = InfraredZoomLevel*2;
        SetVpssZoomLevel(0,InfraredZoomLevel);
    }
    LOG_INFO("%d",InfraredZoomLevel);
    return HI_SUCCESS;
}
/**********************************************************************/
HI_S32 MPP_VPSS::DownInfraredZoom()
{
    HI_S32 s32Ret  = HI_SUCCESS;
    if(InfraredZoomLevel >1)
    {
       InfraredZoomLevel = InfraredZoomLevel/2;
       SetVpssZoomLevel(0,InfraredZoomLevel);
    }
    else if(InfraredZoomLevel <1)
    {
        InfraredZoomLevel =1;
    }
    LOG_INFO("%d",InfraredZoomLevel);
    return HI_SUCCESS;
}
/**********************************************************************/
/*设置 变焦放大 设置裁剪区域  注意VPSS的通道0 可以实现缩放 其他只支持缩小*/
HI_S32 MPP_VPSS::SetVpssZoomLevel(VPSS_GRP VpssGrp, int zoom)
{
    //如果已经相同就退出
    if(zoom <= 0)
    {
       return HI_FAILURE;
    }
    if(InfraredZoomLevel != zoom)
    {
        //设置当前放大倍数
        HI_S32 s32Ret  = HI_SUCCESS;
        VPSS_CROP_INFO_S stCropInfo;
        s32Ret = HI_MPI_VPSS_GetGrpCrop(0, &stCropInfo);
        if (s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("HI_MPI_VPSS_GetGrpCrop failed with %#x\n", s32Ret);
        }
        stCropInfo.bEnable = HI_TRUE;
        stCropInfo.enCropCoordinate = VPSS_CROP_RATIO_COOR;
        // stCropInfo.stCropRect.s32X = 1280/zoom;
        // stCropInfo.stCropRect.s32Y = 1024/zoom;
        //相对坐标设置中心

        stCropInfo.stCropRect.s32X = 500;
        stCropInfo.stCropRect.s32Y = 500;

        if(zoom == 1)
        {  
            LOG_INFO("%s","ZOOOM1");
            stCropInfo.enCropCoordinate =VPSS_CROP_ABS_COOR;
            stCropInfo.stCropRect.s32X = 0;
            stCropInfo.stCropRect.s32Y = 0;
        }
        //设置放大倍数
        stCropInfo.stCropRect.u32Width =  1280/zoom;
        stCropInfo.stCropRect.u32Height = 1024/zoom;   
        s32Ret = HI_MPI_VPSS_SetGrpCrop(VpssGrp,&stCropInfo);
        if(s32Ret != HI_SUCCESS)
        {
            LOG_ERROR("%s","ERRROR Set CrpCrop");
            return HI_FAILURE;
        }
        InfraredZoomLevel = zoom;
        LOG_INFO("VPSS ZOOM LEVEL%d",InfraredZoomLevel);
        return HI_SUCCESS;
    }
    return  HI_FAILURE;

}






