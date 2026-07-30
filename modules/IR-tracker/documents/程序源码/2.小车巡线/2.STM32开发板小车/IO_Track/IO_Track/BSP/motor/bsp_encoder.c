#include "bsp_encoder.h"

int g_Encoder_M1_Now = 0;
int g_Encoder_M2_Now = 0;
int g_Encoder_M3_Now = 0;
int g_Encoder_M4_Now = 0;


static void encode_M1_TIM4(void)
{
	TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
	TIM_ICInitTypeDef TIM_ICInitStructure;
	GPIO_InitTypeDef GPIO_InitStructure;
	Bsp_JTAG_Set(SWD_ENABLE);    //=====打开SWD接口,关闭JTAG接口 可以利用主板的SWD接口调试
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);  // 使能AFIO时钟
	GPIO_PinRemapConfig(GPIO_Remap_TIM4, ENABLE);    // remap TIM4引脚CH1 CH2
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE); // 使能定时器4的时钟
	
	//A端
	RCC_APB2PeriphClockCmd(HAL_1A_CLK, ENABLE);			  //使能端口时钟
	GPIO_InitStructure.GPIO_Pin = HAL_1A_PIN;			  //端口配置
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING; //浮空输入
	GPIO_Init(HAL_1A_PORT, &GPIO_InitStructure);		  //根据设定参数初始化GPIO
	//B端
	RCC_APB2PeriphClockCmd(HAL_1B_CLK, ENABLE);			  //使能端口时钟
	GPIO_InitStructure.GPIO_Pin = HAL_1B_PIN;			  //端口配置
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING; //浮空输入
	GPIO_Init(HAL_1B_PORT, &GPIO_InitStructure);		  //根据设定参数初始化GPIO
	
	TIM_TimeBaseStructInit(&TIM_TimeBaseStructure);
	TIM_TimeBaseStructure.TIM_Prescaler = 0x0;					//预分频器
	TIM_TimeBaseStructure.TIM_Period = ENCODER_TIM_PERIOD;		//设定计数器自动重装值
	TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;		//选择时钟分频：不分频
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up; //TIM向上计数
	TIM_TimeBaseInit(TIM4, &TIM_TimeBaseStructure);
	
	TIM_EncoderInterfaceConfig(TIM4, TIM_EncoderMode_TI12, TIM_ICPolarity_Rising, TIM_ICPolarity_Rising); //使用编码器模式3
	
	TIM_ICStructInit(&TIM_ICInitStructure);
	TIM_ICInitStructure.TIM_ICFilter = 10;
    
	TIM_ICInit(TIM4, &TIM_ICInitStructure);
	TIM_ClearFlag(TIM4, TIM_FLAG_Update);                  //清除TIM的更新标志位
	TIM_ITConfig(TIM4, TIM_IT_Update, ENABLE);
	//Reset counter
	TIM_SetCounter(TIM4, 0);
	//===============================================
	TIM4->CNT = 0x7fff;
	//===============================================
	TIM_Cmd(TIM4, ENABLE);

}


static void encode_M2_TIM2(void)
{
	TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
	TIM_ICInitTypeDef TIM_ICInitStructure;
	GPIO_InitTypeDef GPIO_InitStructure;
	Bsp_JTAG_Set(SWD_ENABLE);    //=====打开SWD接口,关闭JTAG接口 可以利用主板的SWD接口调试
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);  // 使能AFIO时钟
	GPIO_PinRemapConfig(GPIO_PartialRemap1_TIM2, ENABLE);    // remap TIM2引脚CH1 CH2
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE); // 使能定时器2的时钟


	RCC_APB2PeriphClockCmd(HAL_2A_CLK, ENABLE);			  //使能端口时钟
	GPIO_InitStructure.GPIO_Pin = HAL_2A_PIN;			  //端口配置
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING; //浮空输入
	GPIO_Init(HAL_2A_PORT, &GPIO_InitStructure);		  //根据设定参数初始化GPIO

	RCC_APB2PeriphClockCmd(HAL_2B_CLK, ENABLE);			  //使能端口时钟
	GPIO_InitStructure.GPIO_Pin = HAL_2B_PIN;			  //端口配置
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING; //浮空输入
	GPIO_Init(HAL_2B_PORT, &GPIO_InitStructure);		  //根据设定参数初始化GPIO

	TIM_TimeBaseStructInit(&TIM_TimeBaseStructure);
	TIM_TimeBaseStructure.TIM_Prescaler = 0x0;					//预分频器
	TIM_TimeBaseStructure.TIM_Period = ENCODER_TIM_PERIOD;		//设定计数器自动重装值
	TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;		//选择时钟分频：不分频
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up; //TIM向上计数
	TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

	TIM_EncoderInterfaceConfig(TIM2, TIM_EncoderMode_TI12, TIM_ICPolarity_Rising, TIM_ICPolarity_Rising); //使用编码器模式3

	TIM_ICStructInit(&TIM_ICInitStructure);
	TIM_ICInitStructure.TIM_ICFilter = 10;
    
	TIM_ICInit(TIM2, &TIM_ICInitStructure);
	TIM_ClearFlag(TIM2, TIM_FLAG_Update);                  //清除TIM的更新标志位
	TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);
	//Reset counter
	TIM_SetCounter(TIM2, 0);
	//===============================================
	TIM2->CNT = 0x7fff;
	//===============================================
	TIM_Cmd(TIM2, ENABLE);

}


