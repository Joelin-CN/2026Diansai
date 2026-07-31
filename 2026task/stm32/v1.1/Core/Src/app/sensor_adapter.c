/**
 * @file sensor_adapter.c
 * @brief 传感器适配器实现（STM32平台）
 * @date 2026-07-29
 */

#include "sensor_adapter.h"
#include "encoder_hw_bridge.h"
#include "icm42688_hal.h"
#include "ir_uart_sensor.h"
#include "config.h"

/*
 * Sens-Decision uses logical indices 0=left and 1=right, while the assembled
 * PCB wiring is raw0/TIM3=C/right and raw1/TIM4=B/left. Swap at the platform
 * boundary so odometry and encoder-derived yaw keep logical left/right order.
 */
static const uint8_t s_encoder_raw_index[2] = {
    1U,  /* logical left  -> TIM4/raw 1 */
    0U   /* logical right -> TIM3/raw 0 */
};

static sd_status_t read_encoder_count(uint8_t index, int32_t *count) {
    if (index >= 2U || count == NULL) return SD_ERR_INVALID_ARGUMENT;
    *count = EncoderHwBridge_GetCount(s_encoder_raw_index[index]);
    return SD_OK;
}

static sd_status_t read_imu_raw(imu_raw_data_t *data) {
    if (data == NULL) return SD_ERR_INVALID_ARGUMENT;

    icm42688_data_t icm_data;
    icm42688_status_t status = icm42688_read(&icm_data);

    if (status != ICM42688_STATUS_OK) {
        return SD_ERR_READ;
    }

    data->accel[0] = icm_data.acc_raw.x;
    data->accel[1] = icm_data.acc_raw.y;
    data->accel[2] = icm_data.acc_raw.z;
    data->gyro[0] = icm_data.gyro_raw.x;
    data->gyro[1] = icm_data.gyro_raw.y;
    data->gyro[2] = icm_data.gyro_raw.z;
    data->temperature = icm_data.temperature_raw;

    return SD_OK;
}

/**
 * @brief 读取红外传感器阵列数据
 *
 * @param active_mask [out] 传感器通道激活掩码（当前始终为0xFFFF，表示8通道全部有效）
 * @param values [out] 8通道红外传感器原始数值数组（0-4095范围）
 * @return sd_status_t 状态码：SD_OK=成功, SD_ERR_READ=无数据, SD_ERR_INVALID_ARGUMENT=参数错误
 *
 * @note 关键修复 (2026-07-30):
 *       必须先调用 IrUartSensor_Process() 驱动帧解析器。
 *
 * @details 驱动层设计模式（中断缓冲 + 轮询解析）:
 *       1. 中断层(USART2_IRQHandler): 字节追加到 g_frame_buffer[], 设置 g_frame_ready
 *       2. 解析层(IrUartSensor_Process): 主循环轮询，帧校验并解包到 g_analog[]
 *       3. 读取层(IrUartSensor_GetAnalog): 返回已解析数据（检查 g_analog_valid）
 *
 * @warning 如果不调用 IrUartSensor_Process():
 *       - 中断接收的字节永远停留在缓冲区
 *       - g_analog_valid 永远为 false
 *       - IrUartSensor_GetAnalog() 永远返回 false
 *       - 导致感知层无数据（SD_ERR_READ）
 *
 * @see BUGFIX_lateral_error_verification.md 第3.2节 - 本修复的详细说明
 * @see logs/2026-07-30_ir_sensor_fix_implementation.md - 完整修复方案
 */
static sd_status_t read_ir(uint16_t *active_mask, float values[SD_IR_CHANNEL_COUNT]) {
    if (active_mask == NULL || values == NULL) return SD_ERR_INVALID_ARGUMENT;

    /*
     * Require a newly parsed frame.  Returning the previous g_analog[] forever
     * makes a disconnected UART look healthy and defeats the line-loss stop.
     * The sensor streams at 125 Hz while this function runs at 50 Hz, so a
     * fresh frame should normally be available on every call.
     */
    ir_uart_sensor_status_t process_status = IrUartSensor_Process();
    if (process_status != IR_UART_SENSOR_STATUS_OK) {
        return SD_ERR_READ;
    }

    uint16_t raw_values[SD_IR_CHANNEL_COUNT];
    if (!IrUartSensor_GetAnalog(raw_values)) {
        return SD_ERR_READ;  // 无有效帧数据
    }

    *active_mask = 0xFFFF;  // 假设所有8个通道都有效
    for (uint8_t i = 0; i < SD_IR_CHANNEL_COUNT; i++) {
        values[i] = (float)raw_values[i];
    }

    return SD_OK;
}

static sensor_hal_t s_hal = {
    .read_encoder_count = read_encoder_count,
    .read_imu_raw       = read_imu_raw,
    .read_ir            = read_ir,
};

sensor_hal_t *SensorAdapter_GetInterface(void) {
    return &s_hal;
}
