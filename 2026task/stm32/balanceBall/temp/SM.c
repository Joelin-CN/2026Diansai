/* Private includes ----------------------------------------------------------*/

/*CODE INCLUDE BEGIN*/
#include "SM.h"




/* Private variables ---------------------------------------------------------*/
/*CODE VARIABLE BEGIN*/


/*CODE VARIABLE END*/

/* Private function prototypes -----------------------------------------------*/
/*CODE FUNCTION BEGIN*/
/*循环缓冲区
// 初始化循环缓冲区
void CircBuf_Init(BufferTypedef *cb, uint8_t *buffer) {
  cb->buf = buffer;
  cb->read_idx=0;
  cb->write_idx=0;
}
// 写数据到缓冲区（返回是否成功）
uint8_t CircBuf_Write(BufferTypedef *cb, uint8_t* data,int size,UART_HandleTypeDef* huart) {
  memcpy(&cb->buf[cb->write_idx],data,size);

  cb->write_idx += size;
  cb->write_idx%=128;
  cb->read_idx=(cb->read_idx+size)%128;
  return 1;
}
*/

/*ti串口*/
/*
//串口发送函数ti
int Usart_Transmit(SM_UartHandle huart, uint8_t *pData, int size)
{
    if (pData == NULL || size <= 0) {
        return 0;
    }

    for (int i = 0; i < size; i++) {
        while (DL_UART_isTXFIFOFull(huart)) {
        }

        DL_UART_transmitData(huart, pData[i]);
    }

    while (DL_UART_isBusy(huart)) {
    }

    return size;
}

//串口接收函数ti
int Usart_Receive(SM_UartHandle huart, uint8_t *pData, int size)
{
    int count = 0;

    if (pData == NULL || size <= 0) {
        return 0;
    }

    while (count < size) {
        if (!DL_UART_isRXFIFOEmpty(huart)) {
            pData[count++] = DL_UART_receiveData(huart);
        }
    }

    return count;
}
*/

//命令合成
void SM_OrderMake(SMTypeDef* member,uint8_t orderPart[],int size)
{
	member->command[0]=member->ID;
	for(int i=1;i<=size;i++)
	{
		member->command[i]=orderPart[i-1];
	}
	member->command[size+1]=CheckSum;
	member->commandsize=size+2;
}

//校验命令
int SM_OrderJudge(SMTypeDef member){
	if(member.back[0]==member.command[0]
			&&member.back[1]==member.command[1]
			//&&member.back[strlen(member.back)-1]==member.command[strlen(member.command)-1])
			 &&member.back[member.backsize - 1] == CheckSum)
	{
		if(member.back[2]==0x02){return 1;}
		else if(member.back[2]==0xE2){return 2;}
		else if(member.back[2]==0xEE){return 0;}
	}
	return 3;
}

//计算脉冲
void SM_AngleToPulse(SMTypeDef* member,float angle,int isReal){
	float InitialPulse=angle*200/360;
	if(isReal){
		member->realpulse=InitialPulse*member->microstepping;
	}
	else{
		member->pulse=InitialPulse*member->microstepping;
	}
}

//计算角度
void SM_PulseToAngle(SMTypeDef* member,int pulse,int isReal){
	float InitialAngle=pulse*360/200;
	if(isReal){
		member->realangle=InitialAngle/member->microstepping;
	}
	else{
		member->angle=InitialAngle/member->microstepping;
	}
}

//计算角速度
void SM_PulseToRate(SMTypeDef* member,int pulse,int isReal){
    angular_velocity = rate * 6.0f;  // RPM × 6 = °/s
	if(isReal){
		member->realrate=member->rate;
		member->realangular_velocity=member->angular_velocity;
	}
	else{
		member->rate=member->rate;
		member->angular_velocity=member->angular_velocity;
	}
}

//电机使能控制 [地址 + 0xF3 + 0xAB + 使能状态 + 多机同步标志 + 校验字节]
//返回：Addr F3 02/E2/EE 6B 长度：4
void SM_EnableControl(SMTypeDef* member){
	uint8_t temp[4];
	temp[0]=0xF3;
	temp[1]=0xAB;
	temp[2]=0x01;
	temp[3]=SM_EXEC_IMMEDIATE;//MultiMachineSynchronizationFlag;
	SM_OrderMake(member,temp,4);
	member->commandsize=6;
	member->backsize=4;
	Usart_Transmit(member->huart,member->command,member->commandsize);
}

//电机速度控制 [地址 + 0xF6 + 方向 + 速度 + 加速度 + 多机同步标志 + 校验字节]
//返回：Addr F6 02/12/22/E2/EE 6B 长度：4
void SM_RateControl(SMTypeDef* member,uint8_t direction,int rate,int acceleration){
	uint8_t temp[6];
		temp[0]=0xF6;
		temp[1]=direction;
		member->direction=direction;
		temp[2]=rate/256;
		temp[3]=rate%256;
		member->rate=rate;
		temp[4]=acceleration;
		member->acceleration=acceleration;
		temp[5]=SM_EXEC_IMMEDIATE;//MultiMachineSynchronizationFlag;
		SM_OrderMake(member,temp,6);
		member->commandsize=8;
		member->backsize=4;
		Usart_Transmit(member->huart,member->command,member->commandsize);
}

