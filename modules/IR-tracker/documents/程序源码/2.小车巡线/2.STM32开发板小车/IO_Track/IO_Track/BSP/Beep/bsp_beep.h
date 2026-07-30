#ifndef __BSP_BEEP_H
#define __BSP_BEEP_H

#include "AllHeader.h"

#define BEEP_RCC   RCC_APB2Periph_GPIOG
#define BEEP_PORT  GPIOG
#define BEEP_PIN   GPIO_Pin_12

#define BEEP_ON  GPIO_SetBits(BEEP_PORT,BEEP_PIN)
#define BEEP_OFF  GPIO_ResetBits(BEEP_PORT,BEEP_PIN)

#define BEEP_BEEP PGout(12) 


void init_beep(void);
void beep_timer(void);
void open_beep(void);
void set_beep_time(u8 times,u16 rates);

#endif
