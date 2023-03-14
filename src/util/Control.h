/**
 * @file DirverControl.h
 * @author LnAYuTaK (807874484@qq.com)
 * @brief 外设控制模块
 * @version 0.1
 * @date 2023-03-03
 * @copyright Copyright (c) 2023
 * 
 */
//Base 
#include <stdio.h>
#include <string.h> 

class IOControl
{
private:

public:
    IOControl(){;}
     ~IOControl(){;}
    static int  SetReg(int u32Addr, int u32Value);
    bool SetGPIOIn(unsigned int gpioChipNumS,unsigned int gpioOffsetNum);
    bool SetGPIOOut(unsigned int gpioChipNum, 
                        unsigned int gpioOffsetNum,
                        unsigned int gpioOutVal);
};
enum LedState{
    LED_ON   = 0,
    LED_OFF  = 1
};

class LedControl  :public IOControl
{
private:

public:
    void SetLED(LedState state);
    LedControl();
    ~LedControl();
};






