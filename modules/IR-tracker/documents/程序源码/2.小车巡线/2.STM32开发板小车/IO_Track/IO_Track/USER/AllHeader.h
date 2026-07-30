#ifndef __AllHeader_H
#define __AllHeader_H

#define bool _Bool
#define true 1
#define false 0

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <Time.h>

//基本头文件
#include "stm32f10x.h"
#include "switch_function.h"
#include "delay.h"
#include "bsp.h"
#include "bsp_timer.h"

//串口部分
#include "bsp_usart.h"

//按键部分
#include "bsp_key.h"

//电机部分
#include "bsp_motor.h"
#include "bsp_encoder.h"
#include "PID_Motor.h"
#include "app_motor.h"



//板载led部分
#include "bsp_LED.h"


//红外巡部分
#include "bsp_irtracking.h"
#include "app_irtracking.h"
#include "app_usart2.h"



//其它变量
extern u8 g_board_no_error;
extern u8 g_new_package_flag; //接收到新的一包数据标志
extern u8 ruijiao_flag;
#endif

