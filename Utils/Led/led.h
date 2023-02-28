
#ifndef MY_LED_H
#define MY_LED_H

#include <stdio.h> 
#include <string.h> 
#include "hi_type.h"
//#include "mpi_sys.h"
HI_S32 SetReg(HI_U32 u32Addr, HI_U32 u32Value);
int gpio_test_in(unsigned int gpio_chip_num, unsigned int gpio_offset_num);
int gpio_test_out(unsigned int gpio_chip_num, unsigned int gpio_offset_num,  unsigned int gpio_out_val);
int led_control(int status);

#endif // MY_LED