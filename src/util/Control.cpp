/**
 * @file Control.cpp
 * @author LnAYuTaK (807874484@qq.com)
 * @brief 
 * @version 0.1
 * @date 2023-03-03
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#include "Control.h"
#include "CLOG.h"
bool IOControl::SetGPIOIn(unsigned int gpioChipNumS ,unsigned int gpioOffsetNum)
{
    FILE *fp;
    char fileName[50];
    unsigned char buf[10];
    unsigned int gpioChipNum;
    gpioChipNum = gpioChipNumS * 8 + gpioOffsetNum;
    sprintf(fileName, "/sys/class/gpio/export");
    fp = fopen(fileName, "w");
    if (fp == NULL)
    {
        LOG_ERROR("Cannot open %s.\n", fileName);
        return false;
    }
    fprintf(fp, "%d", gpioChipNum);
    fclose(fp);
    sprintf(fileName, "/sys/class/gpio/gpio%d/direction", gpioChipNum);
    fp = fopen(fileName, "rb+");
    if (fp == NULL)
    {
        LOG_ERROR("Cannot open %s.\n", fileName);
        return false;
    }
    fprintf(fp, "in");
    fclose(fp);
    sprintf(fileName, "/sys/class/gpio/gpio%d/value", gpioChipNum);
    fp = fopen(fileName, "rb+");
    if (fp == NULL)
    {
        LOG_ERROR("Cannot open %s.\n", fileName);
        return false;
    }
    memset(buf, 0, 10);
    fread(buf, sizeof(char), sizeof(buf) - 1, fp);
    fclose(fp);
    sprintf(fileName, "/sys/class/gpio/unexport");
    fp = fopen(fileName, "w");
    if (fp == NULL)
    {
        LOG_ERROR("Cannot open %s.\n", fileName);
        return false;
    }
    fprintf(fp, "%d", gpioChipNum);
    fclose(fp);
    return true;
}
/***********************************************************/
bool  IOControl::SetGPIOOut(unsigned int gpioChipNum, 
                            unsigned int gpioOffsetNum,
                            unsigned int gpioOutVal)
{
     FILE *fp;
    char file_name[50];
    char buf[10];
    unsigned int gpio_num;
    gpio_num = gpioChipNum * 8 + gpioOffsetNum;
    sprintf(file_name, "/sys/class/gpio/export");
    fp = fopen(file_name, "w");
    if (fp == NULL)
    {
        LOG_ERROR("Cannot open %s.\n", file_name);
        return false;
    }
    fprintf(fp, "%d", gpio_num);
    fclose(fp);
    sprintf(file_name, "/sys/class/gpio/gpio%d/direction", gpio_num);
    fp = fopen(file_name, "rb+");
    if (fp == NULL)
    {
        LOG_ERROR("Cannot open %s.\n", file_name);
        return false;
    }
    fprintf(fp, "out");
    fclose(fp);
    sprintf(file_name, "/sys/class/gpio/gpio%d/value", gpio_num);
    fp = fopen(file_name, "rb+");
    if (fp == NULL)
    {
        LOG_ERROR("Cannot open %s.\n", file_name);
        return false;
    }
    if (gpioOutVal)
        strcpy(buf, "1");
    else
        strcpy(buf, "0");
    fwrite(buf, sizeof(char), sizeof(buf) - 1, fp);
    // LOG_INFO("%s: gpio%d_%d = %s\n", __func__,
    //        gpioChipNum, gpioOffsetNum, buf);
    fclose(fp);
    sprintf(file_name, "/sys/class/gpio/unexport");
    fp = fopen(file_name, "w");
    if (fp == NULL)
    {
        LOG_ERROR("Cannot open %s.\n", file_name);
        return false;
    }
    fprintf(fp, "%d", gpio_num);
    fclose(fp);
    return true;
}
int IOControl::SetReg(int u32Addr, int u32Value)
{
    char cmdstring[1024];
    memset(cmdstring, '\0', sizeof(cmdstring)); // 初�?�化buf,以免后面写�?�乱码到文件�?
    sprintf(cmdstring, "devmem %d 32 %d", u32Addr, u32Value);
    int flag = system(cmdstring);
    return flag;
}
/***********************************************************/
LedControl::LedControl()
{
//    SetLED(LED_ON);
}
/***********************************************************/
LedControl::~LedControl()
{

}
/***********************************************************/
void LedControl::SetLED(LedState state)
{
    if(state == LED_ON)
    {
        SetGPIOOut(1, 0, 0);
    }
    else if(state == LED_OFF)
    {
        SetGPIOOut(1, 0, 1);
    }
}





