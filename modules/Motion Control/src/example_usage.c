/**
 * @file example_usage.c
 * @brief 运动控制层使用示例
 * @date 2026-07-14
 * 
 * 展示如何集成运动控制层到实际项目中
 */

#include "motion_control.h"
#include "motion_config.h"

// 假设已有的底层驱动
#include "../../controller/mspm0/inc/motor.h"
#include "../../controller/mspm0/inc/encoder.h"
#include "../../controller/mspm0/inc/line_sensor.h"
// #include "icm42688.h"  // IMU驱动（需要实现）

/* ============================================================================
 * 硬件接口实现：循迹传感器
 * ============================================================================ */

static uint16_t hw_lineSensor_readMask(void) {
    return LineSensor_ReadMask();
}

static bool hw_lineSensor_getError(uint16_t mask, int16_t *error) {
    return LineSensor_GetError(mask, error);
}

static LineSensorInterface_t hw_lineSensorInterface = {
    .readMask = hw_lineSensor_readMask,
    .getError = hw_lineSensor_getError,
};

/* ============================================================================
 * 硬件接口实现：编码器
 * ============================================================================ */

static int32_t hw_encoder_getCount(EncoderId_t id) {
    return Encoder_GetCount((Encoder_Id)id);
}

static void hw_encoder_resetCount(EncoderId_t id) {
    Encoder_ResetCount((Encoder_Id)id);
}

static EncoderInterface_t hw_encoderInterface = {
    .getCount = hw_encoder_getCount,
    .resetCount = hw_encoder_resetCount,
};

/* ============================================================================
 * 硬件接口实现：IMU
 * ============================================================================ */

static bool hw_imu_readGyro(GyroData_t *data) {
    // TODO: 调用ICM42688驱动读取陀螺仪数据
    // return ICM42688_ReadGyro(&data->wx, &data->wy, &data->wz);
    
    // 临时实现：如果IMU未实现，返回零值
    data->wx = 0.0f;
    data->wy = 0.0f;
    data->wz = 0.0f;
    return true;  // 或返回false如果驱动未实现
}

static bool hw_imu_init(void) {
    // TODO: 初始化ICM42688
    // return ICM42688_Init();
    return true;
}

static ImuInterface_t hw_imuInterface = {
    .readGyro = hw_imu_readGyro,
    .init = hw_imu_init,
};

/* ============================================================================
 * 硬件接口实现：电机
 * ============================================================================ */

static void hw_motor_setDifferentialPWM(int16_t left, int16_t right) {
    Motor_SetSpeed(left, right);
}

static void hw_motor_stop(void) {
    Motor_Stop();
}

static void hw_motor_init(void) {
    Motor_Init();
}

static MotorInterface_t hw_motorInterface = {
    .setDifferentialPWM = hw_motor_setDifferentialPWM,
    .stop = hw_motor_stop,
    .init = hw_motor_init,
};

/* ============================================================================
 * 感知层和执行器层接口汇总
 * ============================================================================ */

static PerceptionInterface_t hw_perceptionInterface = {
    .lineSensor = &hw_lineSensorInterface,
    .encoder = &hw_encoderInterface,
    .imu = &hw_imuInterface,
};

static ActuatorInterface_t hw_actuatorInterface = {
    .motor = &hw_motorInterface,
};

/* ============================================================================
 * 运动控制器实例
 * ============================================================================ */

static MotionControl_t g_motionCtrl;

/* ============================================================================
 * 使用示例：初始化和配置
 * ============================================================================ */

void Example_MotionControl_Setup(void) {
    // 1. 初始化底层硬件
    Motor_Init();
    Encoder_Init();
    LineSensor_Init();
    // ICM42688_Init();  // 如果IMU驱动已实现
    
    // 2. 初始化运动控制层
    bool success = MotionControl_Init(&g_motionCtrl, 
                                     &hw_perceptionInterface,
                                     &hw_actuatorInterface);
    
    if (!success) {
        // 初始化失败处理
        while(1);  // 或者错误处理
    }
    
    // 3. 配置控制参数（可选，使用默认值也可以）
    MotionControl_SetBaseSpeed(&g_motionCtrl, 0.5f);  // 0.5 m/s
    
    // 可以根据需要微调PID参数
    // MotionControl_SetLateralPID(&g_motionCtrl, 0.3f, 0.0f, 0.15f);
    // MotionControl_SetOmegaPI(&g_motionCtrl, 0.8f, 0.1f);
    // MotionControl_SetSpeedPI(&g_motionCtrl, 200.0f, 50.0f);
    
    // 4. 启动控制
    MotionControl_Start(&g_motionCtrl);
}

/* ============================================================================
 * 使用示例：定时器中断（500Hz）
 * ============================================================================ */

void TIMG0_IRQHandler(void) {
    // 检查定时器中断标志
    if (DL_TimerG_getPendingInterrupt(TIMG0) & DL_TIMER_IIDX_ZERO) {
        // 调用运动控制更新（500Hz）
        MotionControl_Update(&g_motionCtrl);
    }
}

/* ============================================================================
 * 使用示例：主循环监控
 * ============================================================================ */

