/**
 * @file MPP_SYS.h
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
class MPP_SYS
{
public:
    MPP_SYS(/* args */);
    ~MPP_SYS();
    HI_S32 Init(VB_CONFIG_S* pstVbConfig);
private:
    /* data */
    VB_CONFIG_S* _pstVbConfig;
};


