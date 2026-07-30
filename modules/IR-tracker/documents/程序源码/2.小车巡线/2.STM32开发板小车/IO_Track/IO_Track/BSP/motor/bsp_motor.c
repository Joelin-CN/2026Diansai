#include "bsp_motor.h"


static int16_t Motor_Ignore_Dead_Zone(int16_t pulse)
{
   
		if (pulse > 0) return pulse + MOTOR_IGNORE_PULSE;
		if (pulse < 0) return pulse - MOTOR_IGNORE_PULSE;

    return 0;
}

void motor_gpio_init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	
	gpio_t  motor_pwm[]=
	{
		{Motor1_A_Port,Motor1_A_Pin,Motor1_A_Clk},
		{Motor1_B_Port,Motor1_B_Pin,Motor1_B_Clk},
		
		{Motor2_A_Port,Motor2_A_Pin,Motor2_A_Clk},
		{Motor2_B_Port,Motor2_B_Pin,Motor2_B_Clk},
		
		{Motor3_A_Port,Motor3_A_Pin,Motor3_A_Clk},
		{Motor3_B_Port,Motor3_B_Pin,Motor3_B_Clk},
		
		{Motor4_A_Port,Motor4_A_Pin,Motor4_A_Clk},
		{Motor4_B_Port,Motor4_B_Pin,Motor4_B_Clk},
	};
	
	for(u8 i =0;i<Motor_MAX*2;i++)
	{
	 /* PWM输出引脚 */
		RCC_APB2PeriphClockCmd(motor_pwm[i].clock, ENABLE);
		GPIO_InitStructure.GPIO_Pin = motor_pwm[i].pin;
		GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP; //复用推挽输出
		GPIO_Init(motor_pwm[i].port, &GPIO_InitStructure);
	}
}


void motor_pwm_init(uint16_t arr, uint16_t psc)
{
	
		TIM_OCInitTypeDef       TIM_OCInitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM8 | RCC_APB2Periph_TIM1, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    GPIO_PinRemapConfig(GPIO_FullRemap_TIM1, ENABLE);//完全重映射

		 //重新将Timer设置为缺省值
    TIM_DeInit(TIM8);
    TIM_DeInit(TIM1);
    //预分频系数为0，即不进行预分频，此时TIMER的频率为72MHzre.TIM_Prescaler =0;
    TIM_TimeBaseStructure.TIM_Prescaler = psc;
    //设置计数溢出大小，每计xxx个数就产生一个更新事件
    TIM_TimeBaseStructure.TIM_Period = arr - 1;
    //设置时钟分割
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    //设置计数器模式为向上计数模式
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;

    //将配置应用到TIM8中
    TIM_TimeBaseInit(TIM8, &TIM_TimeBaseStructure);
    TIM_TimeBaseInit(TIM1, &TIM_TimeBaseStructure);
		
		//对timer的通道-设置缺省值
    TIM_OCStructInit(&TIM_OCInitStructure);
    
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;             //设置是PWM模式还是比较模式
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable; //比较输出使能，使能PWM输出到端口
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;     //设置极性是高还是低
    // TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_Low;	  //设置极性是高还是低
    //设置占空比，占空比=(CCRx/ARR)*100%或(TIM_Pulse/TIM_Period)*100%
    TIM_OCInitStructure.TIM_Pulse = 0;
    
		TIM_OC1Init(TIM8, &TIM_OCInitStructure);                      //TIM8的CHx输出
    TIM_OC2Init(TIM8, &TIM_OCInitStructure);                      //TIM8的CHx输出
    TIM_OC3Init(TIM8, &TIM_OCInitStructure);                      //TIM8的CHx输出
    TIM_OC4Init(TIM8, &TIM_OCInitStructure);                      //TIM8的CHx输出
		
    TIM_OC1Init(TIM1, &TIM_OCInitStructure);                      //TIM1的CHx输出
		TIM_OC2Init(TIM1, &TIM_OCInitStructure);                      //TIM1的CHx输出
		TIM_OC3Init(TIM1, &TIM_OCInitStructure);                      //TIM1的CHx输出
    TIM_OC4Init(TIM1, &TIM_OCInitStructure);                      //TIM1的CHx输出
		
		//设置PWM输出为使能
		TIM_CtrlPWMOutputs(TIM8, ENABLE);
		TIM_CtrlPWMOutputs(TIM1, ENABLE);
	  TIM_Cmd(TIM8, ENABLE);
    TIM_Cmd(TIM1, ENABLE);
}

// 所有电机停止
void Motor_Stop(uint8_t brake)
{
    if (brake != 0) brake = 1;
    TIM_SetCompare1(TIM8, brake * MOTOR_MAX_PULSE);
    TIM_SetCompare2(TIM8, brake * MOTOR_MAX_PULSE);
    TIM_SetCompare3(TIM8, brake * MOTOR_MAX_PULSE);
    TIM_SetCompare4(TIM8, brake * MOTOR_MAX_PULSE);

    TIM_SetCompare1(TIM1, brake * MOTOR_MAX_PULSE);
    TIM_SetCompare2(TIM1, brake * MOTOR_MAX_PULSE);
    TIM_SetCompare3(TIM1, brake * MOTOR_MAX_PULSE);
    TIM_SetCompare4(TIM1, brake * MOTOR_MAX_PULSE);
}

// 设置电机速度，speed:±3600, 0为停止
void Motor_Set_Pwm(uint8_t id, int16_t speed)
{
    int16_t pulse = Motor_Ignore_Dead_Zone(speed);
    // 限制输入
    if (pulse >= MOTOR_MAX_PULSE)
        pulse = MOTOR_MAX_PULSE;
    if (pulse <= -MOTOR_MAX_PULSE)
        pulse = -MOTOR_MAX_PULSE;

    switch (id)
    {
    case Motor_M1:
    {
        pulse = -pulse;
        if (pulse >= 0)
        {
            PWM_M1_A = pulse;
            PWM_M1_B = 0;
        }
        else
        {
            PWM_M1_A = 0;
            PWM_M1_B = -pulse;
        }
        break;
    }
    case Motor_M2:
    {
        pulse = -pulse;
        if (pulse >= 0)
        {
            PWM_M2_A = pulse;
            PWM_M2_B = 0;
        }
        else
        {
            PWM_M2_A = 0;
            PWM_M2_B = -pulse;
        }
        break;
    }

    case Motor_M3:
    {
        if (pulse >= 0)
        {
            PWM_M3_A = pulse;
            PWM_M3_B = 0;
        }
        else
        {
            PWM_M3_A = 0;
            PWM_M3_B = -pulse;
        }
        break;
    }
    case Motor_M4:
    {
        if (pulse >= 0)
        {
            PWM_M4_A = pulse;
            PWM_M4_B = 0;
        }
        else
        {
            PWM_M4_A = 0;
            PWM_M4_B = -pulse;
        }
        break;
    }

    default:
        break;
    }
}



