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
            if (recLen > 0)
            {
                data[recLen] = '\0';
                cout << portName << " - Count: " << ++countRead << ", Length: " 
                << recLen << ", Str: " 
                << data << ", Hex: " 
                << char2hexstr(data, recLen).c_str() << endl;
            }
            delete[] data;
            data = NULL;
        }
    } 
}
/***********************************************************/







/***********************************************************/