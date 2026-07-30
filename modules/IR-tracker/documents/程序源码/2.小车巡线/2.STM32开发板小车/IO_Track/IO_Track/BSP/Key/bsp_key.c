#include "bsp_key.h"

uint16_t g_key1_long_press = 0;
uint16_t g_key2_long_press = 0;
uint16_t g_key3_long_press = 0;

// 判断按键是否被按下，按下返回KEY_PRESS，松开返回KEY_RELEASE
static uint8_t Key1_is_Press(void)
{
	if (!GPIO_ReadInputDataBit(KEY1_GPIO_PORT, KEY1_GPIO_PIN))
	{
		return KEY_PRESS; // 如果按键被按下，则返回KEY_PRESS
	}
	return KEY_RELEASE;   // 如果按键是松开状态，则返回KEY_RELEASE
}

static uint8_t Key2_is_Press(void)
{
	if (!GPIO_ReadInputDataBit(KEY2_GPIO_PORT, KEY2_GPIO_PIN))
	{
		return KEY_PRESS; // 如果按键被按下，则返回KEY_PRESS
	}
	return KEY_RELEASE;   // 如果按键是松开状态，则返回KEY_RELEASE
}

static uint8_t Key3_is_Press(void)
{
	if (!GPIO_ReadInputDataBit(KEY3_GPIO_PORT, KEY3_GPIO_PIN))
	{
		return KEY_PRESS; // 如果按键被按下，则返回KEY_PRESS
	}
	return KEY_RELEASE;   // 如果按键是松开状态，则返回KEY_RELEASE
}


// 读取按键K1的长按状态，累计达到长按时间返回1，未达到返回0.
// timeout为设置时间长度，单位为秒
uint8_t Key1_Long_Press(uint16_t timeout)
{
	if (g_key1_long_press > 0)
	{
		if (g_key1_long_press < timeout * 100 + 2)
		{
			g_key1_long_press++;
			if (g_key1_long_press == timeout * 100 + 2)
			{
				return 1;
			}
			return 0;
		}
	}
	return 0;
}


// 读取按键K2的长按状态，累计达到长按时间返回1，未达到返回0.
// timeout为设置时间长度，单位为秒
uint8_t Key2_Long_Press(uint16_t timeout)
{
	if (g_key2_long_press > 0)
	{
		if (g_key2_long_press < timeout * 100 + 2)
		{
			g_key2_long_press++;
			if (g_key2_long_press == timeout * 100 + 2)
			{
				return 1;
			}
			return 0;
		}
	}
	return 0;
}

// 读取按键K1的状态，按下返回1，松开返回0.
// mode:设置模式，0：按下一直返回1；1：按下只返回一次1
uint8_t Key1_State(uint8_t mode)
{
	static uint16_t key1_state = 0;

	if (Key1_is_Press() == KEY_PRESS)
	{
		if (key1_state < (mode + 1) * 2)
		{
			key1_state++;
		}
	}
	else
	{
		key1_state = 0;
		g_key1_long_press = 0;
	}
	if (key1_state == 2)
	{
		g_key1_long_press = 1;
		return KEY_PRESS;
	}
	return KEY_RELEASE;
}

// 读取按键K2的状态，按下返回1，松开返回0.
// mode:设置模式，0：按下一直返回1；1：按下只返回一次1
uint8_t Key2_State(uint8_t mode)
{
	static uint16_t key2_state = 0;

	if (Key2_is_Press() == KEY_PRESS)
	{
		if (key2_state < (mode + 1) * 2)
		{
			key2_state++;
		}
	}
	else
	{
		key2_state = 0;
		g_key2_long_press = 0;
	}
	if (key2_state == 2)
	{
		g_key2_long_press = 1;
		return KEY_PRESS;
	}
	return KEY_RELEASE;
}

// 读取按键K3的长按状态，累计达到长按时间返回1，未达到返回0.
// timeout为设置时间长度，单位为秒
uint8_t Key3_Long_Press(uint16_t timeout)
{
	if (g_key3_long_press > 0)
	{
		if (g_key3_long_press < timeout * 100 + 2)
		{
			g_key3_long_press++;
			if (g_key3_long_press == timeout * 100 + 2)
			{
				return 1;
			}
			return 0;
		}
	}
	return 0;
}

// 读取按键K2的状态，按下返回1，松开返回0.
// mode:设置模式，0：按下一直返回1；1：按下只返回一次1
uint8_t Key3_State(uint8_t mode)
{
	static uint16_t key3_state = 0;

	if (Key3_is_Press() == KEY_PRESS)
	{
		if (key3_state < (mode + 1) * 2)
		{
			key3_state++;
		}
	}
	else
	{
		key3_state = 0;
		g_key3_long_press = 0;
	}
	if (key3_state == 2)
	{
		g_key3_long_press = 1;
		return KEY_PRESS;
	}
	return KEY_RELEASE;
}


// 初始化按键1
void Key1_GPIO_Init(void)
{
	/*定义一个GPIO_InitTypeDef类型的结构体*/
	GPIO_InitTypeDef GPIO_InitStructure;
	/*开启按键端口的时钟*/
	RCC_APB2PeriphClockCmd(KEY1_GPIO_CLK, ENABLE);
	//选择按键的引脚
	GPIO_InitStructure.GPIO_Pin = KEY1_GPIO_PIN;
	//设置按键的引脚为输入
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	//使用结构体初始化按键
	GPIO_Init(KEY1_GPIO_PORT, &GPIO_InitStructure);
}

// 初始化按键2
void Key2_GPIO_Init(void)
{
	/*定义一个GPIO_InitTypeDef类型的结构体*/
	GPIO_InitTypeDef GPIO_InitStructure;
	/*开启按键端口的时钟*/
	RCC_APB2PeriphClockCmd(KEY2_GPIO_CLK, ENABLE);
	//选择按键的引脚
	GPIO_InitStructure.GPIO_Pin = KEY2_GPIO_PIN;
	//设置按键的引脚为输入
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	//使用结构体初始化按键
	GPIO_Init(KEY2_GPIO_PORT, &GPIO_InitStructure);
}

// 初始化按键3
void Key3_GPIO_Init(void)
{
	/*定义一个GPIO_InitTypeDef类型的结构体*/
	GPIO_InitTypeDef GPIO_InitStructure;
	/*开启按键端口的时钟*/
	RCC_APB2PeriphClockCmd(KEY3_GPIO_CLK, ENABLE);
	//选择按键的引脚
	GPIO_InitStructure.GPIO_Pin = KEY3_GPIO_PIN;
	//设置按键的引脚为输入
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	//使用结构体初始化按键
	GPIO_Init(KEY3_GPIO_PORT, &GPIO_InitStructure);
}

void KEYAll_GPIO_Init(void)
{
	#ifdef KEY1_USER
		Key1_GPIO_Init();
	#endif
	
	#ifdef KEY2_USER
		Key2_GPIO_Init();
	#endif
	
	#ifdef KEY3_USER
		Key3_GPIO_Init();
	#endif
	
}


/*********************************************END OF FILE**********************/
