/**
 * @file      test_icm42688.c
 * @brief     Unit tests for platform-independent ICM42688 HAL
 * @note      Fake SPI/delay layer for host-compiled testing
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

/* SUT header - will fail to compile until inc/icm42688_hal.h exists */
#include "icm42688_hal.h"

/* --------------------------------------------------------------------------
 * Fake hardware - these simulate the sensor's register bank and SPI bus
 * -------------------------------------------------------------------------- */

static uint8_t registers[256];
static uint8_t read_buffer[14];
static uint8_t write_reg_log[8];
static uint8_t write_value_log[8];
static size_t   write_count;

static icm42688_comm_t   fake_comm;
static icm42688_system_t fake_system;
static icm42688_config_t fake_config;

static uint8_t fake_read_reg(uint8_t reg)
{
    return registers[reg];
}

static void fake_write_reg(uint8_t reg, uint8_t value)
{
    write_reg_log[write_count]   = reg;
    write_value_log[write_count] = value;
    write_count++;
}

static void fake_read_regs(uint8_t reg, uint8_t *data, uint8_t len)
{
    (void)reg;
    memcpy(data, read_buffer, len);
}

static void fake_delay_ms(uint16_t ms) { (void)ms; }
static void fake_init(void)            { }

static void bind_default_fakes(void)
{
    memset(registers, 0, sizeof(registers));
    memset(read_buffer, 0, sizeof(read_buffer));
    memset(write_reg_log, 0, sizeof(write_reg_log));
    memset(write_value_log, 0, sizeof(write_value_log));
    write_count = 0;

    fake_comm.init      = fake_init;
    fake_comm.read_reg  = fake_read_reg;
    fake_comm.write_reg = fake_write_reg;
    fake_comm.read_regs = fake_read_regs;

    fake_system.delay_ms = fake_delay_ms;

    fake_config.interface_type = ICM42688_INTERFACE_SPI;
    fake_config.acc_sample     = ICM42688_ACC_SAMPLE_SGN_8G;      /* ±8 g   */
    fake_config.gyro_sample    = ICM42688_GYRO_SAMPLE_SGN_1000DPS; /* ±1000 dps */
    fake_config.sample_rate    = ICM42688_SAMPLE_RATE_1000;       /* 1000 Hz */

    icm42688_hal_init(&fake_comm, &fake_system, &fake_config);
}

/* --------------------------------------------------------------------------
 * Test: unbound (NULL) interface pointers
 * -------------------------------------------------------------------------- */

static void test_init_fails_null_comm(void)
{
    icm42688_comm_t comm;
    memset(&comm, 0, sizeof(comm));
    icm42688_hal_init(&comm, &fake_system, &fake_config);
    icm42688_status_t st = icm42688_init();
    assert(st == ICM42688_STATUS_NOT_READY);
}

static void test_init_fails_null_func_ptr(void)
{
    icm42688_comm_t comm = { NULL, NULL, NULL, NULL };
    icm42688_hal_init(&comm, &fake_system, &fake_config);
    icm42688_status_t st = icm42688_init();
    assert(st == ICM42688_STATUS_NOT_READY);
}

/* --------------------------------------------------------------------------
 * Test: bad WHO_AM_I
 * -------------------------------------------------------------------------- */

static void test_init_rejects_wrong_id(void)
{
    registers[ICM42688_WHO_AM_I] = 0x00U;
    bind_default_fakes();
    assert(icm42688_init() == ICM42688_STATUS_BAD_ID);
}

/* --------------------------------------------------------------------------
 * Test: verify register writes during init
 * -------------------------------------------------------------------------- */

static void test_init_writes_device_config_reset(void)
{
    bind_default_fakes();
    registers[ICM42688_WHO_AM_I] = ICM42688_ID;
    assert(icm42688_init() == ICM42688_STATUS_OK);

    /* Must write DEVICE_CONFIG = 0x01 (soft-reset pulse) */
    int found = 0;
    for (size_t i = 0; i < write_count; i++) {
        if (write_reg_log[i] == ICM42688_DEVICE_CONFIG
            && write_value_log[i] == 0x01U) {
            found = 1;
            break;
        }
    }
    assert(found);
}

static void test_init_writes_pwr_mgmt0_wake(void)
{
    bind_default_fakes();
    registers[ICM42688_WHO_AM_I] = ICM42688_ID;
    assert(icm42688_init() == ICM42688_STATUS_OK);

    int found = 0;
    for (size_t i = 0; i < write_count; i++) {
        if (write_reg_log[i] == ICM42688_PWR_MGMT0
            && write_value_log[i] == 0x0FU) {
            found = 1;
            break;
        }
    }
    assert(found);
}

/* --------------------------------------------------------------------------
 * Test: big-endian decode and scale-factor conversion
 * -------------------------------------------------------------------------- */

