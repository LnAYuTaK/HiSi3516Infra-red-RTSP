
/**
 * @file App.cpp
 * @author LnAYuTaK (807874484@qq.com)
 * @brief 
 * @version 0.1
 * @date 2023-03-04
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#include "App.h"
#include <sys/types.h>
#include <dirent.h>
#include "Control.h"
#include "sample_comm.h"
#include "MPP_SYS.h"
#include "MPP_VI.h"
#include "MPP_VPSS.h"
#include "MPP_VENC.h"
#include "CLOG.h"
#include "RtspManager.h"
#include "MavLinkHandle.h"
#define DataDir "/sd/data"
#define LogDir  "/sd/log"

#define UART1   "/dev/USB0"
// #define  H264_SAVE_ENBLE
// #define  JEPG_SAVE_ENBEL

//后边再优化
static char dirname[256];
static int fd = -1;

static void SetCurrentDir()
{
   if (NULL == opendir(DataDir))
   {
      mkdir(DataDir, 0775);
   }
   if (NULL == opendir(LogDir))
   {
      mkdir(LogDir, 0775);
   }
   LOG_INFO("%s","SYS INIT DIR");
}
/***********************************************************/
Application  *Application::_app = nullptr; 
Application::Application(/* args */)
 :log(nullptr)
 ,led(nullptr)
 ,MppSys(nullptr)
 ,MppVi(nullptr)
 ,MppVpss(nullptr)
 ,MppVenc(nullptr)
 ,_serial(nullptr)
 ,_mavlinkhandle(nullptr)
{
   log            = new CLOG(true,true,"/sd/log/log.txt");
   led            = new LedControl();
   MppSys         = new MPP_SYS();
   MppVi          = new MPP_VI();
   MppVpss        = new MPP_VPSS();
   MppVenc        = new MPP_VENC();
   //注意顺序
   _serial        = new CSerialPort();
   _mavlinkhandle = new MavLinkHandle(_serial);
   _app    = this;
}
/***********************************************************/
Application* app(void)
{
  return Application::_app;
}
/***********************************************************/
Application::~Application()
{
   _app =nullptr;
}
/***********************************************************/
void Application::Init(/* args */)
{
   //初始化目标文件夹
   SetCurrentDir();
   
   IOControl::SetReg(0x120C0000, 0x1200); 
   //Serial Init初始化
   _serial->init("/dev/ttyAMA2",
                itas109::BaudRate9600, // baudrate
                itas109::ParityNone,   // parity
                itas109::DataBits8,    // data bit
                itas109::StopOne,      // stop bit
                itas109::FlowNone,     // flow
                4096  
   );
   _serial->setReadIntervalTimeout(0); 
   //Link Mavlink Control 交付给mavlink协议处理
   _serial->connectReadEvent(_mavlinkhandle);
   //VB   Config//
   VB_CONFIG_S stVbConf;
   hi_memset(&stVbConf, sizeof(VB_CONFIG_S), 0, sizeof(VB_CONFIG_S));

   stVbConf.u32MaxPoolCnt = 2;
   stVbConf.astCommPool[0].u64BlkSize = COMMON_GetPicBufferSize(1280,1024,
                                                               SAMPLE_PIXEL_FORMAT, 
                                                               DATA_BITWIDTH_8, 
                                                               COMPRESS_MODE_NONE, 
                                                               DEFAULT_ALIGN);
   stVbConf.astCommPool[0].u32BlkCnt = 16;

   //SYS Init
   if(HI_SUCCESS != (MppSys->Init(&stVbConf)))
   {
      LOG_ERROR("%s","ERROR INIT MPP SYSTEM");
      return;
   }
   //VI Config
   if(HI_SUCCESS != (MppVi->Init(NULL,true)))
   {
      LOG_ERROR("%s","ERROR INIT VI");
      return;
   }
   //VPSS Config
   if(HI_SUCCESS != (MppVpss->Init(0,NULL,NULL,NULL,true)))
   {
      LOG_ERROR("%s","ERROR INIT VPSS");
      return;
   }
   //VI Bind VPSS
   if(HI_SUCCESS != SAMPLE_COMM_VI_Bind_VPSS(0, 0, 0))
   {
      LOG_ERROR("%s","ERROR VI Bind VPSS");
      SAMPLE_COMM_SYS_Exit();
      return;
   }
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
   if (NULL == opendir(dirname))
   {
      LOG_ERROR("%s","open dir error");
   }
   else
   {
      for (int led_num = 0; led_num < 5; led_num++)
      {
         this->led->SetLED(LED_ON);
         usleep(100000);
         this->led->SetLED(LED_OFF);
         usleep(100000);
      }
   }
   int fd = -1;
   fd = open("/dev/gpio_dev",0);
   if (fd < 0)
   {
      LOG_ERROR("%s","ERRPR Open Button DEV ERROR");
   }
   //视频RTSP 传输初始化  TUDO 需要解耦合
   //RTSP INIT
   Rtsp_Init(dir_num);
   LOG_INFO("%s","INIT RTSP OK");
   //视频保存VENC  初始化
   VENC_CHN        StreamSaveVencChn = 2;
   VENC_GOP_ATTR_S stGopAttr;
   stGopAttr.enGopMode = VENC_GOPMODE_SMARTP;
   stGopAttr.stSmartP.s32BgQpDelta = 7;
   stGopAttr.stSmartP.s32ViQpDelta = 2;
   stGopAttr.stSmartP.u32BgInterval = 1200;
   if(HI_SUCCESS != (MppVenc->H264SaveInit(StreamSaveVencChn,PIC_720P,&stGopAttr)))
   {
      LOG_ERROR("%s","ERROR VENC H264SAVE INIT");
   }
	if(HI_SUCCESS != (SAMPLE_COMM_VPSS_Bind_VENC(0, VPSS_CHN0, StreamSaveVencChn))) 
   {
		LOG_ERROR("%s","ERROR VPSS BIND H264Save VENC");
	}
   //拍照VENC初始化//
   VENC_CHN    JPGVencChn = 1;
   SIZE_S pstSize;
   pstSize.u32Width = 1280;
   pstSize.u32Height= 1024;
   if(HI_SUCCESS != (MppVenc->JEPGSnapInit(JPGVencChn,&pstSize)))
   {
      LOG_ERROR("%s","ERROR VENC JEPGSnap INIT");
   }
   if(HI_SUCCESS != (SAMPLE_COMM_VPSS_Bind_VENC(0, VPSS_CHN0, JPGVencChn)))
   {
      LOG_ERROR("%s","ERROR VPSS BIND JEPGSnap VENC");     
   }
}
/***********************************************************/
void Application::Start()
{
//开启mavlink 串口接收线程 检测串口数据执行操作
   _serial->open();
   if(!(_serial->isOpen()))
   {
      LOG_ERROR("%s","Serial OPEN ERROR");
      return;
   }
   else
   {
      LOG_INFO("%s [%s]","OPEN SERIAL OK",_serial->getPortName());
   }
#ifdef   H264_SAVE_ENBLE
    //开启视频保存功能
    pthread_t  H264SaveTask;
    VENC_THREAD_PARA_T H264SavePara;
    H264SavePara.VeChn = 2;
    H264SavePara.stVencChnAttr.stVencAttr.enType = PT_H264;
    H264SavePara.s32Cnt = 1;
    H264SavePara.enSize = PIC_720P;
    memcpy(H264SavePara.filepath,dirname,256);
    //视频保存线程//
    pthread_create (&H264SaveTask,0,User_VENC_GetStream_Save,(HI_VOID*)&H264SavePara);
#endif

#ifdef   JEPG_SAVE_ENBEL
    //开启拍照线程
    pthread_t  JEPGFrameTask;
    VENC_THREAD_JPEG_PARA_T JEPGFramePara;
    JEPGFramePara.VencChn    = 1;
    JEPGFramePara.SnapCnt  = 1 ;//每次拍一张
    JEPGFramePara.bSaveJpg = HI_TRUE;//拍完保存
    memcpy(JEPGFramePara.filepath,dirname,256);
    unsigned int key = 0;
    for (;;)
    {
        // if(read(fd, &key, sizeof(key)>0))
        // {
            pthread_create(&JEPGFrameTask,0,User_VENC_GetJPG_Save,(HI_VOID*)&JEPGFramePara);
            pthread_join(JEPGFrameTask, NULL);
            usleep(2000000);
        // }
    }
#endif
   close(fd);
   LOG_INFO("%s","Enter any key to enable 2Dim-Dis");
   PAUSE();
   ISP_DIS_ATTR_S stDISAttr;
   stDISAttr.bEnable = HI_TRUE;
   HI_MPI_ISP_SetDISAttr(0, &stDISAttr);
   LOG_INFO("%s","Enter any key to Disable 2Dim-Dis");
   PAUSE();
   stDISAttr.bEnable = HI_FALSE;
   HI_MPI_ISP_SetDISAttr(0, &stDISAttr);
   PAUSE();

}
/***********************************************************/








