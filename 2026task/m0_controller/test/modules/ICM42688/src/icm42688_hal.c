/**
 * @file      icm42688_hal.c
 * @brief     ICM42688 sensor HAL - platform-independent implementation
 * @note      No STM32 dependencies. All hardware access via injected callbacks.
 */

#include "icm42688_hal.h"
#include <math.h>
#include <string.h>

/* --------------------------------------------------------------------------
 * Static state - injected via icm42688_hal_init()
 * -------------------------------------------------------------------------- */

static const icm42688_comm_t   *p_comm   = NULL;
static const icm42688_system_t *p_system = NULL;
static const icm42688_config_t *p_config = NULL;
static bool initialized = false;

static float acc_factor  = 1.0f;   /* LSB-to-g conversion factor  */
static float gyro_factor = 1.0f;   /* LSB-to-dps conversion factor */

/* Calibrated gyro bias, subtracted in icm42688_gyro_transition() */
static float gyro_bias_x = 0.0f;
static float gyro_bias_y = 0.0f;
static float gyro_bias_z = 0.0f;

/* --------------------------------------------------------------------------
 * Helpers
 * -------------------------------------------------------------------------- */

/**
 * @brief  Decode a 16-bit big-endian value to signed int16.
 */
static int16_t decode_be16(const uint8_t *bytes)
{
    return (int16_t)(((uint16_t)bytes[0] << 8) | (uint16_t)bytes[1]);
}

/**
 * @brief  Check whether comm interface is fully bound.
 */
static bool comm_is_bound(void)
{
    return (p_comm != NULL)
        && (p_comm->init != NULL)
        && (p_comm->read_reg != NULL)
        && (p_comm->write_reg != NULL)
        && (p_comm->read_regs != NULL);
}

/**
 * @brief  Check whether system interface is fully bound.
 */
static bool system_is_bound(void)
{
    return (p_system != NULL)
        && (p_system->delay_ms != NULL);
}

/* --------------------------------------------------------------------------
 * Scale-factor tables (matching original driver)
 * -------------------------------------------------------------------------- */

static float acc_transition_factor(icm42688_acc_sample_t range)
{
    switch (range) {
        case ICM42688_ACC_SAMPLE_SGN_2G:  return  2.0f  / 32768.0f;
        case ICM42688_ACC_SAMPLE_SGN_4G:  return  4.0f  / 32768.0f;
        case ICM42688_ACC_SAMPLE_SGN_8G:  return  8.0f  / 32768.0f;
        case ICM42688_ACC_SAMPLE_SGN_16G: return 16.0f  / 32768.0f;
        default:                          return  8.0f  / 32768.0f;
    }
}

static float gyro_transition_factor(icm42688_gyro_sample_t range)
{
    switch (range) {
        case ICM42688_GYRO_SAMPLE_SGN_15_625DPS: return  15.625f / 32768.0f;
        case ICM42688_GYRO_SAMPLE_SGN_31_25DPS:  return  31.25f  / 32768.0f;
        case ICM42688_GYRO_SAMPLE_SGN_62_5DPS:   return  62.5f   / 32768.0f;
        case ICM42688_GYRO_SAMPLE_SGN_125DPS:    return 125.0f   / 32768.0f;
        case ICM42688_GYRO_SAMPLE_SGN_250DPS:    return 250.0f   / 32768.0f;
        case ICM42688_GYRO_SAMPLE_SGN_500DPS:    return 500.0f   / 32768.0f;
        case ICM42688_GYRO_SAMPLE_SGN_1000DPS:   return 1000.0f  / 32768.0f;
        case ICM42688_GYRO_SAMPLE_SGN_2000DPS:   return 2000.0f  / 32768.0f;
        default:                                 return 2000.0f  / 32768.0f;
    }
}

static bool acc_fs_sel(icm42688_acc_sample_t range, uint8_t *value)
{
    switch (range) {
        case ICM42688_ACC_SAMPLE_SGN_2G:  *value = 3U; return true;
        case ICM42688_ACC_SAMPLE_SGN_4G:  *value = 2U; return true;
        case ICM42688_ACC_SAMPLE_SGN_8G:  *value = 1U; return true;
        case ICM42688_ACC_SAMPLE_SGN_16G: *value = 0U; return true;
        default: return false;
    }
}

static bool gyro_fs_sel(icm42688_gyro_sample_t range, uint8_t *value)
{
    switch (range) {
        case ICM42688_GYRO_SAMPLE_SGN_15_625DPS: *value = 7U; return true;
        case ICM42688_GYRO_SAMPLE_SGN_31_25DPS:  *value = 6U; return true;
        case ICM42688_GYRO_SAMPLE_SGN_62_5DPS:   *value = 5U; return true;
        case ICM42688_GYRO_SAMPLE_SGN_125DPS:    *value = 4U; return true;
        case ICM42688_GYRO_SAMPLE_SGN_250DPS:    *value = 3U; return true;
        case ICM42688_GYRO_SAMPLE_SGN_500DPS:    *value = 2U; return true;
        case ICM42688_GYRO_SAMPLE_SGN_1000DPS:   *value = 1U; return true;
        case ICM42688_GYRO_SAMPLE_SGN_2000DPS:   *value = 0U; return true;
        default: return false;
    }
}

