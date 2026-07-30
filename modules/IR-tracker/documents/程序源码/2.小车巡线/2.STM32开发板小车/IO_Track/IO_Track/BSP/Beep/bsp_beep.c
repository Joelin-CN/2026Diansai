#include "bsp_beep.h"

u32 beep_time = 0;
u16 beep_rate = 0;

void init_beep(void)
{
	//初始化蜂鸣器的引脚
  GPIO_InitTypeDef GPIO_InitStructure;
	/*开启外设时钟*/
	RCC_APB2PeriphClockCmd(BEEP_RCC, ENABLE); 
	/*选择要控制的引脚*/															   
  	GPIO_InitStructure.GPIO_Pin = BEEP_PIN;	
	/*设置引脚模式为通用推挽输出*/
  	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;   
	/*设置引脚速率为50MHz */   
  	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz; 
	/*调用库函数，初始化BEEP_PORT*/
  	GPIO_Init(BEEP_PORT, &GPIO_InitStructure);

}

/**
* Function                
* @brief         设置蜂鸣器响的时长,频率  10ms中断
* @param[in]     
* @param[out]    times:响的时长//1s起步  rates:频率 //100ms起步
* @retval        void
* @par History   无
*/
void set_beep_time(u8 times,u16 rates)//times:鸣叫的时长
{
	beep_time = times * 100; //要*10才是正确时间
	beep_rate = rates *100;	
}


void open_beep(void)
{
	if(beep_time != 0)
	{
		BEEP_BEEP = !BEEP_BEEP;
		delay_ms(beep_rate);
	}
	
	else if(beep_time == 0)
	{
		BEEP_OFF;
		beep_rate = 0;
	}
		
}