//电机位置控制 [地址 + 0xFD + 方向 + 速度+ 加速度 + 脉冲数 + 相对/绝对模式标志 + 多机同步标志 + 校验字节]
//返回：Addr FD 02/12/22/9F/E2/EE 6B 长度：4
void SM_SiteControl(SMTypeDef* member,int direction,int rate,int acceleration,int pulse,int mode){
	uint8_t temp[11];
			temp[0]=0xFD;
			temp[1]=direction;
			temp[2]=rate/256;
			temp[3]=rate%256;
			temp[4]=acceleration;
			temp[5]=pulse/256/256/256;
			temp[6]=pulse/256/256%256;
			temp[7]=pulse/256%256;
			temp[8]=pulse%256;
			member->pulse=pulse;
			temp[9]=mode;
			member->mode=mode;
			temp[10]=SM_EXEC_IMMEDIATE;//MultiMachineSynchronizationFlag;
			SM_OrderMake(member,temp,11);
			member->commandsize=13;
			member->backsize=4;
			Usart_Transmit(member->huart,member->command,member->commandsize);
}

//F1 快速位置参数设置 [地址 + 0xF1 + 速度 + 加速度 + 运动模式 + 多机同步标志 + 校验字节]/Addr F1 SpeedHi SpeedLo Acc Mode Sync 6B
//返回：Addr F1 02/12/22/9F/E2/EE 6B 长度：4
void SM_F1Control(SMTypeDef* member,int rate,int acceleration,int mode){
	uint8_t temp[6];
		temp[0]=0xF1;
		temp[1]=rate/256;
		temp[2]=rate%256;
		member->rate=rate;
		temp[3]=acceleration;
		member->acceleration=acceleration;
		temp[4]=mode;
		member->mode=mode;	
		//temp[5]=SM_EXEC_SYNC_WAIT ;
		temp[5]=SM_EXEC_IMMEDIATE;//MultiMachineSynchronizationFlag;
		SM_OrderMake(member,temp,6);
		member->commandsize=8;
		member->backsize=4;
		Usart_Transmit(member->huart,member->command,member->commandsize);
}

// FC 快速位置运动 [地址 + 0xFC + 脉冲数  + 校验字节]/Addr FC Pulse3 Pulse2 Pulse1 Pulse0 6B
//返回：Addr FC 02/12/22/9F/E2/EE 6B 长度：4
void SM_FCControl(SMTypeDef* member,int pulse){
	uint8_t temp[5];
	temp[0]=0xFC;
	uint32_t p = (uint32_t)pulse;
	temp[1]=(p >> 24) & 0xFF;
	temp[2]=(p >> 16) & 0xFF;
	temp[3]=(p >> 8) & 0xFF;
	temp[4]=p & 0xFF;
	member->pulse=pulse;
	SM_OrderMake(member,temp,5);
	member->commandsize=7;
	member->backsize=4;
	Usart_Transmit(member->huart,member->command,member->commandsize);
}

//FF 多机同步触发 [地址 + 0xFF+0x66 + 校验字节]/Addr FF 66 6B
//返回：Addr FF 02/E2/EE 6B 长度：4
void SM_FFControl(SMTypeDef* member){
	uint8_t temp[2];
	temp[0]=0xFF;
	temp[1]=0x66;
	member->command[0]=0x00;
	for(int i=1;i<=2;i++)
	{
		member->command[i]=temp[i-1];
	}
	member->command[3]=CheckSum;
	member->commandsize=4;
	member->backsize=4;
	Usart_Transmit(member->huart,member->command,member->commandsize);
}

//36 读取实时位置 [Addr 36 6B]返回结构：Addr 36 Sign Pos3 Pos2 Pos1 Pos0 6B
//返回：Addr 36 Sign Pos3 Pos2 Pos1 Pos0 6B 长度：8
void SM_RealTimePositionRead(SMTypeDef* member){
	uint8_t temp[1];
	temp[0]=0x36;
	SM_OrderMake(member,temp,1);
	member->commandsize=3;
	member->backsize=8;
	Usart_Transmit(member->huart,member->command,member->commandsize);
	Usart_Receive(member->huart,member->back);
	if(member->back[0]==member->ID
			&&member->back[1]==0x36
			&&member->back[7]==CheckSum)
	{
		member->direction=member->back[2];
		member->realpulse=member->back[3]*256*256*256\
						+member->back[4]*256*256\
						+member->back[5]*256\
						+member->back[6];
		SM_PulseToAngle(member,member->realpulse,SM_REAL);
						
	}
}

