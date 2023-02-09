#ifdef __cplusplus
#if __cplusplus
extern "C"
{
#endif
#endif /* End of #ifdef __cplusplus */

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

#define mem_size 20 * 1024 // 20k
#include "sample_comm.h"
#include <sys/time.h>
#include "led.h"
#include "log.h"
#include "RtspManager.h"

//#define YUV2JPEG

#ifdef YUV2JPEG
#include <jpeglib.h>
#endif

    char logtmp[1024];

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

    int m_pseudoColor_index = 5;             // 伪彩号
    unsigned char *pMem_pseudo_color = NULL; // 存储伪彩色板

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
    static IRAY_VPSS_PICTURE vpss_picture;
    static VIDEO_FRAME_INFO_S vpss_stFrame; // vpss frame

    typedef struct thread_data
    {
        int picnum;
        char pwd[128];
        char Y[1310720];
        char UV[655360];
        // VIDEO_FRAME_INFO_S send_stFrame;
    } THREAD_DATA;

    int picflag = 0;

    void *yuv2jpgtask(void *p);

#ifdef YUV2JPEG

    char clip_value(char value, int min, int max)
    {
        if (value < min)
        {
            return min;
        }
        if (value > max)
        {
            return max;
        }
        return value;
    }

    int YUV420_TO_RGB24(char *yuvBuf_y, char *yuvBuf_uv, int w, int h, char *rgbBuf)
    {
        int index_y, index_u, index_v;
        char y, u, v;
        char r, g, b;

        for (size_t i = 0; i < h; i++)
        {
            for (size_t j = 0; j < w; j++)
            {

                index_y = i * w + j;
                index_u = (i / 2) * w + j - j % 2;
                index_v = index_u + 1;

                y = yuvBuf_y[index_y];

                u = yuvBuf_uv[index_u];
                v = yuvBuf_uv[index_v];

                b = y + 1.772 * (u - 128);                         // B = Y +1.779*(U-128)
                g = y - 0.34413 * (u - 128) - 0.71414 * (v - 128); // G = Y-0.3455*(U-128)-0.7169*(V-128)
                r = y + 1.402 * (v - 128);                         // R = Y+1.4075*(V-128)

                *(rgbBuf++) = clip_value(r, 0, 255);
                *(rgbBuf++) = clip_value(g, 0, 255);
                *(rgbBuf++) = clip_value(b, 0, 255);
            }
        }

        return 1;
    }

    int rgb2jpg(char *jpg_file, char *pdata, int width, int height)
    {
        int depth = 3;
        JSAMPROW row_pointer[1];
        struct jpeg_compress_struct cinfo;
        struct jpeg_error_mgr jerr;
        FILE *outfile;

        if ((outfile = fopen(jpg_file, "wb")) == NULL)
        {
            return -1;
        }

        cinfo.err = jpeg_std_error(&jerr);
        jpeg_create_compress(&cinfo);
        jpeg_stdio_dest(&cinfo, outfile);

        cinfo.image_width = width;
        cinfo.image_height = height;
        cinfo.input_components = depth;
        cinfo.in_color_space = JCS_RGB;
        jpeg_set_defaults(&cinfo);

        jpeg_set_quality(&cinfo, 100, TRUE);
        jpeg_start_compress(&cinfo, TRUE);

        int row_stride = width * depth;
        while (cinfo.next_scanline < cinfo.image_height)
        {
            row_pointer[0] = (JSAMPROW)(pdata + cinfo.next_scanline * row_stride);
            jpeg_write_scanlines(&cinfo, row_pointer, 1);
        }

        jpeg_finish_compress(&cinfo);
        jpeg_destroy_compress(&cinfo);
        fclose(outfile);

        return 0;
    }
#endif
    int strlen_xun(char arr[]) // 循环方法
    {
        int i = 0;
        while (arr[i] != '\0')
        {
            ++i;
        }
        return i;
    }

    // yuv422
    // group 组号
    // chn  通道号
    // pwd  地址
    // 图像格式最好在vpss转换成yuv422sp，其他得格式按照实际进行保存
    // YYYYYYYY
    // UVUVUVUV
    // 增加isTakePic开关，如果要拍照保存就置1，完成伪彩+拍照功能
    // isTakePic=0,只进行伪彩映射。
    void vpss_one_frame(int vpssGrp, int vpssChn, char *pwd, int picnum, int isTakePic)
    {

        int try_num = 10; // 尝试次数
        int s32Ret = 0;

        memset(&vpss_stFrame, 0, sizeof(vpss_stFrame));
        vpss_stFrame.u32PoolId = VB_INVALID_POOLID;
        ////
        unsigned char m_color_value = 128;
        unsigned int w = 0, h = 0;
        unsigned char *pMem_red_origin = NULL; // Y
        // unsigned char index = 0;
        int index = 0;
        unsigned char *UV_data = NULL;
        // unsigned char * V_data = NULL;
        unsigned char m_line_Y[1280];
        unsigned char m_line_uv[1280];

        VI_CHN_ATTR_S stChnAttr;

        s32Ret = HI_MPI_VI_GetChnAttr(0, 0, &stChnAttr);

        // u32OrigDepth = stChnAttr.u32Depth;
        stChnAttr.u32Depth = 2;
        s32Ret = HI_MPI_VI_SetChnAttr(0, 0, &stChnAttr);

        ////
        while (1)
        {
            // if (HI_MPI_VPSS_GetGrpFrame(vpssGrp, vpssChn, &vpss_stFrame) != HI_SUCCESS) // 尝试取帧
            if (HI_MPI_VI_GetChnFrame(0, 0, &vpss_stFrame, 50) != HI_SUCCESS) // 尝试取帧
            {

                if (vpss_stFrame.u32PoolId != VB_INVALID_POOLID)
                {
                    // s32Ret = HI_MPI_VPSS_ReleaseGrpFrame(vpssGrp, vpssChn, &vpss_stFrame);
                    s32Ret = HI_MPI_VI_ReleaseChnFrame(0, 0, &vpss_stFrame);
                    if (HI_SUCCESS != s32Ret)
                        printlog("vpss Release frame error ,now exit !!!\n");
                    vpss_stFrame.u32PoolId = VB_INVALID_POOLID;
                }
                try_num--;
                if (try_num < 0)
                {
                    goto EXI1;
                }

                printlog("Get vpss frame fail!2 \n");
                usleep(200);
                // lcc continue;
                goto EXI1;
            }
            else
                break; // 拿到帧了
        }
        VIDEO_FRAME_S *pVpssVBuf = &(vpss_stFrame.stVFrame);
        vpss_picture.phy_addr = pVpssVBuf->u64PhyAddr[0];

        vpss_picture.pUserPageAddr[0] = (HI_CHAR *)HI_MPI_SYS_Mmap(vpss_picture.phy_addr, 1280 * 1024 * 3 / 2);
        // vpss_picture.pUserPageAddr[0] = (HI_CHAR *)HI_MPI_SYS_Mmap(vpss_picture.phy_addr, 1280 * 1024 * 2);
        if (HI_NULL == vpss_picture.pUserPageAddr[0])
        {
            return;
        }
        vpss_picture.pVBufVirt_Y = vpss_picture.pUserPageAddr[0];          // Y分量
        vpss_picture.pVBufVirt_C = vpss_picture.pVBufVirt_Y + 1280 * 1024; // UV分量

#if 0
        memset(vpss_picture.pVBufVirt_C, 0x80, 1280 * 512);
#endif
        //s32Ret = HI_MPI_VPSS_SendFrame(0, 0, &vpss_stFrame, 50);

#if 1
        if (isTakePic == 1)
        {
            /////////////////////////////////////////////////////////////
// 伪彩  420p图像格式
#if 0
            int index_first = 1024 * (m_pseudoColor_index);
            bool h_flag = 1;
            bool w_flag = 1;
            int Y_OFF = 0;
            int C_OFF = 0;
            for (h = 0; h < 1024; h++)
            {

                // YUV420
                pMem_red_origin = vpss_picture.pVBufVirt_Y + Y_OFF;
                if (h_flag)
                {
                    UV_data = vpss_picture.pVBufVirt_C + C_OFF;
                    C_OFF = C_OFF + 1280;
                }
                for (w = 0; w < 1280; w++)
                {
                    m_color_value = *(pMem_red_origin);
                    // m_color_value = pMem_red_origin[w];
                    if (m_pseudoColor_index == 1) // 黑热
                    {
                        m_color_value = 255 - m_color_value;
                    }
                    index = index_first + (HI_U32)m_color_value * 4;
                    *(pMem_red_origin++) == pMem_pseudo_color[index + 2];
                    // pMem_red_origin[w] == pMem_pseudo_color[index + 2];
                    if (h_flag && w_flag)
                    {
                        *(UV_data++) = pMem_pseudo_color[index + 1];
                        *(UV_data++) = pMem_pseudo_color[index];
                        // UV_data[w] = pMem_pseudo_color[index + 1];
                        // UV_data[w + 1] = pMem_pseudo_color[index];
                    }
                    w_flag = !w_flag;
                }
                // memcpy(pMem_red_origin, m_line_Y, 1280);
                // if (h_flag)
                // {
                //     memcpy(UV_data, m_line_uv, 1280);
                //     C_OFF = C_OFF + 1280;
                // }
                h_flag = !h_flag;
                Y_OFF = Y_OFF + 1280;
            }

#endif
            pthread_t picture_deal;
            THREAD_DATA *yuv2jpg_info = (THREAD_DATA *)malloc(sizeof(THREAD_DATA));

            yuv2jpg_info->picnum = picnum;
            memcpy(yuv2jpg_info->pwd, pwd, strlen_xun(pwd));
            memcpy(yuv2jpg_info->Y, vpss_picture.pVBufVirt_Y, 1280 * 1024);
            memcpy(yuv2jpg_info->UV, vpss_picture.pVBufVirt_C, 1280 * 512);

            int s32Ret = pthread_create(&picture_deal, 0, yuv2jpgtask, yuv2jpg_info);

            sprintf(logtmp, "thread %d cread s32Ret %d\n", picnum, s32Ret);
            printlog(logtmp);

            pthread_detach(picture_deal);
        }
#endif

        HI_MPI_SYS_Munmap(vpss_picture.phy_addr, 1280 * 1024 * 3 / 2);
        // HI_MPI_SYS_Munmap(vpss_picture.phy_addr, 1280 * 1024 * 2);
        //  将帧返回内存
        if (vpss_stFrame.u32PoolId != VB_INVALID_POOLID)
        {

            // s32Ret = HI_MPI_VPSS_ReleaseChnFrame(vpssGrp, vpssChn, &vpss_stFrame);
            s32Ret = HI_MPI_VI_ReleaseChnFrame(0, 0, &vpss_stFrame);
            if (HI_SUCCESS != s32Ret)
                printlog("Release frame error ,now exit !!!\n");
            vpss_stFrame.u32PoolId = VB_INVALID_POOLID;
        }
        return 1;

    EXI1:
        try_num = 10;
        return -1;
    }

#ifndef __HuaweiLite_
    void SAMPLE_VIO_HandleSig(HI_S32 signo)
    {
        signal(SIGINT, SIG_IGN);
        signal(SIGTERM, SIG_IGN);

        if (SIGINT == signo || SIGTERM == signo)
        {
            //SAMPLE_COMM_VENC_StopGetStream();
            //SAMPLE_COMM_All_ISP_Stop();
            SAMPLE_COMM_SYS_Exit();
            printlog("\033[0;31mprogram termination abnormally!\033[0;39m\n");
        }
        exit(-1);
    }
#endif

    HI_S32 SAMPLE_VIO_ViOnlineVpssOnlineRoute(HI_U32 u32VoIntfType)
    {
    }

    HI_S32 SAMPLE_VIO_ViOfflineVpssOnline_LDC_Rotation(HI_U32 u32VoIntfType)
    {
    }

    HI_S32 SAMPLE_VIO_ViOnlineVpssOffline_ISPDIS(HI_U32 u32VoIntfType)
    {
        HI_S32 s32Ret = HI_SUCCESS;

        HI_S32 s32ViCnt = 1;
        VI_DEV ViDev = 0;
        VI_PIPE ViPipe = 0;
        VI_CHN ViChn = 0;
        HI_S32 s32WorkSnsId = 0;
        SAMPLE_VI_CONFIG_S stViConfig;

        SIZE_S stSize;
        VB_CONFIG_S stVbConf;
        PIC_SIZE_E enPicSize;
        HI_U32 u32BlkSize;

        VO_CHN VoChn = 0;
        SAMPLE_VO_CONFIG_S stVoConfig;

        WDR_MODE_E enWDRMode = WDR_MODE_NONE;
        DYNAMIC_RANGE_E enDynamicRange = DYNAMIC_RANGE_SDR8;
        // 0115 420-422
        PIXEL_FORMAT_E enPixFormat = PIXEL_FORMAT_YVU_SEMIPLANAR_420; // PIXEL_FORMAT_YVU_SEMIPLANAR_422
        VIDEO_FORMAT_E enVideoFormat = VIDEO_FORMAT_LINEAR;
        COMPRESS_MODE_E enCompressMode = COMPRESS_MODE_NONE;
        VI_VPSS_MODE_E enMastPipeMode = VI_ONLINE_VPSS_OFFLINE;

        VPSS_GRP VpssGrp = 0;
        VPSS_GRP_ATTR_S stVpssGrpAttr;
        VPSS_CHN VpssChn = VPSS_CHN1;
        VPSS_CHN VpssChn2 = VPSS_CHN2;
        HI_BOOL abChnEnable[VPSS_MAX_PHY_CHN_NUM] = {0};
        VPSS_CHN_ATTR_S astVpssChnAttr[VPSS_MAX_PHY_CHN_NUM];

        ISP_DIS_ATTR_S stDISAttr;
        int fd = -1;
        #if 0
        //////////////////////////////////////////////////////////
        // 增加伪彩色板的初始化读取
        
        pMem_pseudo_color = (HI_U8 *)malloc(mem_size * sizeof(HI_U8));
        if (pMem_pseudo_color == NULL)
        {
            printlog("Error: malloc fail!\n");
            return -1;
        }
        memset(pMem_pseudo_color, 0, mem_size * sizeof(HI_U8));

        fd = open("./ColorMapV2_3.dat", O_RDWR); // 色板存放在根目录下。
        if (-1 == fd)
        {
            printlog("Error: Open pseudo_color planner data error\n");
            return -1;
        }
        s32Ret = read(fd, pMem_pseudo_color, mem_size);
        if (-1 == s32Ret)
        {
            printlog("Error: read pseudo_color file fail\n");
            return -1;
        }
        #endif
        //////////////////////////////////////////////////
        /*config vi*/
        SAMPLE_COMM_VI_GetSensorInfo(&stViConfig);

        stViConfig.s32WorkingViNum = s32ViCnt;
        stViConfig.as32WorkingViId[0] = 0;
        stViConfig.astViInfo[s32WorkSnsId].stSnsInfo.MipiDev = ViDev;
        stViConfig.astViInfo[s32WorkSnsId].stSnsInfo.s32BusId = 0;
        stViConfig.astViInfo[s32WorkSnsId].stDevInfo.ViDev = ViDev;
        stViConfig.astViInfo[s32WorkSnsId].stDevInfo.enWDRMode = enWDRMode;
        stViConfig.astViInfo[s32WorkSnsId].stPipeInfo.enMastPipeMode = enMastPipeMode;
        stViConfig.astViInfo[s32WorkSnsId].stPipeInfo.aPipe[0] = ViPipe;
        stViConfig.astViInfo[s32WorkSnsId].stPipeInfo.aPipe[1] = -1;
        stViConfig.astViInfo[s32WorkSnsId].stChnInfo.ViChn = ViChn;
        stViConfig.astViInfo[s32WorkSnsId].stChnInfo.enPixFormat = enPixFormat;
        stViConfig.astViInfo[s32WorkSnsId].stChnInfo.enDynamicRange = enDynamicRange;
        stViConfig.astViInfo[s32WorkSnsId].stChnInfo.enVideoFormat = enVideoFormat;
        stViConfig.astViInfo[s32WorkSnsId].stChnInfo.enCompressMode = enCompressMode;

        /*get picture size*/
        s32Ret = SAMPLE_COMM_VI_GetSizeBySensor(stViConfig.astViInfo[s32WorkSnsId].stSnsInfo.enSnsType, &enPicSize);
        if (HI_SUCCESS != s32Ret)
        {
            printlog("get picture size by sensor failed!\n");
            return s32Ret;
        }

        s32Ret = SAMPLE_COMM_SYS_GetPicSize(enPicSize, &stSize);

        if (stViConfig.astViInfo[s32WorkSnsId].stSnsInfo.enSnsType == BT656_2M_30FPS_8BIT)
        {
            stSize.u32Width = 720;
            stSize.u32Height = 288;
        }

        if (HI_SUCCESS != s32Ret)
        {
            printlog("get picture size failed!\n");
            return s32Ret;
        }

        /*config vb*/
        hi_memset(&stVbConf, sizeof(VB_CONFIG_S), 0, sizeof(VB_CONFIG_S));
        stVbConf.u32MaxPoolCnt = 2;

        u32BlkSize = COMMON_GetPicBufferSize(stSize.u32Width, stSize.u32Height, SAMPLE_PIXEL_FORMAT, DATA_BITWIDTH_8, COMPRESS_MODE_NONE, DEFAULT_ALIGN);
        stVbConf.astCommPool[0].u64BlkSize = u32BlkSize;
        // stVbConf.astCommPool[0].u32BlkCnt   = 3;
        stVbConf.astCommPool[0].u32BlkCnt = 16;


        s32Ret = SAMPLE_COMM_SYS_Init(&stVbConf);
        if (HI_SUCCESS != s32Ret)
        {
            sprintf(logtmp, "system init failed with %d!\n", s32Ret);
            printlog(logtmp);
            return s32Ret;
        }

        /*start vi*/
        s32Ret = SAMPLE_COMM_VI_StartVi(&stViConfig);
        if (HI_SUCCESS != s32Ret)
        {
            sprintf(logtmp, "start vi failed.s32Ret:0x%x !\n", s32Ret);
            printlog(logtmp);
            goto EXIT;
        }

        /*config vpss*/
        hi_memset(&stVpssGrpAttr, sizeof(VPSS_GRP_ATTR_S), 0, sizeof(VPSS_GRP_ATTR_S));
        stVpssGrpAttr.stFrameRate.s32SrcFrameRate = -1;
        stVpssGrpAttr.stFrameRate.s32DstFrameRate = -1;
        stVpssGrpAttr.enDynamicRange = DYNAMIC_RANGE_SDR8;
        stVpssGrpAttr.enPixelFormat = enPixFormat;
        stVpssGrpAttr.u32MaxW = stSize.u32Width;
        stVpssGrpAttr.u32MaxH = stSize.u32Height;
        stVpssGrpAttr.bNrEn = HI_TRUE;
        stVpssGrpAttr.stNrAttr.enCompressMode = COMPRESS_MODE_FRAME;
        stVpssGrpAttr.stNrAttr.enNrMotionMode = NR_MOTION_MODE_NORMAL;

        astVpssChnAttr[VpssChn].u32Width = stSize.u32Width;
        astVpssChnAttr[VpssChn].u32Height = stSize.u32Height;
        astVpssChnAttr[VpssChn].enChnMode = VPSS_CHN_MODE_USER;
        astVpssChnAttr[VpssChn].enCompressMode = enCompressMode;
        astVpssChnAttr[VpssChn].enDynamicRange = enDynamicRange;
        astVpssChnAttr[VpssChn].enVideoFormat = enVideoFormat;
        astVpssChnAttr[VpssChn].enPixelFormat = enPixFormat;
        astVpssChnAttr[VpssChn].stFrameRate.s32SrcFrameRate = -1;
        astVpssChnAttr[VpssChn].stFrameRate.s32DstFrameRate = -1;
        astVpssChnAttr[VpssChn].u32Depth = 4;
        astVpssChnAttr[VpssChn].bMirror = HI_FALSE;
        astVpssChnAttr[VpssChn].bFlip = HI_FALSE;
        astVpssChnAttr[VpssChn].stAspectRatio.enMode = ASPECT_RATIO_NONE;
        // VpssChn2
        // astVpssChnAttr[VpssChn2].u32Width = stSize.u32Width;
        // astVpssChnAttr[VpssChn2].u32Height = stSize.u32Height;
        // astVpssChnAttr[VpssChn2].enChnMode = VPSS_CHN_MODE_USER;
        // astVpssChnAttr[VpssChn2].enCompressMode = enCompressMode;
        // astVpssChnAttr[VpssChn2].enDynamicRange = enDynamicRange;
        // astVpssChnAttr[VpssChn2].enVideoFormat = enVideoFormat;
        // astVpssChnAttr[VpssChn2].enPixelFormat = enPixFormat;
        // astVpssChnAttr[VpssChn2].stFrameRate.s32SrcFrameRate = -1;
        // astVpssChnAttr[VpssChn2].stFrameRate.s32DstFrameRate = -1;
        // astVpssChnAttr[VpssChn2].u32Depth = 4;
        // astVpssChnAttr[VpssChn2].bMirror = HI_FALSE;
        // astVpssChnAttr[VpssChn2].bFlip = HI_FALSE;
        // astVpssChnAttr[VpssChn2].stAspectRatio.enMode = ASPECT_RATIO_NONE;

        /*start vpss*/
        abChnEnable[VPSS_CHN1] = HI_TRUE;
        //abChnEnable[VPSS_CHN2] = HI_TRUE;
        s32Ret = SAMPLE_COMM_VPSS_Start(VpssGrp, abChnEnable, &stVpssGrpAttr, astVpssChnAttr);
        if (HI_SUCCESS != s32Ret)
        {
            sprintf(logtmp, "start vpss group failed. s32Ret: 0x%x !\n", s32Ret);
            printlog(logtmp);
            goto EXIT1;
        }

        // vpss group 1
        // s32Ret = SAMPLE_COMM_VPSS_Start(1, abChnEnable, &stVpssGrpAttr, astVpssChnAttr);
        // if (HI_SUCCESS != s32Ret)
        // {
        //     sprintf(logtmp, "start vpss group failed. s32Ret: 0x%x !\n", s32Ret);
        //     printlog(logtmp);
        //     goto EXIT1;
        // }

        // VPSS_CROP_INFO_S stVpssCropInfo;

        // stVpssCropInfo.bEnable = HI_TRUE;
        // stVpssCropInfo.enCropCoordinate = VPSS_CROP_ABS_COOR;
        // stVpssCropInfo.stCropRect.s32X = 50;
        // stVpssCropInfo.stCropRect.s32Y = 50;
        // stVpssCropInfo.stCropRect.u32Width = stSize.u32Width - 100;
        // stVpssCropInfo.stCropRect.u32Height = stSize.u32Height - 100;
        // s32Ret = HI_MPI_VPSS_SetGrpCrop(VpssGrp, &stVpssCropInfo);

        /*vi bind vpss*/
        s32Ret = SAMPLE_COMM_VI_Bind_VPSS(ViPipe, ViChn, VpssGrp);
        if (HI_SUCCESS != s32Ret)
        {
            sprintf(logtmp, "vi bind vpss failed. s32Ret: 0x%x !\n", s32Ret);
            printlog(logtmp);
            goto EXIT2;
        }

        char dirname[256];
        int dir_num = 1;
        for (dir_num = 1; dir_num < 1000000; dir_num++)
        {
            sprintf(dirname, "/sd/data/%d", dir_num);
            if (NULL == opendir(dirname))
            {
                mkdir(dirname, 0775);
                break;
            }
        }

        logfileinit(dir_num);
        if (NULL == opendir(dirname))
        {
            printlog("Open sd card error!\n");
        }
        else
        {
            for (int led_num = 0; led_num < 5; led_num++)
            {
                led_control(1);
                usleep(100000);
                led_control(0);
                usleep(100000);
            }
        }

        fd = -1;
        int picnum = 0;


        Rtsp_Init(dir_num);

        fd = open("/dev/gpio_dev", 0);
        if (fd < 0)
        {
            printlog("Open button dev error!\n");
        }
        unsigned int key = 0;
        int ret;
        while (1)
        {
            // picflag = 1;
            // usleep(500000);
            ret = read(fd, &key, sizeof(key));
            if (ret < 0)
            {
                printlog("read gpio error\n");
            }
            else
            {
                // SAMPLE_PRT("key = %d\n", key);
                if (key == 1)
                {
                    vpss_one_frame(0, 0, dirname, picnum, 1);
                    picnum++;
                    usleep(300000);
                }
            }
        }
        //         struct timeval start, end;
        // double timeuse;

        // pthread_t pictask;

        // pthread_create(&pictask, 0, readgpiotask, NULL);

        // gettimeofday(&start, NULL);

        // while (1)
        // {
            
        //         usleep(100);
        //     // struct timeval start, end;
        //     // double timeuse;

        //     gettimeofday(&end, NULL);
        //     timeuse = (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_usec - start.tv_usec) / 1000.0;
        //     if (timeuse >= 1)
        //     {
        //         gettimeofday(&start, NULL);
        //         if (picflag)
        //         {
        //             vpss_one_frame(0, 0, dirname, picnum, 1);
        //             picnum++;
        //             picflag = 0;
        //         }
                
        //         //else
        //             //vpss_one_frame(0, 0, dirname, picnum, 0);
        //     }

        //     // gettimeofday(&end, NULL);
        //     // timeuse = (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_usec - start.tv_usec) / 1000.0;
        //     // printf("用时%fms\n", timeuse);

        //     // usleep(30000);
        // }

        close(fd);

        printf("Enter any key to enable 2Dim-Dis.\n");
        PAUSE();
        stDISAttr.bEnable = HI_TRUE;
        HI_MPI_ISP_SetDISAttr(ViPipe, &stDISAttr);

        printf("Enter any key to Disable 2Dim-Dis.\n");
        PAUSE();
        stDISAttr.bEnable = HI_FALSE;
        HI_MPI_ISP_SetDISAttr(ViPipe, &stDISAttr);
        PAUSE();
    EXIT2:
        SAMPLE_COMM_VPSS_Stop(VpssGrp, abChnEnable);
    EXIT1:
        SAMPLE_COMM_VI_StopVi(&stViConfig);
    EXIT:
        SAMPLE_COMM_SYS_Exit();
        return s32Ret;
    }


/******************************************************************************
 * function    : main()
 * Description : main
 ******************************************************************************/
#ifdef __HuaweiLite__
    int app_main(int argc, char *argv[])
#else
int main(int argc, char *argv[])
#endif
    {
        HI_S32 s32Ret = HI_FAILURE;
        HI_S32 s32Index;
        HI_U32 u32VoIntfType = 0;

#ifndef __HuaweiLite__
        signal(SIGINT, SAMPLE_VIO_HandleSig);
        signal(SIGTERM, SAMPLE_VIO_HandleSig);
#endif
        if (NULL == opendir("/sd/data"))
        {
            mkdir("/sd/data", 0775);
        }
        if (NULL == opendir("/sd/log"))
        {
            mkdir("/sd/log", 0775);
        }
        SetReg(0x120C0000, 0x1200);
        led_control(0);

        u32VoIntfType = 0;

        SAMPLE_VIO_ViOnlineVpssOffline_ISPDIS(u32VoIntfType);

        return 1;
    }

    void *yuv2jpgtask(void *p)
    {

        // struct timeval start, end;
        // int timeuse;

        // gettimeofday(&start, NULL);

        THREAD_DATA *info = (THREAD_DATA *)malloc(sizeof(THREAD_DATA));
        memcpy(info, p, sizeof(THREAD_DATA));
        free(p);

        int s32Ret = 0;

        sprintf(logtmp, "子线程_%d start\n", info->picnum);
        printlog(logtmp);

        char filename[128];
#ifndef YUV2JPEG
     // 保存yuv
        FILE *outfile_jpg;

        // test处理前yuv
        sprintf(filename, "%s/%d.yuv", info->pwd, info->picnum);
        
        if ((outfile_jpg = fopen(filename, "wb+")) == NULL) // 保存白光的jpg照片;
        {
            SAMPLE_PRT("can't open %s .\n", filename);
        }
        // 写Y
        s32Ret = fwrite(info->Y, 1024 * 1280, 1, outfile_jpg);
        if (s32Ret == -1)
        {
            SAMPLE_PRT("Erro: write read data. s32Ret: 0x%x \n", s32Ret);
        }
        // 写UV
        s32Ret = fwrite(info->UV, 1024 * 640, 1, outfile_jpg);
        if (s32Ret == -1)
        {
            SAMPLE_PRT("Erro: write read data. s32Ret: 0x%x \n", s32Ret);
        }
        fflush(outfile_jpg);
        fclose(outfile_jpg);

#else
        char *rgb = (char *)malloc(sizeof(char) * 1280 * 1024 * 3);
        YUV420_TO_RGB24(info->Y, info->UV, 1280, 1024, rgb);

        sprintf(filename, "%s/%d.jpg", info->pwd, info->picnum);

        s32Ret = rgb2jpg(filename, rgb, 1280, 1024);
        free(rgb);

#endif

        sprintf(logtmp, "子线程_%d stop savejpg_s32Ret = %d\n", info->picnum, s32Ret);

        printlog(logtmp);
    
        free(info);
        if (s32Ret != -1)
        {
            led_control(1);
            usleep(100000);
            led_control(0);
        }

        // gettimeofday(&end, NULL);
        // timeuse = (end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec) / 1000;
        // printf("子线程_%d stop timeuse: %d ms\n", info->picnum, timeuse);

        pthread_exit(NULL);
    }

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */
