#pragma once 
#include "sample_comm.h"
class MPP_VI
{
public:
    MPP_VI(/* args */);
    ~MPP_VI();
    //初始化如果 使用 DefaltConfig 如果使用默认Config 第一个参数写NULL;
    HI_S32 Init(SAMPLE_VI_CONFIG_S *pstViConfig , bool useDefaltConfig);
    
    HI_VOID GetSensorInfo(SAMPLE_VI_CONFIG_S *pstViConfig);
    HI_S32  GetSizeBySensor(SAMPLE_SNS_TYPE_E enMode, PIC_SIZE_E *penSize);
private:

};