static bool sample_rate_odr(icm42688_sample_rate_t rate, uint8_t *value)
{
    switch (rate) {
        case ICM42688_SAMPLE_RATE_8000: *value = 0x03U; return true;
        case ICM42688_SAMPLE_RATE_4000: *value = 0x04U; return true;
        case ICM42688_SAMPLE_RATE_2000: *value = 0x05U; return true;
        case ICM42688_SAMPLE_RATE_1000: *value = 0x06U; return true;
        case ICM42688_SAMPLE_RATE_500:  *value = 0x0FU; return true;
        case ICM42688_SAMPLE_RATE_200:  *value = 0x07U; return true;
        case ICM42688_SAMPLE_RATE_100:  *value = 0x08U; return true;
        case ICM42688_SAMPLE_RATE_50:   *value = 0x09U; return true;
        case ICM42688_SAMPLE_RATE_25:   *value = 0x0AU; return true;
        case ICM42688_SAMPLE_RATE_12_5: *value = 0x0BU; return true;
        default: return false;
    }
}

/* --------------------------------------------------------------------------
 * Public: bind interfaces
 * -------------------------------------------------------------------------- */

void icm42688_hal_init(const icm42688_comm_t   *comm,
                       const icm42688_system_t *system,
                       const icm42688_config_t *config)
{
    p_comm   = comm;
    p_system = system;
    p_config = config;

    initialized = false;
    acc_factor = 0.0f;
    gyro_factor = 0.0f;
    gyro_bias_x = 0.0f;
    gyro_bias_y = 0.0f;
    gyro_bias_z = 0.0f;
}

/* --------------------------------------------------------------------------
 * Public: sensor init (reset, wake, configure, verify ID)
 * -------------------------------------------------------------------------- */

icm42688_status_t icm42688_init(void)
{
    uint8_t id;
    uint8_t accel_fs;
    uint8_t gyro_fs;
    uint8_t odr;

    initialized = false;

    if (!comm_is_bound() || !system_is_bound() || (p_config == NULL)) {
        return ICM42688_STATUS_NOT_READY;
    }
    if (p_config->interface_type != ICM42688_INTERFACE_SPI ||
        !acc_fs_sel(p_config->acc_sample, &accel_fs) ||
        !gyro_fs_sel(p_config->gyro_sample, &gyro_fs) ||
        !sample_rate_odr(p_config->sample_rate, &odr)) {
        return ICM42688_STATUS_INVALID_ARGUMENT;
    }

    /* Init hardware interface */
    p_comm->init();
    p_system->delay_ms(10);

    /* Verify sensor ID */
    id = p_comm->read_reg(ICM42688_WHO_AM_I);
    if (id != ICM42688_ID) {
        return ICM42688_STATUS_BAD_ID;
    }

    /* Soft reset */
    p_comm->write_reg(ICM42688_DEVICE_CONFIG, 0x01);
    p_system->delay_ms(50);

    /* Wake sensor (enable accel + gyro) */
    p_comm->write_reg(ICM42688_PWR_MGMT0, 0x0F);
    p_system->delay_ms(1);

    /* Configure scale factors */
    acc_factor  = acc_transition_factor(p_config->acc_sample);
    gyro_factor = gyro_transition_factor(p_config->gyro_sample);

    /* Set range and ODR for accel and gyro */
    p_comm->write_reg(ICM42688_ACCEL_CONFIG0,
                      (uint8_t)((accel_fs << 5) | odr));
    p_comm->write_reg(ICM42688_GYRO_CONFIG0,
                      (uint8_t)((gyro_fs << 5) | odr));

    /* Low-pass filter config */
    p_comm->write_reg(ICM42688_GYRO_CONFIG1,  0x01);
    p_comm->write_reg(ICM42688_ACCEL_CONFIG1, 0x01);

    p_system->delay_ms(5);

    initialized = true;
    return ICM42688_STATUS_OK;
}

/* --------------------------------------------------------------------------
 * Public: read 14 bytes from TEMP_DATA1
 * -------------------------------------------------------------------------- */

