#ifndef __BSP_LED_H
#define __BSP_LED_H

#include "AllHeader.h"

#define LED_Red_RCC   RCC_APB2Periph_GPIOG
#define LED_Red_PORT  GPIOG
#define LED_Red_PIN   GPIO_Pin_15
#define LED_Red_ON  GPIO_SetBits(BEEP_PORT,BEEP_PIN) 
#define LED_Red_OFF  GPIO_ResetBits(BEEP_PORT,BEEP_PIN)


#define LED_Blue_RCC   RCC_APB2Periph_GPIOG
#define LED_Blue_PORT  GPIOG
#define LED_Blue_PIN   GPIO_Pin_14
#define LED_Blue_ON  	 GPIO_SetBits(BEEP_PORT,BEEP_PIN) 
#define LED_Blue_OFF   GPIO_ResetBits(BEEP_PORT,BEEP_PIN)


#define LED_Blue  PGout(14) 
#define LED_Red 	PGout(15) 


#define x3pro		PBout(9) 

void init_led_gpio(void);

void init_gpio(void);
#endif
