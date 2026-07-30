#include "bsp.h"

//硬件初始化
void bsp_init(void)
{
	SystemInit();//系统时钟初始化
	
	delay_init();//延迟初始化
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2); //设置为优先级组2
	
	__disable_irq();//关中断
	
	//串口初始化
	USART1_init(115200);
	USART2_init(115200); //先不使用串口

	
	
	//板载led灯初始化
	init_led_gpio();
	//按键初始化
	KEYAll_GPIO_Init();

	
	
#if ENABLE_MOTOR
	//电机初始化
	motor_gpio_init();//引脚初始化
	motor_pwm_init(MOTOR_MAX_PULSE, MOTOR_FREQ_DIVIDE);
	motor_encode_init();//霍尔部分初始化
	PID_Param_Init();//PID初始化
#endif	



	irtracking_init();//8路循迹初始化
//	APP_IRR_PID_Init();//巡线PID参数增量初始化 


	__enable_irq();//开中断
	
	//此定时器要放到最后-避免中断打断 -必须初始化
//	TIM6_Init();//10ms定时器
	
	
	usart_irq_rx_enable(); 
	
}



//JTAG模式设置,用于设置JTAG的模式
//mode:jtag,swd模式设置;00,全使能;01,使能SWD;10,全关闭;
//#define JTAG_SWD_DISABLE   0X02
//#define SWD_ENABLE         0X01
//#define JTAG_SWD_ENABLE    0X00
void Bsp_JTAG_Set(uint8_t mode)
{
	uint32_t temp;
	temp = mode;
	temp <<= 25;
	RCC->APB2ENR |= 1 << 0;	  //开启辅助时钟
	AFIO->MAPR &= 0XF8FFFFFF; //清除MAPR的[26:24]
	AFIO->MAPR |= temp;		  //设置jtag模式
}
