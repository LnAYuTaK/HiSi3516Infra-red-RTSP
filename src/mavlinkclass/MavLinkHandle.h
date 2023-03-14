/**
 * @file MavLink.h
 * @author LnAYuTaK (807874484@qq.com)
 * @brief  mavlink 消息处理类
 * @version 0.1
 * @date 2023-03-08
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#pragma once 
#include "../thirdparty/mavlink/common/mavlink.h"
#include "SerialPort.h"
#include "SerialPortInfo.h"
using namespace itas109;
using namespace std;
class MavLinkHandle : public  CSerialPortListener
{
public:    
    MavLinkHandle(CSerialPort *pserial)
        : serial(pserial){};
    void onReadEvent(const char *portName, unsigned int readBufferLen);
    void HanleInfraredZoom(uint16_t ChnRaw);
private:
    CSerialPort *serial;   
};

