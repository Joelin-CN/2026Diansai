/**
 * @file imu_debug.c
 * @brief IMU调试工具实现 - 测试ICM42688数据读取和AHRS解算
 * @date 2026-07-29
 *
 * 功能：
 * 1. 测试ICM42688 WHO_AM_I寄存器读取
 * 2. 读取6轴原始数据（加速度计+陀螺仪）
 * 3. 显示转换后的物理量（g和dps）
 * 4. 运行AHRS Mahony算法解算姿态角（Roll/Pitch/Yaw）
 * 5. 从debug串口输出所有数据
 */

#include "imu_debug.h"
#include "icm42688_hal.h"
#include "icm42688_stm32.h"
#include "ahrs_hal.h"
#include "platform_time.h"
#include "spi_diagnostic.h"
#include "cmsis_os.h"
#include <stdio.h>
#include <math.h>

/* ============================================================================
 * 常量定义
 * ============================================================================ */

#define DEG_TO_RAD  0.017453292519943295f
#define RAD_TO_DEG  57.29577951308232f

/* ============================================================================
 * 静态变量
 * ============================================================================ */

static bool s_initialized = false;
static uint32_t s_last_print_time_ms = 0;

/* AHRS定时器接口 */
static uint32_t _ahrs_get_time_us(void) {
    return (uint32_t)PlatformTime_GetUs64();
}

static void _ahrs_timer_init(void) {
    /* PlatformTime已在外部初始化 */
}

static void _ahrs_timer_start(void) {
    /* 无需额外操作 */
}

static const ahrs_timer_t s_ahrs_timer = {
    .get_time_us = _ahrs_get_time_us,
    .init        = _ahrs_timer_init,
    .start       = _ahrs_timer_start
};

/* ============================================================================
 * 公共函数实现
 * ============================================================================ */

bool ImuDebug_Init(void) {
    printf("\n");
    printf("========================================\n");
    printf("    ICM42688 IMU Debug Tool v1.0\n");
    printf("========================================\n\n");

    /* 初始化平台时间 */
    printf("[1/5] Initializing platform timer...\n");
    PlatformTime_Init();

    /* 配置ICM42688 */
    printf("[2/5] Binding ICM42688 STM32 adapter...\n");
    const icm42688_config_t imu_config = {
        .interface_type = ICM42688_INTERFACE_SPI,
        .acc_sample     = ICM42688_ACC_SAMPLE_SGN_8G,      // ±8g量程
        .gyro_sample    = ICM42688_GYRO_SAMPLE_SGN_1000DPS, // ±1000dps量程
        .sample_rate    = ICM42688_SAMPLE_RATE_1000         // 1kHz采样率
    };
    icm42688_stm32_bind(&imu_config);

    /* 初始化ICM42688（包含WHO_AM_I验证） */
    printf("[3/5] Initializing ICM42688 sensor...\n");

    /* 运行详细的SPI通信诊断 */
    SpiDiag_TestICM42688();

    printf("\n[3/5] Continuing with ICM42688 initialization...\n");
    icm42688_status_t status = icm42688_init();
    if (status != ICM42688_STATUS_OK) {
        printf("[ERROR] ICM42688 init failed! Status: %d\n", status);
        if (status == 2) {  // ICM42688_STATUS_BAD_ID
            printf("  WHO_AM_I mismatch detected.\n");
            printf("  Possible causes:\n");
            printf("    1. Wrong sensor model (not ICM42688)\n");
            printf("    2. SPI communication error\n");
            printf("    3. Sensor not powered\n");
        }
        printf("  - Check SPI2 connection (SCK/MISO/MOSI)\n");
        printf("  - Check CS pin connection (PE7)\n");
        printf("  - Check power supply (3.3V)\n");
        return false;
    }
    printf("  ICM42688 WHO_AM_I check: OK (0x47)\n");

    /* 陀螺仪零偏标定 */
    printf("[4/5] Calibrating gyro bias (keep sensor still)...\n");
    status = icm42688_calibrate_gyro(100, 10);  // 100样本，每样本间隔10ms
    if (status != ICM42688_STATUS_OK) {
        printf("[WARNING] Gyro calibration failed! Status: %d\n", status);
        printf("  Continuing without calibration...\n");
    } else {
        icm42688_vector3f_t gyro_bias;
        icm42688_get_gyro_bias(&gyro_bias);
        printf("  Gyro bias: X=%.3f Y=%.3f Z=%.3f dps\n",
               gyro_bias.x, gyro_bias.y, gyro_bias.z);
    }

    /* 初始化AHRS */
    printf("[5/5] Initializing AHRS (Mahony 6-axis)...\n");
    ahrs_hal_init(&s_ahrs_timer);
    ahrs_init();

    printf("\n");
    printf("========================================\n");
    printf("  IMU Debug Ready!\n");
    printf("========================================\n\n");
    printf("Data format:\n");
    printf("  ACC_RAW: [x y z] (int16_t, LSB)\n");
    printf("  ACC_G:   [x y z] (float, g)\n");
    printf("  GYRO_RAW:[x y z] (int16_t, LSB)\n");
    printf("  GYRO_DPS:[x y z] (float, deg/s)\n");
    printf("  TEMP:    (float, °C)\n");
    printf("  EULER:   Roll/Pitch/Yaw (float, deg)\n");
    printf("\n");

    s_initialized = true;
    s_last_print_time_ms = osKernelGetTickCount();

    return true;
}

