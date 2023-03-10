/**
 * @file MPP_SYS.cpp
 * @author LnAYuTaK (807874484@qq.com)
 * @brief 
 * @version 0.1
 * @date 2023-03-07
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#include "MPP_SYS.h"
#include "CLOG.h"
MPP_SYS::MPP_SYS(/* args */)
{
    
}
/***********************************************************/
MPP_SYS::~MPP_SYS()
{

}
/***********************************************************/
HI_S32 MPP_SYS::Init(VB_CONFIG_S* pstVbConfig)
{
    HI_S32 s32Ret = HI_FAILURE;
    HI_MPI_SYS_Exit();
    HI_MPI_VB_Exit();
    if (NULL == pstVbConfig)
    {
        LOG_ERROR("%s","input parameter is null, it is invaild!\n");
        return HI_FAILURE;
    }
    s32Ret = HI_MPI_VB_SetConfig(pstVbConfig);
    if (HI_SUCCESS != s32Ret)
    {
        LOG_ERROR("%s","HI_MPI_VB_SetConf failed!\n");
        return HI_FAILURE;
    }
    s32Ret = HI_MPI_VB_Init();

    if (HI_SUCCESS != s32Ret)
    {
        LOG_ERROR("%s","HI_MPI_VB_Init failed!\n");
        return HI_FAILURE;
    }
    s32Ret = HI_MPI_SYS_Init();

    if (HI_SUCCESS != s32Ret)
    {
        LOG_ERROR("%s","HI_MPI_SYS_Init failed!\n");
        HI_MPI_VB_Exit();
        return HI_FAILURE;
    }

    return HI_SUCCESS;
}
/***********************************************************/