/**
 * @file sensor_adapter.c
 * @brief Sensor HAL adapter for Sens-Decision - implementation
 * @date 2026-07-18
 */

#include "sensor_adapter.h"
#include "encoder_hw_bridge.h"
#include "../modules/ICM42688/inc/icm42688_hal.h"
#include "../modules/MCP23017/inc/mcp23017.h"

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

/**
 * @brief Read IR sensor mask
 * @param active_mask Output pointer for 12-bit IR mask
 * @return SD_OK on success, error codes on failure
 * 
 * @note Converts MCP23017 status codes:
 *       - MCP23017_STATUS_OK -> SD_OK
 *       - MCP23017_STATUS_TIMEOUT -> SD_ERR_TIMEOUT
 *       - Other failures -> SD_ERR_READ
 *       Masks result to 12 bits (0x0FFF).
 */
static sd_status_t ReadIr(uint16_t *active_mask)
{
    if (active_mask == NULL) {
        return SD_ERR_INVALID_ARGUMENT;
    }
    
    uint16_t raw_inputs;
    mcp23017_status_t status = MCP23017_ReadInputs(&raw_inputs);
    
    switch (status) {
        case MCP23017_STATUS_OK:
            /* Mask to 12 bits for IR channels */
            *active_mask = raw_inputs & 0x0FFF;
            return SD_OK;
            
        case MCP23017_STATUS_TIMEOUT:
            return SD_ERR_TIMEOUT;
            
        case MCP23017_STATUS_INVALID_ARGUMENT:
            return SD_ERR_INVALID_ARGUMENT;
            
        case MCP23017_STATUS_IO_ERROR:
        default:
            return SD_ERR_READ;
    }
}

/* Static HAL instance */
static const sensor_hal_t g_sensor_hal = {
    .read_encoder_count = ReadEncoder,
    .read_imu_raw = ReadImu,
    .read_ir_mask = ReadIr
};

const sensor_hal_t *SensorAdapter_GetHal(void)
{
    return &g_sensor_hal;
}