//3A 读取电机状态标志 [Addr 3A 6B]返回结构：Addr 3A Status 6B
//返回：Addr 3A Status 6B 长度：4
void SM_MotorStatusRead(SMTypeDef* member){
	uint8_t temp[1];
	temp[0]=0x3A;
	SM_OrderMake(member,temp,1);
	member->commandsize=3;
	member->backsize=4;
	Usart_Transmit(member->huart,member->command,member->commandsize);
	Usart_Receive(member->huart,member->back);
	if(member->back[0]==member->ID
			&&member->back[1]==0x3A
			&&member->back[3]==CheckSum)
	{
		member->enble=member->back[2];
	}
}



//设置细分 [地址 + 0x84 + 0x8A + 是否存储标志 + 细分值 + 校验字节]
//返回：Addr 84 02/E2/EE 6B 长度：4
void SM_MicrosteppingSet(SMTypeDef* member,int flag,int microstepping){
	uint8_t temp[4];
		temp[0]=0x84;
		temp[1]=0x8A;
		temp[2]=flag;
		temp[3]=microstepping;
		member->microstepping=microstepping;
		SM_OrderMake(member,temp,4);
		member->commandsize=6;
		member->backsize=4;
		Usart_Transmit(member->huart,member->command,member->commandsize);
}

//SM初始化
void SM_Initial(SMTypeDef* member1/*,SMTypeDef* member2*/){
	member1->huart=SMhuart1;
	//member2->huart=SMhuart2;
	member1->ID=0x01;
	//member2->ID=0x02;
	SM_MicrosteppingSet(member1,01,128);
	//SM_MicrosteppingSet(member2,01,128);
	SM_EnableControl(member1);
	//SM_EnableControl(member2);
}

//SM停止 [地址 + 0xFE + 0x98 + 多机同步标志 + 校验字节]
//返回：Addr FE 02/E2/EE 6B 长度：4
void SM_Stop(SMTypeDef* member){
	uint8_t temp[3];
		temp[0]=0xFE;
		temp[1]=0x98;
		temp[2]= SM_EXEC_IMMEDIATE;
		SM_OrderMake(member,temp,3);
		member->commandsize=5;
		member->backsize=4;
		Usart_Transmit(member->huart,member->command,member->commandsize);
}

//SM单圈零点设置 [地址 + 0x93 + 0x88 + 是否存储标志(01/00) + 校验字节]
//返回：Addr 93 02/E2/EE 6B 长度：4
void SM_InitialPlaceSet(SMTypeDef* member,int flag){
	uint8_t temp[3];
			temp[0]=0x93;
			temp[1]=0x88;
			temp[2]=flag;
			SM_OrderMake(member,temp,3);
			member->commandsize=5;
			member->backsize=4;
			Usart_Transmit(member->huart,member->command,member->commandsize);
}

//SM触发回零 [地址 + 0x9A + 回零模式(单圈/多圈碰撞/多圈开关) + 多机同步标志 + 校验字节]
//返回：Addr 9A 02/12/9F/E2/EE 6B 长度：4
void SM_InitialPlaceBack(SMTypeDef* member,int BackMode){
	uint8_t temp[3];
			temp[0]=0x9A;
			temp[1]=BackMode;
			temp[2]= SM_EXEC_IMMEDIATE	;
			SM_OrderMake(member,temp,3);
			member->commandsize=5;
			member->backsize=4;
			Usart_Transmit(member->huart,member->command,member->commandsize);
}

//SM退出回零 [地址 + 0x9C + 0x48 + 校验字节]
//返回：Addr 9C 02/E2/EE 6B 长度：4
void SM_InitialPlaceBackNow(SMTypeDef* member){
	uint8_t temp[2];
			temp[0]=0x9c;
			temp[1]=0x48;
			SM_OrderMake(member,temp,2);
			member->commandsize=4;
			member->backsize=4;
			Usart_Transmit(member->huart,member->command,member->commandsize);
}

//SM读取pid [地址 + 0x21 + 校验字节]
//返回（Emm）：Addr 21 Kp3 Kp2 Kp1 Kp0 Ki3 Ki2 Ki1 Ki0 Kd3 Kd2 Kd1 Kd0 6B 长度：15
//返回（X）：Addr 21 pTkp3 pTkp2 pTkp1 pTkp0 pBkp3 pBkp2 pBkp1 pBkp0 vkp3 vkp2 vkp1 vkp0 vki3 vki2 vki1 vki0 6B 长度：19
void SM_PidRead(SMTypeDef* member){
	uint8_t temp[1];
			temp[0]=0x21;
			SM_OrderMake(member,temp,1);
			member->commandsize=3;
			member->backsize=15;
			Usart_Transmit(member->huart,member->command,member->commandsize);
			Usart_Receive(member->huart,member->back);
			if(member->back[0]==member->ID
					&&member->back[1]==0x21
					&&member->back[14]==CheckSum)
			{
				member->Kp=member->back[2]*256*256*256\
								+member->back[3]*256*256\
								+member->back[4]*256\
								+member->back[5];
				member->Ki=member->back[6]*256*256*256\
								+member->back[7]*256*256\
								+member->back[8]*256\
								+member->back[9];
				member->Kd=member->back[10]*256*256*256\
								+member->back[11]*256*256\
								+member->back[12]*256\
								+member->back[13];
			}
}
/*CODE FUNCTION END*/