static void encode_M3_TIM5(void)
{
	TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
	TIM_ICInitTypeDef TIM_ICInitStructure;
	GPIO_InitTypeDef GPIO_InitStructure;
	Bsp_JTAG_Set(SWD_ENABLE);    //=====打开SWD接口,关闭JTAG接口 可以利用主板的SWD接口调试
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM5, ENABLE); // 使能定时器5的时钟
	//A
	RCC_APB2PeriphClockCmd(HAL_3A_CLK, ENABLE);			  //使能端口时钟
	GPIO_InitStructure.GPIO_Pin = HAL_3A_PIN;			  //端口配置
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING; //浮空输入
	GPIO_Init(HAL_3A_PORT, &GPIO_InitStructure);		  //根据设定参数初始化GPIO
	//B
	RCC_APB2PeriphClockCmd(HAL_3B_CLK, ENABLE);			  //使能端口时钟
	GPIO_InitStructure.GPIO_Pin = HAL_3B_PIN;			  //端口配置
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING; //浮空输入
	GPIO_Init(HAL_3B_PORT, &GPIO_InitStructure);		  //根据设定参数初始化GPIO
	
	TIM_TimeBaseStructInit(&TIM_TimeBaseStructure);
	TIM_TimeBaseStructure.TIM_Prescaler = 0x0;					//预分频器
	TIM_TimeBaseStructure.TIM_Period = ENCODER_TIM_PERIOD;		//设定计数器自动重装值
	TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;		//选择时钟分频：不分频
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up; //TIM向上计数
	TIM_TimeBaseInit(TIM5, &TIM_TimeBaseStructure);

	TIM_EncoderInterfaceConfig(TIM5, TIM_EncoderMode_TI12, TIM_ICPolarity_Rising, TIM_ICPolarity_Rising); //使用编码器模式3

	TIM_ICStructInit(&TIM_ICInitStructure);
	TIM_ICInitStructure.TIM_ICFilter = 10;
    
	TIM_ICInit(TIM5, &TIM_ICInitStructure);
	TIM_ClearFlag(TIM5, TIM_FLAG_Update);                  //清除TIM的更新标志位
	TIM_ITConfig(TIM5, TIM_IT_Update, ENABLE);
	//Reset counter
	TIM_SetCounter(TIM5, 0);
	//===============================================
	TIM5->CNT = 0x7fff;
	//===============================================
	TIM_Cmd(TIM5, ENABLE);
}


