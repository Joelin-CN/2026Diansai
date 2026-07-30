#include "app_irtracking.h"

//巡线探头的处理
static void track_deal_four(u8 *s1,u8 *s2,u8 *s3,u8 *s4,u8 *s5,u8 *s6,u8 *s7,u8 *s8)
{
	*s1 = IN_X1;
	*s2 = IN_X2;
	*s3 = IN_X3;
	*s4 = IN_X4;
	
	*s5 = IN_X5;
	*s6 = IN_X6;
	*s7 = IN_X7;
	*s8 = IN_X8;
}



/*************************PID*用PID进行巡黑线********************************/

//pid_t pid_IRR;
////增量PID
//#define IRR_PID_KP       (0) //这可能是接近最好的参数
//#define IRR_PID_KI       (0)
//#define IRR_PID_KD       (0)//kp固定情况下 不能往上调


#define IRTrack_Trun_KP (450) //350 490
#define IRTrack_Trun_KI (0) 
#define IRTrack_Trun_KD (30) 

int pid_output_IRR = 0;

#define IRR_SPEED 			  500  //巡线速度 400：也能巡
#define IRTrack_Minddle    0 //中间的值


float APP_ELE_PID_Calc(int8_t actual_value)
{

	float IRTrackTurn = 0;
	int8_t error;
	static int8_t error_last=0;
	static float IRTrack_Integral;//积分
	

	error=actual_value-IRTrack_Minddle;
	
	IRTrack_Integral +=error;
	
	//	//位置式pid
	IRTrackTurn=error*IRTrack_Trun_KP
							+IRTrack_Trun_KI*IRTrack_Integral
							+(error - error_last)*IRTrack_Trun_KD;
	return IRTrackTurn;
}


//x1-x8 从左往右数
void LineWalking(void)
{
	static int8_t err = 0;
	static u8 x1,x2,x3,x4,x5,x6,x7,x8;
	track_deal_four(&x1,&x2,&x3,&x4,&x5,&x6,&x7,&x8);
	//debug
	//printf("%d\t %d\t %d\t %d\t %d\t %d\t %d\t %d\t \r\n",x1,x2,x3,x4,x5,x6,x7,x8);

	
	//优先判断
  if(x1 == 0 &&  x3 == 0 && x4 == 0 && x5 == 0 && x8 == 0 )
	{
		err = 0;
	}
	//添加直角
	else if((x1 == 0 || x2 == 0 ) && x8 == 1)  //改成|| 再试试直角
	{
		err = -15; 
	}
	//添加直角
	else if((x7 == 0 ||  x8 == 0) && x1 == 1) 
	{
		err = 15 ;
	}
	


	else if(x1 == 1 && x2 == 1  && x3 == 1&& x4 == 0 && x5 == 1 && x6 == 1  && x7 == 1 && x8 == 1) // 1110 1111
	{
		err = -1;
	}
	else if(x1 == 1 && x2 == 1  && x3 == 0&& x4 == 0 && x5 == 1 && x6 == 1  && x7 == 1 && x8 == 1) // 1100 1111
	{
		err = -2;
	}
	else if(x1 == 1 && x2 == 1  && x3 == 0&& x4 == 1 && x5 == 1 && x6 == 1  && x7 == 1 && x8 == 1) // 1101 1111
	{
		err = -2;
	}
	
//	else if(x1 == 1 && x2 == 0  && x3 == 1&& x4 == 1 && x5 == 1 && x6 == 1  && x7 == 1 && x8 == 1) // 1011 1111
//	{
//		err = -3;
//	}
	else if(x1 == 1 && x2 == 0  && x3 == 0&& x4 == 1 && x5 == 1 && x6 == 1  && x7 == 1 && x8 == 1) // 1001 1111
	{
		err = -3;
	}
//		else if(x1 == 0 && x2 == 0  && x3 == 1&& x4 == 1 && x5 == 1 && x6 == 1  && x7 == 1 && x8 == 1) // 0011 1111
//	{
//		err = -4;   //注释，当成直角处理
//	}
//	else if(x1 == 0 && x2 == 1  && x3 == 1&& x4 == 1 && x5 == 1 && x6 == 1  && x7 == 1 && x8 == 1) // 0111 1111
//	{
//		err = -5; //注释，当成直角处理
//	}

	

	
	
	
	else if(x1 == 1 && x2 == 1  && x3 == 1&& x4 == 1 && x5 == 0 && x6 == 1  && x7 == 1 && x8 == 1) // 1111 0111
	{
		err = 1;
	} 
	else if(x1 == 1 && x2 == 1  && x3 == 1&& x4 == 1 && x5 == 0 && x6 == 0  && x7 == 1 && x8 == 1) // 1111 0011
	{
		err = 2;
	}
	else if(x1 == 1 && x2 == 1  && x3 == 1&& x4 == 1 && x5 == 1 && x6 == 0  && x7 == 1 && x8 == 1) // 1111 1011
	{
		err = 2;
	}
	else if(x1 == 1 && x2 == 1  && x3 == 1&& x4 == 1 && x5 == 1 && x6 == 0  && x7 == 0 && x8 == 1) // 1111 1001
	{
		err = 3;
	}
	
//	else if(x1 == 1 && x2 == 1  && x3 == 1&& x4 == 1 && x5 == 1 && x6 == 1  && x7 == 0 && x8 == 1) // 1111 1101
//	{
//		err = 3;
//	}
//	else if(x1 == 1 && x2 == 1  && x3 == 1&& x4 == 1 && x5 == 1 && x6 == 1  && x7 == 0 && x8 == 0) // 1111 1100
//	{
//		err = 4; ///当成直角处理
//	}
//		else if(x1 == 1 && x2 == 1  && x3 == 1&& x4 == 1 && x5 == 1 && x6 == 1  && x7 == 1 && x8 == 0) // 1111 1110
//	{
//		err = 5; //当成直角处理
//	}
	

	
 
	else if(x1 == 1 &&x2 == 1 &&x3 == 1 && x4 == 0 && x5 == 0 && x6 == 1 && x7 == 1&& x8 == 1) //直走
	{
		err = 0;
	}
	
	
	
	//剩下的就保持上一个状态	
	pid_output_IRR = (int)(APP_ELE_PID_Calc(err));
	
	Motion_Ctrl(IRR_SPEED, 0, pid_output_IRR, 0);

}