icm42688_status_t icm42688_read(icm42688_data_t *data)
{
    uint8_t buf[14];

    if (!comm_is_bound()) {
        return ICM42688_STATUS_NOT_READY;
    }
    if (data == NULL) {
        return ICM42688_STATUS_INVALID_ARGUMENT;
    }

    /* 14-byte burst: TEMP_DATA1 (0x1D) + 2 temp + 6 accel + 6 gyro */
    p_comm->read_regs(ICM42688_TEMP_DATA1, buf, 14);

    /* Big-endian decode */
    data->temperature_raw = decode_be16(&buf[0]);   /* TEMP_DATA1/DATA0  */
    data->acc_raw.x       = decode_be16(&buf[2]);   /* ACCEL_X1/X0       */
    data->acc_raw.y       = decode_be16(&buf[4]);   /* ACCEL_Y1/Y0       */
    data->acc_raw.z       = decode_be16(&buf[6]);   /* ACCEL_Z1/Z0       */
    data->gyro_raw.x      = decode_be16(&buf[8]);   /* GYRO_X1/X0        */
    data->gyro_raw.y      = decode_be16(&buf[10]);  /* GYRO_Y1/Y0        */
    data->gyro_raw.z      = decode_be16(&buf[12]);  /* GYRO_Z1/Z0        */

    /* Convert to physical units */
    data->acc_g.x    = (float)data->acc_raw.x  * acc_factor;
    data->acc_g.y    = (float)data->acc_raw.y  * acc_factor;
    data->acc_g.z    = (float)data->acc_raw.z  * acc_factor;
    data->gyro_dps.x = (float)data->gyro_raw.x * gyro_factor - gyro_bias_x;
    data->gyro_dps.y = (float)data->gyro_raw.y * gyro_factor - gyro_bias_y;
    data->gyro_dps.z = (float)data->gyro_raw.z * gyro_factor - gyro_bias_z;

    return ICM42688_STATUS_OK;
}

icm42688_status_t icm42688_get_scale_factors(float *accel_g_per_lsb,
                                              float *gyro_dps_per_lsb)
{
    if (accel_g_per_lsb == NULL || gyro_dps_per_lsb == NULL) {
        return ICM42688_STATUS_INVALID_ARGUMENT;
    }
    if (!initialized) {
        return ICM42688_STATUS_NOT_READY;
    }
    *accel_g_per_lsb = acc_factor;
    *gyro_dps_per_lsb = gyro_factor;
    return ICM42688_STATUS_OK;
}

icm42688_status_t icm42688_get_gyro_bias(icm42688_vector3f_t *bias_dps)
{
    if (bias_dps == NULL) {
        return ICM42688_STATUS_INVALID_ARGUMENT;
    }
    if (!initialized) {
        return ICM42688_STATUS_NOT_READY;
    }
    bias_dps->x = gyro_bias_x;
    bias_dps->y = gyro_bias_y;
    bias_dps->z = gyro_bias_z;
    return ICM42688_STATUS_OK;
}

/* --------------------------------------------------------------------------
 * Public: gyro calibration (average N samples while stationary)
 * -------------------------------------------------------------------------- */

icm42688_status_t icm42688_calibrate_gyro(uint16_t samples,
                                          uint16_t interval_ms)
{
    icm42688_data_t data;
    double sum_x = 0.0, sum_y = 0.0, sum_z = 0.0;

    if (samples == 0) {
        return ICM42688_STATUS_INVALID_ARGUMENT;
    }
    if (!comm_is_bound() || !system_is_bound()) {
        return ICM42688_STATUS_NOT_READY;
    }

    for (uint16_t i = 0; i < samples; i++) {
        icm42688_status_t st;

        /* Temporarily remove bias so calibration average is unbiased */
        float save_bx = gyro_bias_x;
        float save_by = gyro_bias_y;
        float save_bz = gyro_bias_z;
        gyro_bias_x = 0.0f;
        gyro_bias_y = 0.0f;
        gyro_bias_z = 0.0f;

        st = icm42688_read(&data);

        gyro_bias_x = save_bx;
        gyro_bias_y = save_by;
        gyro_bias_z = save_bz;

        if (st != ICM42688_STATUS_OK) {
            return st;
        }

        sum_x += (double)data.gyro_raw.x * (double)gyro_factor;
        sum_y += (double)data.gyro_raw.y * (double)gyro_factor;
        sum_z += (double)data.gyro_raw.z * (double)gyro_factor;

        if (interval_ms > 0) {
            p_system->delay_ms(interval_ms);
        }
    }

    gyro_bias_x = (float)(sum_x / (double)samples);
    gyro_bias_y = (float)(sum_y / (double)samples);
    gyro_bias_z = (float)(sum_z / (double)samples);

    return ICM42688_STATUS_OK;
}

/* --------------------------------------------------------------------------
 * Public: raw-to-physical unit conversions
 * -------------------------------------------------------------------------- */

float icm42688_acc_transition(int16_t raw)
{
    return (float)raw * acc_factor;
}

float icm42688_gyro_transition(int16_t raw)
{
    return (float)raw * gyro_factor;
}
