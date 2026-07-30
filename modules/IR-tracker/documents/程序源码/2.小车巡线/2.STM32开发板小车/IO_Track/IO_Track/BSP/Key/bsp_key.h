#ifndef __BSP_KEY_H__
#define __BSP_KEY_H__

#include "AllHeader.h"

//  引脚定义
#define KEY1_GPIO_PORT GPIOG
#define KEY1_GPIO_PIN  GPIO_Pin_3
#define KEY1_GPIO_CLK  RCC_APB2Periph_GPIOG

#define KEY2_GPIO_PORT GPIOG
#define KEY2_GPIO_PIN  GPIO_Pin_4
#define KEY2_GPIO_CLK  RCC_APB2Periph_GPIOG

#define KEY3_GPIO_PORT GPIOG
#define KEY3_GPIO_PIN  GPIO_Pin_5
#define KEY3_GPIO_CLK  RCC_APB2Periph_GPIOG

// 按键状态，与实际电平相反。
#define KEY_PRESS      1
#define KEY_RELEASE    0

#define KEY_MODE_ONE_TIME   1
#define KEY_MODE_ALWAYS     0


uint8_t Key_Scan(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin);

void Key1_GPIO_Init(void);
void Key2_GPIO_Init(void);
void Key3_GPIO_Init(void);
void KEYAll_GPIO_Init(void);

uint8_t Key1_State(uint8_t mode);
uint8_t Key1_Long_Press(uint16_t timeout);

uint8_t Key2_State(uint8_t mode);
uint8_t Key2_Long_Press(uint16_t timeout);

uint8_t Key3_State(uint8_t mode);
uint8_t Key3_Long_Press(uint16_t timeout);



#endif /* __BSP_KEY_H__ */
