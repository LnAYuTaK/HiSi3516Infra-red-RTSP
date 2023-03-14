/**
 * @file MPP_VPSS.h
 * @author LnAYuTaK (807874484@qq.com)
 * @brief 
 * @version 0.1
 * @date 2023-03-06
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#pragma once 
#include "sample_comm.h"
class MPP_VPSS
{
public:
    MPP_VPSS(/* args */);
    ~MPP_VPSS();
    HI_S32 Init(VPSS_GRP VpssGrp, 
                HI_BOOL *pabChnEnable, 
                VPSS_GRP_ATTR_S *pstVpssGrpAttr, 
                VPSS_CHN_ATTR_S *pastVpssChnAttr,bool useDefaltConfig);    
    HI_S32 UpInfraredZoom();
    HI_S32 DownInfraredZoom();
    HI_S32 SetVpssZoomLevel(VPSS_GRP VpssGrp, int zoom);
    inline int zoom()const {return this->InfraredZoomLevel;}
private:
    //HI_S32 SetVpssZoomLevel(VPSS_GRP VpssGrp, int zoom);
    int InfraredZoomLevel = 1 ; 

};