void Example_MainLoop_Monitor(void) {
    static uint32_t last_print_time = 0;
    uint32_t current_time = get_system_time_ms();  // 假设有系统时间函数
    
    // 每500ms打印一次状态
    if (current_time - last_print_time > 500) {
        last_print_time = current_time;
        
        // 获取控制状态
        ControlState_t state = MotionControl_GetState(&g_motionCtrl);
        int16_t error = MotionControl_GetLateralError(&g_motionCtrl);
        
        float v_left, v_right;
        MotionControl_GetWheelSpeed(&g_motionCtrl, &v_left, &v_right);
        
        uint32_t loop_count, max_exec_time;
        MotionControl_GetPerformance(&g_motionCtrl, &loop_count, &max_exec_time);
        
        // 通过串口输出（如果有）
        printf("State: %d, Error: %d, VL: %.2f, VR: %.2f, Loop: %lu, MaxT: %lu\n",
               state, error, v_left, v_right, loop_count, max_exec_time);
    }
}

/* ============================================================================
 * 使用示例：运行时参数调整
 * ============================================================================ */

void Example_RuntimeAdjustment(void) {
    // 示例：根据按键调整速度
    if (button_up_pressed()) {
        float current_speed;
        // 获取当前速度并增加
        MotionControl_SetBaseSpeed(&g_motionCtrl, current_speed + 0.1f);
    }
    
    if (button_down_pressed()) {
        float current_speed;
        MotionControl_SetBaseSpeed(&g_motionCtrl, current_speed - 0.1f);
    }
    
    // 示例：紧急停止
    if (emergency_button_pressed()) {
        MotionControl_EmergencyStop(&g_motionCtrl);
    }
    
    // 示例：重新启动
    if (start_button_pressed()) {
        if (MotionControl_GetState(&g_motionCtrl) == CONTROL_STATE_IDLE) {
            MotionControl_Start(&g_motionCtrl);
        }
    }
}

/* ============================================================================
 * 使用示例：完整的main函数模板
 * ============================================================================ */

int main(void) {
    // 1. 系统初始化
    SYSCFG_DL_init();  // TI SysConfig生成的初始化
    
    // 2. 运动控制层初始化
    Example_MotionControl_Setup();
    
    // 3. 使能全局中断
    NVIC_EnableIRQ(TIMG0_INT_IRQn);
    
    // 4. 主循环
    while (1) {
        // 监控和调试
        Example_MainLoop_Monitor();
        
        // 运行时参数调整
        Example_RuntimeAdjustment();
        
        // 其他任务（显示、通信等）
        // ...
        
        // 可以进入低功耗模式，等待中断唤醒
        // __WFI();
    }
}

/* ============================================================================
 * 使用示例：参数标定辅助函数
 * ============================================================================ */

/**
 * @brief 轮径标定：推车直线行驶并记录编码器数据
 */
void Example_CalibrateWheelRadius(void) {
    // 1. 复位编码器
    Encoder_ResetCount(ENCODER_LEFT_FRONT);
    
    // 2. 提示用户推车10米
    printf("Please push the robot 10 meters in a straight line...\n");
    delay_ms(5000);
    
    // 3. 读取编码器
    int32_t pulses = Encoder_GetCount(ENCODER_LEFT_FRONT);
    
    // 4. 计算轮径
    float wheel_radius = 10.0f / ((float)pulses / ENCODER_PPR) / (2.0f * 3.14159f);
    
    printf("Measured wheel radius: %.4f m\n", wheel_radius);
    printf("Update WHEEL_RADIUS in motion_config.h to: %.4f\n", wheel_radius);
}

/**
 * @brief 轮距标定：原地旋转360度
 */
void Example_CalibrateWheelBase(void) {
    // 1. 复位编码器
    Encoder_ResetCount(ENCODER_LEFT_FRONT);
    Encoder_ResetCount(ENCODER_RIGHT_FRONT);
    
    // 2. 提示用户旋转小车
    printf("Please rotate the robot 360 degrees in place...\n");
    delay_ms(5000);
    
    // 3. 读取编码器
    int32_t left_pulses = Encoder_GetCount(ENCODER_LEFT_FRONT);
    int32_t right_pulses = Encoder_GetCount(ENCODER_RIGHT_FRONT);
    
    // 4. 计算轮距
    float wheel_circumference = 2.0f * 3.14159f * WHEEL_RADIUS;
    float distance_diff = (float)(right_pulses - left_pulses) / ENCODER_PPR * wheel_circumference;
    float wheel_base = distance_diff / (2.0f * 3.14159f);
    
    printf("Measured wheel base: %.4f m\n", wheel_base);
    printf("Update WHEEL_BASE in motion_config.h to: %.4f\n", wheel_base);
}

/* ============================================================================
 * 使用示例：调试模式（单独测试各环路）
 * ============================================================================ */

/**
 * @brief 测试模式1：仅测试内环（轮速控制）
 */
void Example_TestInnerLoop(void) {
    // 禁用外环和中环，手动设置目标速度
    // 这需要修改motion_control.c或提供测试接口
    // 仅用于调试
}

/**
 * @brief 测试模式2：测试中环（角速度控制）
 */
void Example_TestMiddleLoop(void) {
    // 禁用外环，手动设置目标角速度
}

/**
 * @brief 测试模式3：完整系统测试
 */
void Example_TestFullSystem(void) {
    // 正常运行，记录所有数据用于分析
}
