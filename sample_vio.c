#ifdef __cplusplus
#if __cplusplus
extern "C"
{
#endif
#endif /* End of #ifdef __cplusplus */

#include "sample_vio.h"
#include "./Common/user_comm.h"
#define mem_size 20 * 1024 // 20k
static char logtmp[1024];
// static int m_pseudoColor_index = 5;             // 伪彩号
// unsigned char *pMem_pseudo_color = NULL; // 存储伪彩色板
static IRAY_VPSS_PICTURE vpss_picture;
static VIDEO_FRAME_INFO_S vpss_stFrame; // vpss frame
int picflag = 0;
void *yuv2jpgtask(void *p);
    int strlen_xun(char arr[]) // 循环方法
    {
        int i = 0;
        while (arr[i] != '\0')
        {
            ++i;
        }
        return i;
    }

    /*yuv422   group 组号chn  通道号 pwd  地址图像格式最好在vpss转换成yuv422sp，
    其他得格式按照实际进行保存 YYYYYYYY UVUVUVUV
    增加isTakePic开关，如果要拍照保存就置1，完成伪彩+拍照功能*/ 
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
    HI_CHAR aszFileName[64];
#if 1
        if (isTakePic == 1)
        {
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
    /* ------Config Vi ---- 
       ------Start  Vi------
       ------Config Vpss------
       ----- Start  Vpss ----
       ------Bind   Vpss---- 
    */ 
    HI_S32 APP(HI_U32 u32VoIntfType)
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

        //开启VI 
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
        //VPSS 初始化
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
//RTSP  注意必须要开启RTSP才可以拍照 //需要解耦
    Rtsp_Init(dir_num);
    fd = open("/dev/gpio_dev", 0);
    if (fd < 0)
    {
        printlog("Open button dev error!\n");
    }
//视频保存功能初始化
#ifdef  H264_SAVE_ENBLE
    VENC_GOP_ATTR_S stGopAttr;
    //VENC 通道2 
    VENC_CHN    StreamSaveVencChn = 2;
    stGopAttr.enGopMode = VENC_GOPMODE_SMARTP;
	stGopAttr.stSmartP.s32BgQpDelta = 7;
	stGopAttr.stSmartP.s32ViQpDelta = 2;
	stGopAttr.stSmartP.u32BgInterval = 1200;
	s32Ret = SAMPLE_COMM_VENC_Start(StreamSaveVencChn,PT_H264, PIC_720P, SAMPLE_RC_CBR, 0, HI_FALSE, &stGopAttr);
	if (HI_SUCCESS != s32Ret)
	{
		SAMPLE_PRT("Error: start venc H264 failed. s32Ret: 0x%x !\n", s32Ret);
	}
	//Mp4Bind
	s32Ret = SAMPLE_COMM_VPSS_Bind_VENC(VpssGrp, VpssChn, StreamSaveVencChn); 
	if (HI_SUCCESS != s32Ret)
	{
		SAMPLE_PRT("Error: Venc bind H264 Vpss failed. s32Ret: 0x%x !n", s32Ret);
	}
#endif

#ifdef  H264_SAVE_ENBLE
    //开启视频保存功能
    pthread_t  H264SaveTask;
    VENC_THREAD_PARA_T H264SavePara;
    H264SavePara.VeChn = StreamSaveVencChn;
    H264SavePara.stVencChnAttr.stVencAttr.enType = PT_H264;
    H264SavePara.s32Cnt = 1;
    H264SavePara.enSize = PIC_720P;
    memcpy(H264SavePara.filepath,dirname,256);
    //视频保存线程//
    int result= pthread_create (&H264SaveTask,0,User_VENC_GetStream_Save,(HI_VOID*)&H264SavePara);
#endif

//拍照功能初始化
#ifdef   JPG_SAVE_ENBEL
    //VENC 通道1
    VENC_CHN    JPGVencChn = 1;
    SIZE_S pstSize;
    pstSize.u32Width = 1280;
    pstSize.u32Height= 1024;
    s32Ret = SAMPLE_COMM_VENC_SnapStart(JPGVencChn,&pstSize,HI_FALSE);
    if (HI_SUCCESS != s32Ret)
	{
		SAMPLE_PRT("Error: start venc JPG failed. s32Ret: 0x%x !\n", s32Ret);
	}
    s32Ret = SAMPLE_COMM_VPSS_Bind_VENC(VpssGrp, VpssChn, JPGVencChn); 
	if (HI_SUCCESS != s32Ret)
	{
		SAMPLE_PRT("Error: Venc JPG bind Vpss failed. s32Ret: 0x%x !n", s32Ret);
	}
#endif

//拍照
#ifdef   JPG_SAVE_ENBEL
    pthread_t  JEPGFrameTask;
    VENC_THREAD_JPEG_PARA_T JEPGFramePara;
    JEPGFramePara.VencChn    = JPGVencChn;
    JEPGFramePara.SnapCnt  = 1 ;//每次拍一张
    JEPGFramePara.bSaveJpg = HI_TRUE;//拍完保存
    memcpy(JEPGFramePara.filepath,dirname,256);
    // JEPGFramePara.filepath = dirname;
    unsigned int key = 0;
    int PicNum  = 0 ; 
    for (;;)
    {
        // if(read(fd, &key, sizeof(key)>0))
        // {
            //拍照 
            pthread_create(&JEPGFrameTask,0,User_VENC_GetJPG_Save,(HI_VOID*)&JEPGFramePara);
            pthread_join(JEPGFrameTask, NULL);
            PicNum ++;
            usleep(500000);
        // }
        // else
        // {
        //     printf("read gpio error\n");
        // }
    }
#endif
        // unsigned int key = 0;
        // int ret;
        // while (1)
        // {
        //     // picflag = 1;
        //     // usleep(500000);
        //     ret = read(fd, &key, sizeof(key));
        //     if (ret < 0)
        //     {
        //         printlog("read gpio error\n");
        //     }
        //     else
        //     {
        //         SAMPLE_PRT("key = %d\n", key);
        //         if (key == 1)
        //         {
        //  
        //             // vpss_one_frame(0, 0, dirname, picnum, 1);
        //             pthread_create (&JPGSaveTask,0,User_VENC_GetJPG_Save,(HI_VOID*)0);
        //             // picnum++;
        //             usleep(300000);
        //         }
        //     }
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
