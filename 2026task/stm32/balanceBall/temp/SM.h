#ifndef __SM_H__
#define __SM_H__
/* Private includes ----------------------------------------------------------*/
/*CODE INCLUDE BEGIN*/
#include <stdio.h>
#include <string.h>
#include "main.h"
#include <stdint.h>
#include "usart.h"
#include "dma.h"

/*CODE INCLUDE END*/


/* Private define ------------------------------------------------------------*/
/*CODE MACRO BEGIN*/
/*
// 定义循环缓冲区结构体
typedef struct {
  uint8_t *buf;       // 缓冲区数组
  uint16_t read_idx;  // 读指针（下一个要读的位置）
  uint16_t write_idx; // 写指针（下一个要写的位置）
} BufferTypedef;
//缓冲区
extern uint8_t SMbuffer[128];
extern BufferTypedef SMbuf;
*/

//缓冲区

extern uint8_t SMbuffer[10][50];
extern uint8_t SMbufIdx;
extern uint8_t SMFlag;


//串口中断重声明
/*
extern DMA_HandleTypeDef hdma_usart1_rx;
extern DMA_HandleTypeDef hdma_usart2_rx;
*/

//串口重定义

#define SMhuart1 &huart1
#define SMhuart2 &huart2

/*#define SMhuart1 UART_0_INST//实际名称
#define SMhuart2 UART_1_INST
typedef void* SM_UartHandle;*/

//串口发送函数

#define Usart_Transmit(huart,pData,size)\
	do { \
    if (pData != NULL) { \
    	memcpy(SMbuffer[SMbufIdx],pData,size);\
    	HAL_UART_Transmit_DMA(huart,SMbuffer[SMbufIdx], size);\
    	SMbufIdx+=1;\
    	SMbufIdx%=10;\
    HAL_Delay(1);\
    } \
} while(0)



//串口接收函数

#define Usart_Receive(huart,pData)\
	do { \
    if (pData != NULL) { \
        HAL_UARTEx_ReceiveToIdle_DMA(huart, pData, 50);\
    } \
} while(0)
 


//校验字节
#define CheckSum 0x6B

//多机同步标志
//#define MultiMachineSynchronizationFlag 0x00
#define SM_EXEC_IMMEDIATE  0x00
#define SM_EXEC_SYNC_WAIT  0x01

//错误标志
//#define ErroFlag 01 00 EE 6B
static const uint8_t ErroFlag[4] = {0x01, 0x00, 0xEE, 0x6B};

//数据真实或目标
#define SM_TARGET 0
#define SM_REAL 1

//未细分前的一圈的脉冲
#define InitialPPR 200
/*CODE MACRO END*/


/* Private variables ---------------------------------------------------------*/
/*CODE VARIABLE BEGIN*/
typedef struct SMTypeDef{
	int ID;//地址
	int rate;//速率
	float realrate;//实时速率
	float angular_velocity;//角速度
	float realangular_velocity;//实时角速度
	int pulse;//脉冲
	int realpulse;//实时脉冲
	float angle;//角度
	float realangle;//实时角度
	int acceleration;//加速度
	int direction;//方向
	int enble;//使能
	int Kp,Ki,Kd;//pid
	int microstepping;//细分
	int mode;//相对/绝对模式
	int commandsize;//命令长度
	int backsize;//返回长度
	uint8_t command[50];//命令
	uint8_t back[50];//返回值
	UART_HandleTypeDef* huart;//串口
	//SM_UartHandle huart;//串口
}SMTypeDef;

/*使用需要定义*/
/*
 * SMTypeDef SM01;
 * SMTypeDef SM02;
 * uint8_t SMbuffer[10][50];
 * uint8_t SMbufIdx=0;
 */
/*CODE VARIABLE END*/


/* Private function prototypes -----------------------------------------------*/
/*CODE FUNCTION BEGIN*/
/*
void CircBuf_Init(BufferTypedef *cb, uint8_t *buffer);// 初始化循环缓冲区
uint8_t CircBuf_Write(BufferTypedef *cb, uint8_t* data,int size,UART_HandleTypeDef* huart);// 写数据到缓冲区（返回是否成功）
*/
/*int Usart_Transmit(SM_UartHandle huart, uint8_t *pData, int size);//串口发送函数
int Usart_Receive(SM_UartHandle huart, uint8_t *pData, int size);//串口接收函数*/

