/**
 * @file MPP_VENC.h
 * @author LnAYuTaK (807874484@qq.com)
 * @brief 
 * @version 0.1
 * @date 2023-03-07
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#pragma once 
#include "sample_comm.h"
using namespace  std;
class MPP_VENC
{
public:
    MPP_VENC(/* args */);
    ~MPP_VENC();
    HI_S32 H264SaveInit(VENC_CHN VencChn, 
                        PIC_SIZE_E enSize, 
                        VENC_GOP_ATTR_S *pstGopAttr);

    HI_S32 JEPGSnapInit(VENC_CHN VencChn, SIZE_S *pstSize);
private:

};