void ImuDebug_Run(uint32_t print_interval_ms) {
    if (!s_initialized) {
        printf("[ERROR] ImuDebug not initialized!\n");
        return;
    }

    /* 检查是否到打印时间 */
    uint32_t current_time_ms = osKernelGetTickCount();
    if ((current_time_ms - s_last_print_time_ms) < print_interval_ms) {
        return;
    }
    s_last_print_time_ms = current_time_ms;

    /* 读取ICM42688数据 */
    icm42688_data_t imu_data;
    icm42688_status_t status = icm42688_read(&imu_data);

    if (status != ICM42688_STATUS_OK) {
        printf("[ERROR] ICM42688 read failed! Status: %d\n", status);
        return;
    }

    /* 更新AHRS（将dps转为rad/s） */
    ahrs_update_6axis(
        imu_data.gyro_dps.x * DEG_TO_RAD,
        imu_data.gyro_dps.y * DEG_TO_RAD,
        imu_data.gyro_dps.z * DEG_TO_RAD,
        imu_data.acc_g.x,
        imu_data.acc_g.y,
        imu_data.acc_g.z
    );

    /* 获取姿态角 */
    ahrs_euler_angle_t euler;
    ahrs_get_attitude(&euler);

    /* 温度转换（来自ICM42688数据手册） */
    float temp_c = (float)imu_data.temperature_raw / 132.48f + 25.0f;

    /* 打印所有数据 */
    printf("========================================\n");
    printf("Timestamp: %lu ms\n", current_time_ms);
    printf("----------------------------------------\n");

    /* 原始数据 */
    printf("ACC_RAW:  [%6d, %6d, %6d]\n",
           imu_data.acc_raw.x,
           imu_data.acc_raw.y,
           imu_data.acc_raw.z);

    printf("GYRO_RAW: [%6d, %6d, %6d]\n",
           imu_data.gyro_raw.x,
           imu_data.gyro_raw.y,
           imu_data.gyro_raw.z);

    printf("TEMP_RAW: %d\n", imu_data.temperature_raw);
    printf("----------------------------------------\n");

    /* 转换后的物理量 */
    printf("ACC_G:    [%7.3f, %7.3f, %7.3f] g\n",
           imu_data.acc_g.x,
           imu_data.acc_g.y,
           imu_data.acc_g.z);

    printf("GYRO_DPS: [%7.2f, %7.2f, %7.2f] deg/s\n",
           imu_data.gyro_dps.x,
           imu_data.gyro_dps.y,
           imu_data.gyro_dps.z);

    printf("TEMP:     %.2f °C\n", temp_c);
    printf("----------------------------------------\n");

    /* AHRS解算结果 */
    printf("EULER:    Roll=%.2f  Pitch=%.2f  Yaw=%.2f deg\n",
           euler.roll,
           euler.pitch,
           euler.yaw);

    /* 计算加速度模长（用于验证传感器是否正常） */
    float acc_magnitude = sqrtf(
        imu_data.acc_g.x * imu_data.acc_g.x +
        imu_data.acc_g.y * imu_data.acc_g.y +
        imu_data.acc_g.z * imu_data.acc_g.z
    );
    printf("ACC_MAG:  %.3f g (should be ~1.0 when still)\n", acc_magnitude);

    printf("========================================\n\n");
}