static void test_big_endian_sample_and_scale(void)
{
    icm42688_data_t data;

    bind_default_fakes();
    registers[ICM42688_WHO_AM_I] = ICM42688_ID;

    /* Set up read_buffer after bind_default_fakes clears it */
    const uint8_t sample[] = {
        0x40, 0x00,  /* acc x = +16384 -> +4.0 g @ +/-8 g      */
        0xC0, 0x00,  /* acc y = -16384 -> -4.0 g @ +/-8 g      */
        0x20, 0x00,  /* acc z =  +8192 -> +2.0 g             */
        0x10, 0x00,  /* gyr x =  +4096 -> +125.0 dps @ +/-1000 */
        0xF0, 0x00,  /* gyr y =  -4096 -> -125.0 dps         */
        0x08, 0x00   /* gyr z =  +2048 ->  +62.5 dps         */
    };
    memcpy(read_buffer, sample, sizeof(sample));

    assert(icm42688_init() == ICM42688_STATUS_OK);
    assert(icm42688_read(&data) == ICM42688_STATUS_OK);

    /* Accel in g */
    assert(fabsf(data.acc_g.x - 4.0f)   < 0.001f);
    assert(fabsf(data.acc_g.y + 4.0f)   < 0.001f);
    assert(fabsf(data.acc_g.z - 2.0f)   < 0.001f);

    /* Gyro in dps */
    assert(fabsf(data.gyro_dps.x - 125.0f) < 0.001f);
    assert(fabsf(data.gyro_dps.y + 125.0f) < 0.001f);
    assert(fabsf(data.gyro_dps.z - 62.5f)  < 0.001f);
}

/* --------------------------------------------------------------------------
 * Test: read() with NULL data pointer
 * -------------------------------------------------------------------------- */

static void test_read_null_data_fails(void)
{
    bind_default_fakes();
    registers[ICM42688_WHO_AM_I] = ICM42688_ID;
    assert(icm42688_init() == ICM42688_STATUS_OK);
    assert(icm42688_read(NULL) == ICM42688_STATUS_INVALID_ARGUMENT);
}

/* --------------------------------------------------------------------------
 * Test: read() with unbound interface
 * -------------------------------------------------------------------------- */

static void test_read_unbound_fails(void)
{
    icm42688_comm_t comm = { NULL, NULL, NULL, NULL };
    icm42688_hal_init(&comm, &fake_system, &fake_config);
    assert(icm42688_read(NULL) == ICM42688_STATUS_NOT_READY);
}

/* --------------------------------------------------------------------------
 * Test: gyro calibration - zero samples
 * -------------------------------------------------------------------------- */

static void test_calibrate_gyro_zero_samples(void)
{
    bind_default_fakes();
    registers[ICM42688_WHO_AM_I] = ICM42688_ID;
    assert(icm42688_init() == ICM42688_STATUS_OK);
    assert(icm42688_calibrate_gyro(0, 10) == ICM42688_STATUS_INVALID_ARGUMENT);
}

/* --------------------------------------------------------------------------
 * Test: gyro calibration with zero-rate data
 * -------------------------------------------------------------------------- */

static void test_calibrate_gyro_zero(void)
{
    /* All 12 bytes zero → zero-rate output */
    memset(read_buffer, 0, 12);
    bind_default_fakes();
    registers[ICM42688_WHO_AM_I] = ICM42688_ID;
    assert(icm42688_init() == ICM42688_STATUS_OK);

    icm42688_status_t st = icm42688_calibrate_gyro(10, 0);
    assert(st == ICM42688_STATUS_OK);

    /* After calibration, a single read should return ~0 dps on all axes */
    icm42688_data_t data;
    assert(icm42688_read(&data) == ICM42688_STATUS_OK);
    assert(fabsf(data.gyro_dps.x) < 0.001f);
    assert(fabsf(data.gyro_dps.y) < 0.001f);
    assert(fabsf(data.gyro_dps.z) < 0.001f);
}

/* ==========================================================================
 * Runner
 * ========================================================================== */

typedef struct {
    const char *name;
    void       (*func)(void);
} test_case_t;

int main(void)
{
    test_case_t tests[] = {
        { "null comm pointer",          test_init_fails_null_comm },
        { "null function pointers",     test_init_fails_null_func_ptr },
        { "wrong WHO_AM_I",             test_init_rejects_wrong_id },
        { "DEVICE_CONFIG reset",        test_init_writes_device_config_reset },
        { "PWR_MGMT0 wake",             test_init_writes_pwr_mgmt0_wake },
        { "big-endian scale convert",   test_big_endian_sample_and_scale },
        { "read NULL data",             test_read_null_data_fails },
        { "read unbound interface",     test_read_unbound_fails },
        { "calibrate zero samples",     test_calibrate_gyro_zero_samples },
        { "calibrate gyro zero",        test_calibrate_gyro_zero },
    };

    int ntests = (int)(sizeof(tests) / sizeof(tests[0]));
    for (int i = 0; i < ntests; i++) {
        tests[i].func();
        printf("  PASS  %s\n", tests[i].name);
    }

    printf("test_icm42688: PASS\n");
    return 0;
}
