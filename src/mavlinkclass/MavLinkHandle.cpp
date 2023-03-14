/**
 * @file MavLink.cpp
 * @author LnAYuTaK (807874484@qq.com)
 * @brief 处理接收mavlink协议消息交付给对应组件执行操作
 * @version 0.1
 * @date 2023-03-08
 * @copyright Copyright (c) 2023
 * 
 */
#include "App.h"
#include "MavLinkHandle.h"
#include "MPP_VPSS.h"
#include <iostream>
static int countRead = 0;
static string char2hexstr(const char *str, int len)
{
    static const char hexTable[17] = "0123456789ABCDEF";

    std::string result;
    for (int i = 0; i < len; ++i)
    {
        result += "0x";
        result += hexTable[(unsigned char)str[i] / 16];
        result += hexTable[(unsigned char)str[i] % 16];
        result += " ";
    }
    return result;
}
void MavLinkHandle::onReadEvent(const char *portName, 
                                unsigned int readBufferLen)
{

    if (readBufferLen > 0)
    {
        char *data = new char[readBufferLen + 1]; // '\0'
        if (data)
        {
            // read
            int recLen = serial->readData(data, readBufferLen);
            //  cout  << "Hex: "  << char2hexstr(data,readBufferLen + 1).c_str() << endl;
            if (recLen > 0)
            {
                // data[recLen] = '\0';
                uint8_t chan = MAVLINK_COMM_0;
                uint8_t* buffer =NULL;
                mavlink_message_t rmsg;
                mavlink_status_t status;
                mavlink_rc_channels_t rcChannelsRaw;
                for (int i = 0; i < recLen; i++)
                {
                    if (mavlink_parse_char(0, data[i], &rmsg, &status) == 1)
                    {
                        switch (rmsg.msgid)
                        {
                        case  MAVLINK_MSG_ID_RC_CHANNELS:
                            mavlink_msg_rc_channels_decode(&rmsg,&rcChannelsRaw);
                            printf("%hd\n",
                            rcChannelsRaw.chan9_raw);
                        //(2005 - 982) /4  取整   
                        std::cout << app()->MPPVPSS()->zoom() << std::endl;
                        if(rcChannelsRaw.chan9_raw<=1237)
                        {
                           app()->MPPVPSS()->SetVpssZoomLevel(0,1);
                           break;
                        }
                        if ((1237<rcChannelsRaw.chan9_raw)&&(rcChannelsRaw.chan9_raw<=1492))
                        {
                            app()->MPPVPSS()->SetVpssZoomLevel(0,2);
                            break;
                        }
                        if ((1492<rcChannelsRaw.chan9_raw )&&(rcChannelsRaw.chan9_raw <=1747))
                        {
                            app()->MPPVPSS()->SetVpssZoomLevel(0,4);
                            break;
                        }
                        if (1747<rcChannelsRaw.chan9_raw)
                        {
                            app()->MPPVPSS()->SetVpssZoomLevel(0,8);
                            break;
                        }
                        default:
                            break;
                        }
                    }
                }
            }
            delete[] data;
            data = NULL;
        }
    } 
}
/***********************************************************/


















/***********************************************************/