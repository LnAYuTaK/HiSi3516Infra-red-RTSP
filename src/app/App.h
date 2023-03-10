#pragma once 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <semaphore.h>
#include <pthread.h>
#include <math.h>
#include <time.h>
#include "MavLinkHandle.h"
class MavLinkHandle;
class LedControl;
class MPP_SYS;
class MPP_VI;
class MPP_VPSS;
class MPP_VENC;
class CLOG;

// #define  H264_SAVE_ENBLE
#define  JEPG_SAVE_ENBEL
using namespace std;
using namespace itas109;
class Application
{
public:
    Application();
    ~Application();

    void Init();
    void Start();

    static Application* _app;
    
    LedControl *    LED()           {return this->led;}
    CLOG  *         LOG()           {return this->log;}
    MPP_SYS *       MPPSYS()        {return this->MppSys;}
    MPP_VI *        MPPVI()         {return this->MppVi;}
    MPP_VPSS *      MPPVPSS()       {return this->MppVpss;}
    MPP_VENC *      MPPVENC()       {return this->MppVenc;}
    CSerialPort *   Serial()        {return this->_serial;} 
    MavLinkHandle * MavlinkHandle() {return this->_mavlinkhandle;} 

private:
    CLOG *        log;
    LedControl *  led;
    MPP_SYS *     MppSys;
    MPP_VI *      MppVi;
    MPP_VPSS *    MppVpss;
    MPP_VENC *    MppVenc;
    CSerialPort * _serial;
    MavLinkHandle *_mavlinkhandle;
};
Application * app();











