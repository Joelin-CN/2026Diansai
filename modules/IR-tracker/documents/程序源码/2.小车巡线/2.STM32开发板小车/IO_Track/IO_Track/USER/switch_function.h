#ifndef __SWITCH_FUNCTION_H
#define __SWITCH_FUNCTION_H


#define DEBUG_USARTx USART1 //调试串口定义

//0:关闭  1：开启
#define ENABLE_YAW_ADJUST 0 //小车yaw角调整开关
#define ENABLE_MPU6050   	0 //MPU6050开关  最好不要和太多的传感器一起用

#define ENABLE_VOL				0 //电压检测
#define ENABLE_LIHGTSEEK  0 //板载光敏电阻开关
#define W25Q64_Flash      0 //板载外部flash开关

#define ENABLE_PWMServo   0 //PWMServo开关
#define ENABLE_MOTOR   		1 //电机开关

#define ENABLE_OLED       0 //oled开关
#define ENABLE_ws281x    	0 //RGB灯条开关  V1.2版的硬件没适配

#define ENABLE_Ultrasonic 0 //超声波模块开关
#define ENABLE_IRavoid    0 //红外避障开关

#define ENABLE_IRTRACK    0 //红外循迹开关
#define ENABLE_IRRPID     0 //红外PID循迹

#define ENABLE_IRREMOTE   0 //红外遥控开关
#define ENABLE_PS2  		  0 //ps2手柄遥控开关

#define ENABLE_ELE			  0 //电磁循迹开关
#define ENABLE_CCD			  0 //CCD传感器开关

#define ENABLE_X3RADAR		0 //雷达开关



//4路舵机的开关 ,注释定义即可关闭
#define USE_SERVO_J1
#define USE_SERVO_J2
#define USE_SERVO_J3
#define USE_SERVO_J4

//3个按键的开关，,注释定义即可关闭
#define  KEY1_USER
#define  KEY2_USER
#define  KEY3_USER




#endif

