#include "app_motor.h"

static float speed_lr = 0;
static float speed_fb = 0;
static float speed_spin = 0;

static int speed_L1_setup = 0;
static int speed_L2_setup = 0;
static int speed_R1_setup = 0;
static int speed_R2_setup = 0;

static int g_offset_yaw = 0;
static uint16_t g_speed_setup = 0;


// 编码器10毫秒前后数据
int g_Encoder_All_Now[Motor_MAX] = {0};
int g_Encoder_All_Last[Motor_MAX] = {0};
int g_Encoder_All_Offset[Motor_MAX] = {0};


uint8_t g_start_ctrl = 0;

car_data_t car_data;
motor_data_t motor_data;

uint8_t g_yaw_adjust = 0;

static float Motion_Get_Circle_Pulse(void)
{  
    return ENCODER_CIRCLE_450;
}

// 仅用于添加到调试中显示数据。
void* Motion_Get_Data(uint8_t index)
{
    if (index == 1) return (int*)g_Encoder_All_Now;
    if (index == 2) return (int*)g_Encoder_All_Last;
    if (index == 3) return (int*)g_Encoder_All_Offset;
    return 0;
}

// 获取电机速度
void Motion_Get_Motor_Speed(float* speed)
{
    for (int i = 0; i < 4; i++)
    {
        speed[i] = motor_data.speed_mm_s[i];
        
    }
}


// 设置偏航角状态，如果使能则刷新target目标角度。
void Motion_Set_Yaw_Adjust(uint8_t adjust)
{
    if (adjust == 0)
    {
        g_yaw_adjust = 0;
    }
    else
    {
        g_yaw_adjust = 1;
    }
    if (g_yaw_adjust)
    {
     //PID_Yaw_Reset(获取当前IMU偏航角-yaw);  
    }
}

// 返回偏航角调节状态。
uint8_t Motion_Get_Yaw_Adjust(void)
{
    return g_yaw_adjust;
}

// 控制小车运动，Motor_X=[-3600, 3600]，超过范围则无效。
void Motion_Set_Pwm(int16_t Motor_1, int16_t Motor_2, int16_t Motor_3, int16_t Motor_4)
{
    if (Motor_1 >= -MOTOR_MAX_PULSE && Motor_1 <= MOTOR_MAX_PULSE)
    {
        Motor_Set_Pwm(Motor_M1, Motor_1);
    }
    if (Motor_2 >= -MOTOR_MAX_PULSE && Motor_2 <= MOTOR_MAX_PULSE)
    {
        Motor_Set_Pwm(Motor_M2, Motor_2);
    }
    if (Motor_3 >= -MOTOR_MAX_PULSE && Motor_3 <= MOTOR_MAX_PULSE)
    {
        Motor_Set_Pwm(Motor_M3, Motor_3);
    }
    if (Motor_4 >= -MOTOR_MAX_PULSE && Motor_4 <= MOTOR_MAX_PULSE)
    {
        Motor_Set_Pwm(Motor_M4, Motor_4);
    }
}

// 小车停止
void Motion_Stop(uint8_t brake)
{
    Motion_Set_Speed(0, 0, 0, 0);
    PID_Clear_Motor(Motor_MAX);
    g_start_ctrl = 0;
    g_yaw_adjust = 0;
    Motor_Stop(brake);
}


// speed_mX=[-1000, 1000], 单位为：mm/s
void Motion_Set_Speed(int16_t speed_m1, int16_t speed_m2, int16_t speed_m3, int16_t speed_m4)
{
    g_start_ctrl = 1;
    motor_data.speed_set[0] = speed_m1;
    motor_data.speed_set[1] = speed_m2;
    motor_data.speed_set[2] = speed_m3;
    motor_data.speed_set[3] = speed_m4;
    for (uint8_t i = 0; i < Motor_MAX; i++)
    {
        PID_Set_Motor_Target(i, motor_data.speed_set[i]*1.0);
    }
}

// 增加偏航角校准小车运动方向
void Motion_Yaw_Calc(float yaw)
{
    Wheel_Yaw_Calc(yaw);
}

void Wheel_Yaw_Calc(float yaw)
{
    float yaw_offset = PID_Yaw_Calc(yaw);
    g_offset_yaw = yaw_offset * g_speed_setup;

    int speed_L1 = speed_L1_setup - g_offset_yaw;
    int speed_L2 = speed_L2_setup - g_offset_yaw;
    int speed_R1 = speed_R1_setup + g_offset_yaw;
    int speed_R2 = speed_R2_setup + g_offset_yaw;

   
        if (speed_L1 > 1000) speed_L1 = 1000;
        if (speed_L1 < -1000) speed_L1 = -1000;
        if (speed_L2 > 1000) speed_L2 = 1000;
        if (speed_L2 < -1000) speed_L2 = -1000;
        if (speed_R1 > 1000) speed_R1 = 1000;
        if (speed_R1 < -1000) speed_R1 = -1000;
        if (speed_R2 > 1000) speed_R2 = 1000;
        if (speed_R2 < -1000) speed_R2 = -1000;
    Motion_Set_Speed(speed_L1, speed_L2, speed_R1, speed_R2);
}

