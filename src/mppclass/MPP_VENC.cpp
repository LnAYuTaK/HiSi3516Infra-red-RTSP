#include "MPP_VENC.h"
#include "CLOG.h"
MPP_VENC::MPP_VENC(/* args */)
{

}
/***********************************************************/
MPP_VENC::~MPP_VENC()
{

}
/***********************************************************/
HI_S32 MPP_VENC::H264SaveInit(VENC_CHN VencChn, 
                              PIC_SIZE_E enSize, 
                              VENC_GOP_ATTR_S *pstGopAttr)
{
    HI_S32 s32Ret =  HI_SUCCESS;
    s32Ret = SAMPLE_COMM_VENC_Start(VencChn,
                                    PT_H264,
                                    enSize, 
                                    SAMPLE_RC_CBR, 
                                    0, 
                                    HI_FALSE, 
                                    pstGopAttr);
   return s32Ret;
}
/***********************************************************/
HI_S32 MPP_VENC::JEPGSnapInit(VENC_CHN VencChn, SIZE_S *pstSize)
{
    HI_S32 s32Ret =  HI_SUCCESS;
    s32Ret = SAMPLE_COMM_VENC_SnapStart(VencChn,pstSize,HI_FALSE);
    return s32Ret;
}
/***********************************************************/