void SM_OrderMake(SMTypeDef* member,uint8_t* orderPart,int size);//命令合成
void SM_AngleToPulse(SMTypeDef* member,float angle,int isReal);//计算脉冲
void SM_PulseToAngle(SMTypeDef* member,int pulse,int isReal);//计算角度
void SM_PulseToRate(SMTypeDef* member,int pulse,int isReal);//计算角速度
void SM_EnableControl(SMTypeDef* member);//电机使能控制 [地址 + 0xF3 + 0xAB + 使能状态 + 多机同步标志 + 校验字节]
//电机速度控制 [地址 + 0xF6 + 方向 + 速度 + 加速度 + 多机同步标志 + 校验字节]
void SM_RateControl(SMTypeDef* member,uint8_t direction,int rate,int acceleration);
//电机位置控制 [地址 + 0xFD + 方向 + 速度+ 加速度 + 脉冲数 + 相对/绝对模式标志(00/01) + 多机同步标志 + 校验字节]
void SM_SiteControl(SMTypeDef* member,int direction,int rate,int acceleration,int pulse,int mode);
//f1控制 [地址 + 0xF1 + 速度 + 加速度 + 运动模式 + 多机同步标志 + 校验字节]/Addr F1 SpeedHi SpeedLo Acc Mode Sync 6B
void SM_F1Control(SMTypeDef* member,int rate,int acceleration,int mode);
// FC 快速位置运动 [地址 + 0xFC + 脉冲数  + 校验字节]/Addr FC Pulse3 Pulse2 Pulse1 Pulse0 6B
void SM_FCControl(SMTypeDef* member,int pulse);
void SM_FFControl(SMTypeDef* member);//FF 多机同步触发 [地址 + 0xFF+0x66 + 校验字节]/Addr FF 66 6B
void SM_RealTimePositionRead(SMTypeDef* member);//36 读取实时位置 [Addr 36 6B]返回结构：Addr 36 Sign Pos3 Pos2 Pos1 Pos0 6B
void SM_MotorStatusRead(SMTypeDef* member);//3A 读取电机状态标志 [Addr 3A 6B]返回结构：Addr 3A Status 6B

void SM_Initial(SMTypeDef* member1/*,SMTypeDef* member2*/);//SM初始化
void SM_Stop(SMTypeDef* member);//SM停止 [地址 + 0xFE + 0x98 + 多机同步标志 + 校验字节]
void SM_InitialPlaceSet(SMTypeDef* member,int flag);//SM单圈零点设置 [地址 + 0x93 + 0x88 + 是否存储标志(01/00) + 校验字节]
void SM_InitialPlaceBack(SMTypeDef* member,int BackMode);//SM触发回零 [地址 + 0x9A + 回零模式(单圈/多圈碰撞/多圈开关) + 多机同步标志 + 校验字节]
void SM_InitialPlaceBackNow(SMTypeDef* member);//SM退出回零 [地址 + 0x9C + 0x48 + 校验字节]
void SM_PidRead(SMTypeDef* member);//SM读取pid [地址 + 0x21 + 校验字节]
/*
函数	发送命令	正常返回格式	返回长度
SM_EnableControl	Addr F3 AB Enable Sync 6B	Addr F3 02/E2/EE 6B	4
SM_RateControl	Addr F6 Dir Speed Acc Sync 6B	Addr F6 02/12/22/E2/EE 6B	4
SM_SiteControl	Addr FD Dir Speed Acc Pulse Mode Sync 6B	Addr FD 02/12/22/9F/E2/EE 6B	4
SM_F1Control	Addr F1 Speed Acc Mode Sync 6B	Addr F1 02/12/22/9F/E2/EE 6B	4
SM_FCControl	Addr FC Pulse 6B	Addr FC 02/12/22/9F/E2/EE 6B	4
SM_FFControl	应为 00 FF 66 6B 或 Addr FF 66 6B	Addr FF 02/E2/EE 6B	4
SM_RealTimePositionRead	Addr 36 6B	Addr 36 Sign Pos3 Pos2 Pos1 Pos0 6B	8
SM_MotorStatusRead	Addr 3A 6B	Addr 3A Status 6B	4
SM_MicrosteppingSet	Addr 84 8A Save Microstep 6B	Addr 84 02/E2/EE 6B	4
SM_Stop	Addr FE 98 Sync 6B	Addr FE 02/E2/EE 6B	4
SM_InitialPlaceSet	Addr 93 88 Save 6B	Addr 93 02/E2/EE 6B	4
SM_InitialPlaceBack	Addr 9A Mode Sync 6B	Addr 9A 02/12/9F/E2/EE 6B	4
SM_InitialPlaceBackNow	Addr 9C 48 6B	Addr 9C 02/E2/EE 6B	4
SM_PidRead	Addr 21 6B	Emm: Addr 21 Kp4 Ki4 Kd4 6B	15
SM_PidRead	Addr 21 6B	X: Addr 21 pTkp4 pBkp4 vkp4 vki4 6B	19
*/

/*CODE FUNCTION END*/



#endif /* __SM_H */
