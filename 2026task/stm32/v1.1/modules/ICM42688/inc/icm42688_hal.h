/**
 * @file      icm42688_hal.h
 * @brief     ICM42688 sensor Hardware Abstraction Layer - platform-independent
 * @author    AzureHalcyon (original STM32), ported to portable C99 HAL
 * @date      2025-03-28 (original), 2026-07-17 (port)
 * @note      This header is 100% platform-independent. All hardware dependencies
 *            are injected via callback-interface structs.
 */

#ifndef _ICM42688_HAL_H_
#define _ICM42688_HAL_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ======================================================================
 * Register Map
 * ====================================================================== */

#define ICM42688_DEVICE_CONFIG             0x11
#define ICM42688_DRIVE_CONFIG              0x13
#define ICM42688_INT_CONFIG                0x14
#define ICM42688_FIFO_CONFIG               0x16
#define ICM42688_TEMP_DATA1                0x1D
#define ICM42688_TEMP_DATA0                0x1E
#define ICM42688_ACCEL_DATA_X1             0x1F
#define ICM42688_ACCEL_DATA_X0             0x20
#define ICM42688_ACCEL_DATA_Y1             0x21
#define ICM42688_ACCEL_DATA_Y0             0x22
#define ICM42688_ACCEL_DATA_Z1             0x23
#define ICM42688_ACCEL_DATA_Z0             0x24
#define ICM42688_GYRO_DATA_X1              0x25
#define ICM42688_GYRO_DATA_X0              0x26
#define ICM42688_GYRO_DATA_Y1              0x27
#define ICM42688_GYRO_DATA_Y0              0x28
#define ICM42688_GYRO_DATA_Z1              0x29
#define ICM42688_GYRO_DATA_Z0              0x2A
#define ICM42688_INT_STATUS                0x2D
#define ICM42688_PWR_MGMT0                 0x4E
#define ICM42688_GYRO_CONFIG0              0x4F
#define ICM42688_ACCEL_CONFIG0             0x50
#define ICM42688_GYRO_CONFIG1              0x51
#define ICM42688_ACCEL_CONFIG1             0x53
#define ICM42688_WHO_AM_I                  0x75
#define ICM42688_REG_BANK_SEL              0x76

#define ICM42688_ID                        0x47

/* ======================================================================
 * Enums (retained from original header)
 * ====================================================================== */

typedef enum {
    ICM42688_STATUS_OK = 0,
    ICM42688_STATUS_NOT_READY,
    ICM42688_STATUS_BAD_ID,
    ICM42688_STATUS_INVALID_ARGUMENT
} icm42688_status_t;

typedef enum {
    ICM42688_INTERFACE_SPI,
    ICM42688_INTERFACE_IIC
} icm42688_interface_type_t;

typedef enum {
    ICM42688_ACC_SAMPLE_SGN_2G,
    ICM42688_ACC_SAMPLE_SGN_4G,
    ICM42688_ACC_SAMPLE_SGN_8G,
    ICM42688_ACC_SAMPLE_SGN_16G
} icm42688_acc_sample_t;

typedef enum {
    ICM42688_GYRO_SAMPLE_SGN_15_625DPS,
    ICM42688_GYRO_SAMPLE_SGN_31_25DPS,
    ICM42688_GYRO_SAMPLE_SGN_62_5DPS,
    ICM42688_GYRO_SAMPLE_SGN_125DPS,
    ICM42688_GYRO_SAMPLE_SGN_250DPS,
    ICM42688_GYRO_SAMPLE_SGN_500DPS,
    ICM42688_GYRO_SAMPLE_SGN_1000DPS,
    ICM42688_GYRO_SAMPLE_SGN_2000DPS
} icm42688_gyro_sample_t;

typedef enum {
    ICM42688_SAMPLE_RATE_8000,
    ICM42688_SAMPLE_RATE_4000,
    ICM42688_SAMPLE_RATE_2000,
    ICM42688_SAMPLE_RATE_1000,
    ICM42688_SAMPLE_RATE_200,
    ICM42688_SAMPLE_RATE_100,
    ICM42688_SAMPLE_RATE_50,
    ICM42688_SAMPLE_RATE_25,
    ICM42688_SAMPLE_RATE_12_5,
    ICM42688_SAMPLE_RATE_500
} icm42688_sample_rate_t;

/* ======================================================================
 * Data types
 * ====================================================================== */

typedef struct {
    int16_t x;
    int16_t y;
    int16_t z;
} icm42688_axis_data_t;

typedef struct {
    float x;
    float y;
    float z;
} icm42688_vector3f_t;

typedef struct {
    int16_t              temperature_raw;
    icm42688_axis_data_t acc_raw;
    icm42688_axis_data_t gyro_raw;
    icm42688_vector3f_t  acc_g;
    icm42688_vector3f_t  gyro_dps;
} icm42688_data_t;

typedef struct {
    icm42688_interface_type_t interface_type;
    icm42688_acc_sample_t     acc_sample;
    icm42688_gyro_sample_t    gyro_sample;
    icm42688_sample_rate_t    sample_rate;
} icm42688_config_t;

/* ======================================================================
 * Callback interface structs (platform-independent injection)
 * ====================================================================== */

typedef struct {
    void     (*init)(void);
    uint8_t  (*read_reg)(uint8_t reg);
    void     (*write_reg)(uint8_t reg, uint8_t value);
    void     (*read_regs)(uint8_t reg, uint8_t *data, uint8_t len);
} icm42688_comm_t;

typedef struct {
    void (*delay_ms)(uint16_t ms);
} icm42688_system_t;

/* ======================================================================
 * Public API
 * ====================================================================== */

/**
 * @brief  Bind communication, system, and configuration interfaces.
 * @note   Must be called before icm42688_init().
 */
void icm42688_hal_init(const icm42688_comm_t   *comm,
                       const icm42688_system_t *system,
                       const icm42688_config_t *config);

/**
 * @brief  Initialize the sensor: reset, wake, configure, verify ID.
 * @return ICM42688_STATUS_OK on success.
 */
icm42688_status_t icm42688_init(void);

/**
 * @brief  Read 14 bytes from TEMP_DATA1 in one burst: temperature, accelerometer,
 *         and gyroscope data. Fills temperature_raw, acc_raw, gyro_raw, acc_g,
 *         and gyro_dps.
 * @param  data  Pointer to output struct.
 * @return ICM42688_STATUS_OK on success.
 */
icm42688_status_t icm42688_read(icm42688_data_t *data);

/**
 * @brief  Average N gyro reads while stationary to compute bias.
 * @param  samples     Number of samples to average (>0).
 * @param  interval_ms Delay between samples in ms (0 = no extra delay).
 * @return ICM42688_STATUS_OK on success.
 */
icm42688_status_t icm42688_calibrate_gyro(uint16_t samples,
                                          uint16_t interval_ms);

icm42688_status_t icm42688_get_scale_factors(float *accel_g_per_lsb,
                                              float *gyro_dps_per_lsb);
icm42688_status_t icm42688_get_gyro_bias(icm42688_vector3f_t *bias_dps);

/**
 * @brief  Convert raw ADC reading to acceleration in g.
 */
float icm42688_acc_transition(int16_t raw);

/**
 * @brief  Convert raw ADC reading to angular rate in dps.
 */
float icm42688_gyro_transition(int16_t raw);

#ifdef __cplusplus
}
#endif

#endif /* _ICM42688_HAL_H_ */
