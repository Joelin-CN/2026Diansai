/**
 * @file sensor_adapter.c
 * @brief Sensor HAL adapter for Sens-Decision - implementation
 * @date 2026-07-18
 */

#include "sensor_adapter.h"
#include "encoder_hw_bridge.h"
#include "../modules/ICM42688/inc/icm42688_hal.h"
#include "../modules/IR-tracker/inc/ir_uart_sensor.h"

/**
 * @brief Read encoder count via HAL
 * @param index Encoder index (0-3)
 * @param count Output pointer for encoder count
 * @return SD_OK on success, SD_ERR_INVALID_ARGUMENT if invalid
 */
static sd_status_t ReadEncoder(uint8_t index, int32_t *count)
{
    if (count == NULL || index >= 4) {
        return SD_ERR_INVALID_ARGUMENT;
    }

    *count = EncoderHwBridge_GetCount(index);
    return SD_OK;
}

/**
 * @brief Read IMU raw data
 * @param data Output pointer for IMU raw data
 * @return SD_OK on success, SD_ERR_READ on failure, SD_ERR_INVALID_ARGUMENT if NULL
 *
 * @note Only copies raw LSB values (temperature_raw, acc_raw, gyro_raw).
 *       Does NOT copy converted values (acc_g, gyro_dps).
 */
static sd_status_t ReadImu(imu_raw_data_t *data)
{
    if (data == NULL) {
        return SD_ERR_INVALID_ARGUMENT;
    }

    icm42688_data_t icm_data;
    icm42688_status_t status = icm42688_read(&icm_data);

    if (status != ICM42688_STATUS_OK) {
        return SD_ERR_READ;
    }

    /* Copy only raw LSB data */
    data->accel[0] = icm_data.acc_raw.x;
    data->accel[1] = icm_data.acc_raw.y;
    data->accel[2] = icm_data.acc_raw.z;
    data->gyro[0] = icm_data.gyro_raw.x;
    data->gyro[1] = icm_data.gyro_raw.y;
    data->gyro[2] = icm_data.gyro_raw.z;
    data->temperature = icm_data.temperature_raw;

    return SD_OK;
}

#define IR_ANALOG_FULL_SCALE 4096.0f
#define IR_LINE_THRESHOLD    0.5f

static float    g_ir_values[SD_IR_CHANNEL_COUNT];
static uint16_t g_ir_mask = 0U;
static bool     g_ir_valid = false;

/**
 * @brief Read IR sensor array (analog graded)
 * @param active_mask Output: bitmask of channels above line threshold
 * @param values Output: normalized reflectance per channel [0..1]
 * @return SD_OK on success, SD_ERR_INVALID_ARGUMENT if NULL, SD_ERR_READ if no frame
 *
 * @note Pulls analog samples from the IR-tracker UART driver, normalizes to
 *       reflectance (1 - raw/4096), derives a binary mask at IR_LINE_THRESHOLD.
 *       Returns SD_ERR_READ during module warm-up until first valid frame.
 */
static sd_status_t ReadIr(uint16_t *active_mask, float *values)
{
    uint8_t i;

    if (active_mask == NULL || values == NULL) {
        return SD_ERR_INVALID_ARGUMENT;
    }

    if (IrUartSensor_Process() == IR_UART_SENSOR_STATUS_OK) {
        uint16_t raw[IR_UART_SENSOR_COUNT];
        if (IrUartSensor_GetAnalog(raw)) {
            uint16_t mask = 0U;
            for (i = 0U; i < IR_UART_SENSOR_COUNT; ++i) {
                float s = 1.0f - (float)raw[i] / IR_ANALOG_FULL_SCALE;
                if (s < 0.0f) s = 0.0f;
                if (s > 1.0f) s = 1.0f;
                g_ir_values[i] = s;
                if (s > IR_LINE_THRESHOLD) mask |= (uint16_t)(1U << i);
            }
            g_ir_mask = mask;
            g_ir_valid = true;
        }
    }

    if (!g_ir_valid) {
        return SD_ERR_READ;
    }

    *active_mask = g_ir_mask;
    for (i = 0U; i < SD_IR_CHANNEL_COUNT; ++i) {
        values[i] = g_ir_values[i];
    }
    return SD_OK;
}

#if (IR_UART_SENSOR_COUNT != SD_IR_CHANNEL_COUNT)
#error "IR channel count mismatch: IR_UART_SENSOR_COUNT != SD_IR_CHANNEL_COUNT"
#endif

/* Static HAL instance */
static const sensor_hal_t g_sensor_hal = {
    .read_encoder_count = ReadEncoder,
    .read_imu_raw = ReadImu,
    .read_ir = ReadIr
};

const sensor_hal_t *SensorAdapter_GetHal(void)
{
    return &g_sensor_hal;
}
