/*4路循迹驱动文件*/

#include "bsp_irtracking.h"

void irtracking_init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	RCC_APB2PeriphClockCmd(IR_X1_Clk|IR_X2_Clk|IR_X3_Clk|IR_X4_Clk, ENABLE); 
	RCC_APB2PeriphClockCmd(IR_X5_Clk|IR_X6_Clk|IR_X7_Clk|IR_X8_Clk, ENABLE); 
	
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_IN_FLOATING;
  GPIO_InitStructure.GPIO_Pin=IR_X1_Pin;
  GPIO_Init(IR_X1_Port,&GPIO_InitStructure);
	
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_IN_FLOATING;
  GPIO_InitStructure.GPIO_Pin=IR_X2_Pin;
  GPIO_Init(IR_X2_Port,&GPIO_InitStructure);
	
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_IN_FLOATING;
  GPIO_InitStructure.GPIO_Pin=IR_X3_Pin;
  GPIO_Init(IR_X3_Port,&GPIO_InitStructure);
	
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_IN_FLOATING;
  GPIO_InitStructure.GPIO_Pin=IR_X4_Pin;
  GPIO_Init(IR_X4_Port,&GPIO_InitStructure);
	
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_IN_FLOATING;
  GPIO_InitStructure.GPIO_Pin=IR_X5_Pin;
  GPIO_Init(IR_X5_Port,&GPIO_InitStructure);
	
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_IN_FLOATING;
  GPIO_InitStructure.GPIO_Pin=IR_X6_Pin;
  GPIO_Init(IR_X6_Port,&GPIO_InitStructure);
	
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_IN_FLOATING;
  GPIO_InitStructure.GPIO_Pin=IR_X7_Pin;
  GPIO_Init(IR_X7_Port,&GPIO_InitStructure);
	
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_IN_FLOATING;
  GPIO_InitStructure.GPIO_Pin=IR_X8_Pin;
  GPIO_Init(IR_X8_Port,&GPIO_InitStructure);
	
	
}



