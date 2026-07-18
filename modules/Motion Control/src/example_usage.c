/**
 * @file example_usage.c
 * @brief 运动控制层使用示例 - 轨迹跟踪模式
 * @date 2026-07-14
 *
 * 展示如何将 Sens-Decision 的输出接入 Motion Control 执行层
 * 架构:
 *   Sens-Decision → trajectory_point_t → Motion Control → PWM
 */

#include "motion_control.h"
#include "motion_config.h"

/* 假设已有的底层驱动 */
#include "../../controller/mspm0/inc/motor.h"
#include "../../controller/mspm0/inc/encoder.h"

/* Sens-Decision 输出类型 */
typedef struct {
    float x, y, theta;
    float v, omega;
    float a, alpha;
    float curvature;
} trajectory_point_t;

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
    .getCount   = hw_encoder_getCount,
    .resetCount = hw_encoder_resetCount,
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
    .stop               = hw_motor_stop,
    .init               = hw_motor_init,
};

/* ============================================================================
 * 运动控制器实例
 * ============================================================================ */

static MotionControl_t g_motionCtrl;

/* ============================================================================
 * 使用示例：初始化和配置
 * ============================================================================ */

void Example_MotionControl_Setup(void) {
    /* 1. 初始化底层硬件 */
    Motor_Init();
    Encoder_Init();

    /* 2. 初始化运动控制层（只需编码器和电机接口） */
    bool success = MotionControl_Init(&g_motionCtrl,
                                      &hw_encoderInterface,
                                      &hw_motorInterface);
    if (!success) {
        while (1); /* 初始化失败处理 */
    }

    /* 3. 运行时调参（可选，默认值在 motion_config.h 中） */
    /* 调参方法: 在 motion_config.h 中修改 #define 值，重新编译即可 */
    // MotionControl_SetSpeedPI(&g_motionCtrl, 200.0f, 50.0f);
    // MotionControl_SetFeedforward(&g_motionCtrl, 50.0f, 300.0f, 80.0f);

    /* 4. 设置初始速度指令（启动前设置） */
    MotionControl_SetVelocityCommand(&g_motionCtrl, 0.0f, 0.0f);

    /* 5. 启动控制 */
    MotionControl_Start(&g_motionCtrl);
}

/* ============================================================================
 * 使用示例：定时器中断（500Hz）- 高频执行层
 * ============================================================================ */

void TIMG0_IRQHandler(void) {
    /* 检查定时器中断标志 */
    if (DL_TimerG_getPendingInterrupt(TIMG0) & DL_TIMER_IIDX_ZERO) {
        /* 调用运动控制更新（500Hz）*/
        MotionControl_Update(&g_motionCtrl);
    }
}

/* ============================================================================
 * 使用示例：主循环（10-20Hz）- 低频决策层
 * ============================================================================ */

/* 假设 Sens-Decision 提供以下函数 */
extern trajectory_point_t SensDecision_GetTrajectory(void);

void Example_MainLoop(void) {
    static uint32_t last_decision_time = 0;
    uint32_t current_time = get_system_time_ms();

    /* 每20ms（50Hz）更新一次决策指令 */
    if (current_time - last_decision_time >= 20) {
        last_decision_time = current_time;

        /* 1. 从 Sens-Decision 获取轨迹点 */
        trajectory_point_t traj = SensDecision_GetTrajectory();

        /* 2. 传递给 Motion Control 执行 */
        MotionControl_SetVelocityCommand(&g_motionCtrl, traj.v, traj.omega);
    }

    /* 3. 其他任务（显示、通信等） */
    /* ... */
}

/* ============================================================================
 * 使用示例：完整的 main 函数模板
 * ============================================================================ */

int main(void) {
    /* 1. 系统初始化 */
    SYSCFG_DL_init(); /* TI SysConfig 生成的初始化 */

    /* 2. 运动控制层初始化 */
    Example_MotionControl_Setup();

    /* 3. 使能全局中断 */
    NVIC_EnableIRQ(TIMG0_INT_IRQn);

    /* 4. 主循环 */
    while (1) {
        Example_MainLoop();

        /* 可以进入低功耗模式，等待中断唤醒 */
        /* __WFI(); */
    }
}

/* ============================================================================
 * 使用示例：运行时参数调整
 * ============================================================================ */

