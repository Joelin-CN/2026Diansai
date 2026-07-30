#ifndef __DELAY_H
#define __DELAY_H

#include "stm32f10x.h"
#include "system_stm32f10x.h"
#include "AllHeader.h"

void delay_init(void);
void delay_ms(uint16_t nms);
void delay_us(uint32_t nus);


void delay_ms_mpu(u32 n);
void delay_us_mpu(u32 n);

void get_ms(unsigned long *time);

#endif