static void encode_M4_TIM3(void)
{
	TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
	TIM_ICInitTypeDef TIM_ICInitStructure;
	GPIO_InitTypeDef GPIO_InitStructure;
	
	Bsp_JTAG_Set(SWD_ENABLE);    //=====打开SWD接口,关闭JTAG接口 可以利用主板的SWD接口调试
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);  // 使能AFIO时钟
	GPIO_PinRemapConfig(GPIO_PartialRemap_TIM3, ENABLE);    // remap TIM3引脚CH1 CH2
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE); // 使能定时器3的时钟
	
	//A
	RCC_APB2PeriphClockCmd(HAL_4A_CLK, ENABLE);			  //使能端口时钟
	GPIO_InitStructure.GPIO_Pin = HAL_4A_PIN;			  //端口配置
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING; //浮空输入
	GPIO_Init(HAL_4A_PORT, &GPIO_InitStructure);		  //根据设定参数初始化GPIO
	//B
	RCC_APB2PeriphClockCmd(HAL_3B_CLK, ENABLE);			  //使能端口时钟
	GPIO_InitStructure.GPIO_Pin = HAL_4B_PIN;			  //端口配置
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING; //浮空输入
	GPIO_Init(HAL_4B_PORT, &GPIO_InitStructure);		  //根据设定参数初始化GPIO
	
	TIM_TimeBaseStructInit(&TIM_TimeBaseStructure);
	TIM_TimeBaseStructure.TIM_Prescaler = 0x0;					// 预分频器
	TIM_TimeBaseStructure.TIM_Period = ENCODER_TIM_PERIOD;		//设定计数器自动重装值
	TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;		//选择时钟分频：不分频
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up; //TIM向上计数
	TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);
	TIM_EncoderInterfaceConfig(TIM3, TIM_EncoderMode_TI12, TIM_ICPolarity_Rising, TIM_ICPolarity_Rising); //使用编码器模式3
	TIM_ICStructInit(&TIM_ICInitStructure);
	TIM_ICInitStructure.TIM_ICFilter = 10;
	TIM_ICInit(TIM3, &TIM_ICInitStructure);
	TIM_ClearFlag(TIM3, TIM_FLAG_Update);                   //清除TIM的更新标志位
	TIM_ITConfig(TIM3, TIM_IT_Update, ENABLE);
	//Reset counter
	TIM_SetCounter(TIM3, 0);
	//===============================================
	TIM3->CNT = 0x7fff;
	//===============================================
	TIM_Cmd(TIM3, ENABLE);
}

void motor_encode_init(void)
{
	encode_M1_TIM4();
	encode_M2_TIM2();
	encode_M3_TIM5();	
	encode_M4_TIM3();
}

/**
 * @Brief: 读取编码器计数
 * @Note: 
 * @Parm: 电机的ID号:Motor_M1, Motor_M2, Motor_M3, Motor_M4
 * @Retval: 返回编码器计数数据
 */
static int16_t Encoder_Read_CNT(uint8_t Motor_id)
{
	int16_t Encoder_TIM = 0;
	switch(Motor_id)
	{
	case Motor_M1:  Encoder_TIM = 0x7fff - (short)TIM4 -> CNT; TIM4 -> CNT = 0x7fff; break;
	case Motor_M2:  Encoder_TIM = 0x7fff - (short)TIM2 -> CNT; TIM2 -> CNT = 0x7fff; break;
	case Motor_M3:  Encoder_TIM = 0x7fff - (short)TIM5 -> CNT; TIM5 -> CNT = 0x7fff; break;
	case Motor_M4:  Encoder_TIM = 0x7fff - (short)TIM3 -> CNT; TIM3 -> CNT = 0x7fff; break;
	default:  break;
	}
	return Encoder_TIM;
}

// 返回开机到现在总共统计的编码器的计数（单路）。
int Encoder_Get_Count_Now(uint8_t Motor_id)
{
	if (Motor_id == Motor_M1) return g_Encoder_M1_Now;
	if (Motor_id == Motor_M2) return g_Encoder_M2_Now;
	if (Motor_id == Motor_M3) return g_Encoder_M3_Now;
	if (Motor_id == Motor_M4) return g_Encoder_M4_Now;
	return 0;
}

// 获取开机到现在总共的四路编码器计数。
void Encoder_Get_ALL(int* Encoder_all)
{
	Encoder_all[0] = g_Encoder_M1_Now;
	Encoder_all[1] = g_Encoder_M2_Now;
	Encoder_all[2] = g_Encoder_M3_Now;
	Encoder_all[3] = g_Encoder_M4_Now;
}

// 更新编码器的计数总值。 //10ms调用一次
void Encoder_Update_Count(void)
{
		g_Encoder_M1_Now -= Encoder_Read_CNT(Motor_M1);
		g_Encoder_M2_Now -= Encoder_Read_CNT(Motor_M2);
		g_Encoder_M3_Now += Encoder_Read_CNT(Motor_M3);
		g_Encoder_M4_Now += Encoder_Read_CNT(Motor_M4);
//	 g_Encoder_M1_Now += Encoder_Read_CNT(Motor_M1);
//	 g_Encoder_M2_Now += Encoder_Read_CNT(Motor_M2);
//	 g_Encoder_M3_Now -= Encoder_Read_CNT(Motor_M3);
//   g_Encoder_M4_Now -= Encoder_Read_CNT(Motor_M4);
		

	 

	
}

