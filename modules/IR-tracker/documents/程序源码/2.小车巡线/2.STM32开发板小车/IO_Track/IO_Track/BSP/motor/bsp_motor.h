#ifndef __BSP_MOTOR_H
#define __BSP_MOTOR_H

#include "AllHeader.h"

#define MOTOR_IGNORE_PULSE  (2000) //2000
#define MOTOR_MAX_PULSE     (3600)
#define MOTOR_FREQ_DIVIDE   (0)

//TIM8 4个正通道
#define Motor1_A_Port GPIOC
#define Motor1_A_Pin 	GPIO_Pin_6
#define Motor1_A_Clk  RCC_APB2Periph_GPIOC
#define Motor1_B_Port GPIOC
#define Motor1_B_Pin 	GPIO_Pin_7
#define Motor1_B_Clk	RCC_APB2Periph_GPIOC

#define Motor2_A_Port GPIOC
#define Motor2_A_Pin 	GPIO_Pin_8
#define Motor2_A_Clk 	RCC_APB2Periph_GPIOC
#define Motor2_B_Port	GPIOC
#define Motor2_B_Pin 	GPIO_Pin_9
#define Motor2_B_Clk	RCC_APB2Periph_GPIOC

//TIM1  4个正通道
#define Motor3_A_Port GPIOE
#define Motor3_A_Pin  GPIO_Pin_9
#define Motor3_A_Clk  RCC_APB2Periph_GPIOE
#define Motor3_B_Port	GPIOE
#define Motor3_B_Pin  GPIO_Pin_11
#define Motor3_B_Clk  RCC_APB2Periph_GPIOE

#define Motor4_A_Port GPIOE
#define Motor4_A_Pin  GPIO_Pin_13
#define Motor4_A_Clk	RCC_APB2Periph_GPIOE
#define Motor4_B_Port GPIOE
#define Motor4_B_Pin  GPIO_Pin_14
#define Motor4_B_Clk	RCC_APB2Periph_GPIOE

#define PWM_M1_A  TIM8->CCR1
#define PWM_M1_B  TIM8->CCR2

#define PWM_M2_A  TIM8->CCR3
#define PWM_M2_B  TIM8->CCR4

#define PWM_M3_A  TIM1->CCR1
#define PWM_M3_B  TIM1->CCR2

#define PWM_M4_A  TIM1->CCR3
#define PWM_M4_B  TIM1->CCR4

// MOTOR: M1 M2 M3 M4
// MOTOR: L1 L2 R1 R2
typedef enum
{
	Motor_M1 = 0,
	Motor_M2 ,
	Motor_M3 ,
	Motor_M4 ,
	Motor_MAX
}Motor_ID;



void motor_gpio_init(void);
void motor_pwm_init(uint16_t arr, uint16_t psc);
void Motor_Set_Pwm(uint8_t id, int16_t speed);
void Motor_Stop(uint8_t brake);

#endif
