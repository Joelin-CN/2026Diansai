#include "bsp_LED.h"

void init_led_gpio(void)
{
		
		GPIO_InitTypeDef GPIO_InitStructure;
	
		//初始化LED1_blue的引脚
		RCC_APB2PeriphClockCmd(LED_Blue_RCC, ENABLE); 									   
  	GPIO_InitStructure.GPIO_Pin = LED_Blue_PIN;	
  	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;   
  	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz; 
  	GPIO_Init(LED_Blue_PORT, &GPIO_InitStructure);
	
		//初始化LED2_red的引脚
		RCC_APB2PeriphClockCmd(LED_Red_RCC, ENABLE); 									   
  	GPIO_InitStructure.GPIO_Pin = LED_Red_PIN;	
  	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;   
  	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz; 
  	GPIO_Init(LED_Red_PORT, &GPIO_InitStructure);
	
		LED_Blue = 0;
		LED_Red = 0;

}

void init_gpio(void)
{
		
		GPIO_InitTypeDef GPIO_InitStructure;
	
		RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE); 									   
  	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;	
  	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;   
  	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz; 
  	GPIO_Init(GPIOB, &GPIO_InitStructure);
	
		x3pro = 1;
}


