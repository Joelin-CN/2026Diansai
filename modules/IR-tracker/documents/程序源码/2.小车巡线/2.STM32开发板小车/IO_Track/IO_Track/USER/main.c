#include "AllHeader.h"

u8 g_board_no_error = 0;//板子没有错误的情况下


int main()
{
	bsp_init();
		
	
	//需要等待红外正常才进行下一步
	while(!Key1_State(1));
	TIM6_Init();//10ms定时器 

	while(1)
	{ 

		
		//IO直接巡线 
		//LineWalking();
		

	}
	
}

