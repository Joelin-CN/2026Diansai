/**
 * @file      test_icm42688.c
 * @brief     Host tests for ICM42688 HAL (temperature + accel + gyro)
 * @date      2026-07-18
 */

#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>

#include "../modules/ICM42688/inc/icm42688_hal.h"

/* ======================================================================
 * Mock hardware state
 * ====================================================================== */

static uint8_t mock_registers[256];
static uint8_t mock_who_am_i_value = ICM42688_ID;

/* ======================================================================
 * Mock communication interface
 * ====================================================================== */

static void mock_comm_init(void)
{
    /* No-op for host test */
}

static uint8_t mock_read_reg(uint8_t reg)
{
    if (reg == ICM42688_WHO_AM_I) {
        return mock_who_am_i_value;
    }
    return mock_registers[reg];
}

static void mock_write_reg(uint8_t reg, uint8_t value)
{
    mock_registers[reg] = value;
}

static void mock_read_regs(uint8_t reg, uint8_t *data, uint8_t len)
{
    for (uint8_t i = 0; i < len; i++) {
        data[i] = mock_registers[reg + i];
    }
}

static const icm42688_comm_t mock_comm = {
    .init      = mock_comm_init,
    .read_reg  = mock_read_reg,
    .write_reg = mock_write_reg,
    .read_regs = mock_read_regs,
};

/* ======================================================================
 * Mock system interface
 * ====================================================================== */

static void mock_delay_ms(uint16_t ms)
{
    (void)ms;  /* No-op for host test */
}

static const icm42688_system_t mock_system = {
    .delay_ms = mock_delay_ms,
};

/* ======================================================================
 * Test helpers
 * ====================================================================== */

/**
 * @brief  Encode signed int16 as big-endian into mock registers
 */
static void encode_be16(uint8_t reg, int16_t value)
{
    mock_registers[reg]     = (uint8_t)((value >> 8) & 0xFF);
    mock_registers[reg + 1] = (uint8_t)(value & 0xFF);
}

/**
 * @brief  Setup mock registers with 14-byte burst data:
 *         TEMP_DATA1 (0x1D) = 2 bytes temp + 6 bytes accel + 6 bytes gyro
 */
static void setup_14byte_burst(int16_t temp, int16_t ax, int16_t ay, int16_t az,
                                int16_t gx, int16_t gy, int16_t gz)
{
    encode_be16(ICM42688_TEMP_DATA1,    temp);  /* 0x1D-0x1E */
    encode_be16(ICM42688_ACCEL_DATA_X1, ax);    /* 0x1F-0x20 */
    encode_be16(ICM42688_ACCEL_DATA_Y1, ay);    /* 0x21-0x22 */
    encode_be16(ICM42688_ACCEL_DATA_Z1, az);    /* 0x23-0x24 */
    encode_be16(ICM42688_GYRO_DATA_X1,  gx);    /* 0x25-0x26 */
    encode_be16(ICM42688_GYRO_DATA_Y1,  gy);    /* 0x27-0x28 */
    encode_be16(ICM42688_GYRO_DATA_Z1,  gz);    /* 0x29-0x2A */
}

/* ======================================================================
 * Tests
 * ====================================================================== */

static void test_temperature_reading(void)
{
    icm42688_config_t config = {
        .interface_type = ICM42688_INTERFACE_SPI,
        .acc_sample     = ICM42688_ACC_SAMPLE_SGN_8G,
        .gyro_sample    = ICM42688_GYRO_SAMPLE_SGN_2000DPS,
        .sample_rate    = ICM42688_SAMPLE_RATE_1000,
    };
    icm42688_data_t data;
    icm42688_status_t status;

    printf("Test: ICM42688 temperature + accel + gyro reading...\n");

    /* Reset mock state */
    memset(mock_registers, 0, sizeof(mock_registers));
    mock_who_am_i_value = ICM42688_ID;

    /* Setup 14-byte burst: temp=0x1234, accel=(100,-200,300), gyro=(-50,75,-125) */
    setup_14byte_burst(0x1234, 100, -200, 300, -50, 75, -125);

    /* Bind and initialize */
    icm42688_hal_init(&mock_comm, &mock_system, &config);
    status = icm42688_init();
    assert(status == ICM42688_STATUS_OK);

    /* Read sensor data */
    status = icm42688_read(&data);
    assert(status == ICM42688_STATUS_OK);

    /* Verify temperature raw */
    assert(data.temperature_raw == 0x1234);

    /* Verify accel raw */
    assert(data.acc_raw.x == 100);
    assert(data.acc_raw.y == -200);
    assert(data.acc_raw.z == 300);

    /* Verify gyro raw */
    assert(data.gyro_raw.x == -50);
    assert(data.gyro_raw.y == 75);
    assert(data.gyro_raw.z == -125);

    printf("  Temperature raw: 0x%04X\n", data.temperature_raw);
    printf("  Accel raw: (%d, %d, %d)\n", data.acc_raw.x, data.acc_raw.y, data.acc_raw.z);
    printf("  Gyro raw: (%d, %d, %d)\n", data.gyro_raw.x, data.gyro_raw.y, data.gyro_raw.z);
    printf("  PASS\n");
}

int main(void)
{
    test_temperature_reading();
    printf("\nAll ICM42688 tests PASSED\n");
    return 0;
}