// 从编码器读取当前各轮子速度，单位mm/s
void Motion_Get_Speed(car_data_t* car)
{
    int i = 0;
    float speed_mm[Motor_MAX] = {0};
    float circle_mm = Motion_Get_Circle_MM();
    float circle_pulse = Motion_Get_Circle_Pulse();
    float robot_APB = Motion_Get_APB();

    Motion_Get_Encoder();

    // 计算轮子速度，单位mm/s。
    for (i = 0; i < 4; i++)
    {
        speed_mm[i] = (g_Encoder_All_Offset[i]) * 100 * circle_mm / circle_pulse;
    }
		
		 car->Vx = (speed_mm[0] + speed_mm[1] + speed_mm[2] + speed_mm[3]) / 4;
     car->Vy = 0;
     car->Vz = -(speed_mm[0] + speed_mm[1] - speed_mm[2] - speed_mm[3]) / 4.0f / robot_APB * 1000;
		
		if (g_start_ctrl)
    {
        for (i = 0; i < Motor_MAX; i++)
        {
            motor_data.speed_mm_s[i] = speed_mm[i];
        }
        
        #if ENABLE_YAW_ADJUST && ENABLE_MPU6050
        if (g_yaw_adjust)
        {
           //Motion_Yaw_Calc(imu的yaw角);
        }
        #endif
        PID_Calc_Motor(&motor_data);
		}
}

// 返回当前小车轮子轴间距和的一半
float Motion_Get_APB(void)
{
    return STM32Car_APB;
}

// 返回当前小车轮子转一圈的多少毫米
float Motion_Get_Circle_MM(void)
{
		return MECANUM_CIRCLE_MM; 
}

// 获取编码器数据，并计算偏差脉冲数
void Motion_Get_Encoder(void)
{
    Encoder_Get_ALL(g_Encoder_All_Now);

    for(uint8_t i = 0; i < Motor_MAX; i++)
    {
        // 记录两次测试时间差的脉冲数
        g_Encoder_All_Offset[i] = g_Encoder_All_Now[i] - g_Encoder_All_Last[i];
	    // 记录上次编码器数据
	    g_Encoder_All_Last[i] = g_Encoder_All_Now[i];
    
    }
}

// 控制小车运动
void Motion_Ctrl(int16_t V_x, int16_t V_y, int16_t V_z, uint8_t adjust)
{
	wheel_Ctrl(V_x, V_y, V_z, adjust);
}

void Motion_Ctrl_State(uint8_t state, uint16_t speed, uint8_t adjust)
{
	uint16_t input_speed = speed * 10;
	wheel_State(state, input_speed, adjust);
}






// 控制麦克纳姆轮小车运动状态。 
// 速度控制：speed=0~1000。
// 偏航角调节运动：adjust=1开启，=0不开启。
void wheel_State(uint8_t state, uint16_t speed, uint8_t adjust)
{
    Motion_Set_Yaw_Adjust(adjust);
    g_speed_setup = speed;
    switch (state)
    {
    case MOTION_STOP:
        g_speed_setup = 0;
        Motion_Stop(speed==0?STOP_FREE:STOP_BRAKE);
        break;
    case MOTION_RUN:
        wheel_Ctrl(speed, 0, 0, adjust);
        break;
    case MOTION_BACK:
        wheel_Ctrl(-speed, 0, 0, adjust);
        break;
    case MOTION_LEFT:
        wheel_Ctrl(speed/2, 0, -speed*2, adjust);
        break;
    case MOTION_RIGHT:
        wheel_Ctrl(speed/2, 0, speed*2, adjust);
        break;
    case MOTION_SPIN_LEFT:
        Motion_Set_Yaw_Adjust(0);
        wheel_Ctrl(0, 0, speed*5, 0);
        break;
    case MOTION_SPIN_RIGHT:
        Motion_Set_Yaw_Adjust(0);
        wheel_Ctrl(0, 0, -speed*5, 0);
        break;
    case MOTION_BRAKE:
        Motion_Stop(STOP_BRAKE);
        break;
    default:
        break;
    }
}

void wheel_Ctrl(int16_t V_x, int16_t V_y, int16_t V_z, uint8_t adjust)
{
    float robot_APB = Motion_Get_APB();
//    speed_lr = -V_y;
		speed_lr = 0;
    speed_fb = V_x;
    speed_spin = (V_z / 1000.0f) * robot_APB;
    if (V_x == 0 && V_y == 0 && V_z == 0)
    {
        Motion_Stop(STOP_BRAKE);
        return;
    }

    speed_L1_setup = speed_fb + speed_lr + speed_spin;
    speed_L2_setup = speed_fb - speed_lr + speed_spin;
    speed_R1_setup = speed_fb - speed_lr - speed_spin;
    speed_R2_setup = speed_fb + speed_lr - speed_spin;
		
		if (speed_L1_setup > 1000) speed_L1_setup = 1000;
		if (speed_L1_setup < -1000) speed_L1_setup = -1000;
		if (speed_L2_setup > 1000) speed_L2_setup = 1000;
		if (speed_L2_setup < -1000) speed_L2_setup = -1000;
		if (speed_R1_setup > 1000) speed_R1_setup = 1000;
		if (speed_R1_setup < -1000) speed_R1_setup = -1000;
		if (speed_R2_setup > 1000) speed_R2_setup = 1000;
		if (speed_R2_setup < -1000) speed_R2_setup = -1000;
		
		//printf("%d\t,%d\t,%d\t,%d\r\n",speed_L1_setup,speed_L2_setup,speed_R1_setup,speed_R2_setup);
		
		Motion_Set_Speed(speed_L1_setup, speed_L2_setup, speed_R1_setup, speed_R2_setup);
		
	}
		
		


// 运动控制句柄，每10ms调用一次，主要处理速度相关的数据
void Motion_Handle(void)
{
    Motion_Get_Speed(&car_data);

    if (g_start_ctrl)
    {
        Motion_Set_Pwm(motor_data.speed_pwm[0], motor_data.speed_pwm[1], motor_data.speed_pwm[2], motor_data.speed_pwm[3]);
    }
}
		