void Example_RuntimeAdjustment(void) {
    /* 示例：根据按键调整轮速 PI 参数 */
    if (button_up_pressed()) {
        MotionControl_SetSpeedPI(&g_motionCtrl, 250.0f, 60.0f);
    }

    /* 示例：调整前馈参数 */
    if (button_down_pressed()) {
        MotionControl_SetFeedforward(&g_motionCtrl, 60.0f, 350.0f, 90.0f);
    }

    /* 示例：紧急停止 */
    if (emergency_button_pressed()) {
        MotionControl_EmergencyStop(&g_motionCtrl);
    }

    /* 示例：重新启动（从紧急停止恢复） */
    if (start_button_pressed()) {
        ControlState_t state = MotionControl_GetState(&g_motionCtrl);
        if (state == CONTROL_STATE_EMERGENCY) {
            /* 先手动停止回到 IDLE，再重新启动 */
            MotionControl_Stop(&g_motionCtrl);
            MotionControl_SetVelocityCommand(&g_motionCtrl, 0.5f, 0.0f);
            MotionControl_Start(&g_motionCtrl);
        }
    }
}

/* ============================================================================
 * 使用示例：监控和调试输出
 * ============================================================================ */

void Example_Monitor(void) {
    static uint32_t last_print_time = 0;
    uint32_t current_time = get_system_time_ms();

    /* 每500ms打印一次状态 */
    if (current_time - last_print_time > 500) {
        last_print_time = current_time;

        ControlState_t state = MotionControl_GetState(&g_motionCtrl);

        float v_left, v_right;
        MotionControl_GetWheelSpeed(&g_motionCtrl, &v_left, &v_right);

        float target_left, target_right;
        MotionControl_GetTargetWheelSpeed(&g_motionCtrl, &target_left, &target_right);

        uint32_t loop_count, max_exec_time;
        MotionControl_GetPerformance(&g_motionCtrl, &loop_count, &max_exec_time);

        printf("State:%d | V_actual(L,R): %.2f,%.2f | V_target(L,R): %.2f,%.2f | Loop:%lu\n",
               state, v_left, v_right, target_left, target_right, loop_count);
    }
}

/* ============================================================================
 * 使用示例：参数标定辅助函数
 * ============================================================================ */

/**
 * @brief 轮径标定
 */
void Example_CalibrateWheelRadius(void) {
    Encoder_ResetCount(ENCODER_LEFT_FRONT);

    printf("Please push the robot 10 meters in a straight line...\n");
    delay_ms(5000);

    int32_t pulses = Encoder_GetCount(ENCODER_LEFT_FRONT);
    float wheel_radius = 10.0f / ((float)pulses / ENCODER_PPR) / (2.0f * 3.14159f);

    printf("Measured wheel radius: %.4f m\n", wheel_radius);
    printf("Update WHEEL_RADIUS in motion_config.h to: %.4f\n", wheel_radius);
}

/**
 * @brief 轮距标定
 */
void Example_CalibrateWheelBase(void) {
    Encoder_ResetCount(ENCODER_LEFT_FRONT);
    Encoder_ResetCount(ENCODER_RIGHT_FRONT);

    printf("Please rotate the robot 360 degrees in place...\n");
    delay_ms(5000);

    int32_t left_pulses  = Encoder_GetCount(ENCODER_LEFT_FRONT);
    int32_t right_pulses = Encoder_GetCount(ENCODER_RIGHT_FRONT);

    float wheel_circumference = 2.0f * 3.14159f * WHEEL_RADIUS;
    float distance_diff = (float)(right_pulses - left_pulses) / ENCODER_PPR * wheel_circumference;
    float wheel_base = distance_diff / (2.0f * 3.14159f);

    printf("Measured wheel base: %.4f m\n", wheel_base);
    printf("Update WHEEL_BASE in motion_config.h to: %.4f\n", wheel_base);
}

/**
 * @brief 前馈参数标定 - 静摩擦
 */
void Example_CalibrateStaticFriction(void) {
    /* 从 PWM_START_MIN (80) 开始逐次增加，直到电机刚好转动 */
    int16_t start_pwm = 0;
    while (start_pwm < 200) {
        Motor_SetSpeed(start_pwm, start_pwm);
        delay_ms(100);
        /* 检查编码器是否有脉冲增量，有则说明已开始转动 */
        int32_t count = Encoder_GetCount(ENCODER_LEFT_FRONT);
        Encoder_ResetCount(ENCODER_LEFT_FRONT);
        if (count != 0) break;
        start_pwm += 5;
    }
    printf("Static friction PWM threshold: %d\n", start_pwm);
    printf("Update FF_K_STATIC in motion_config.h to: %d\n", start_pwm);
}
