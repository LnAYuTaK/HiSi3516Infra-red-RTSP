#include "sample_vio.h"
 #include <sys/types.h>
 #include <dirent.h>
#include "Utils/Led/led.h"
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
//Check File Path 
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
//APP Start 
        u32VoIntfType = 0;
        SAMPLE_VIO_ViOnlineVpssOffline_ISPDIS(u32VoIntfType);
        return 1;
}

