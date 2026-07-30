/**
 * @file test_icm42688_hardware.c
 * @brief ICM42688 gyroscope hardware validation test
 * @date 2026-07-23
 *
 * Test procedure:
 * 1. SPI communication test (read WHO_AM_I register)
 * 2. Sensor initialization
 * 3. Raw data reading
 * 4. Gyro calibration (keep device stationary!)
 * 5. Continuous data output with calibration applied
 */

#include "ti_msp_dl_config.h"
#include "uart_debug.h"
#include "platform_time.h"
#include "../modules/ICM42688/inc/icm42688_hal.h"
#include "../modules/ICM42688/inc/icm42688_mspm0.h"

#include <stdio.h>
#include <string.h>

/* Test configuration */
#define CALIBRATION_SAMPLES 100
#define CALIBRATION_INTERVAL_MS 10
#define TEST_READ_INTERVAL_MS 100  // 10Hz output

/* Test state */
typedef enum {
    TEST_STEP_INIT = 0,
    TEST_STEP_SPI_COMM,
    TEST_STEP_SENSOR_INIT,
    TEST_STEP_RAW_READ,
    TEST_STEP_CALIBRATION,
    TEST_STEP_CONTINUOUS,
    TEST_STEP_FAILED,
    TEST_STEP_COMPLETE
} test_step_t;

static test_step_t g_test_step = TEST_STEP_INIT;
static uint32_t g_test_counter = 0;
static icm42688_data_t g_imu_data;

/* Test configuration for ICM42688 */
static const icm42688_config_t g_icm_config = {
    .interface_type = ICM42688_INTERFACE_SPI,
    .acc_sample     = ICM42688_ACC_SAMPLE_SGN_4G,      // ±4g
    .gyro_sample    = ICM42688_GYRO_SAMPLE_SGN_2000DPS, // ±2000 dps
    .sample_rate    = ICM42688_SAMPLE_RATE_1000         // 1kHz
};

/**
 * @brief Test Step 1: Direct SPI communication test
 * @return true if WHO_AM_I register reads correctly
 */
static bool test_spi_communication(void)
{
    printf("\n=== TEST 1: SPI Communication ===\n");
    printf("Reading WHO_AM_I register (should be 0x47)...\n");

    // Manual SPI read: WHO_AM_I register
    // CS low
    DL_GPIO_clearPins(ICM42688_PORT, ICM42688_CS_PIN);
    delay_cycles(160); // ~5us @ 32MHz

    // Send register address with read bit
    uint8_t tx_buf[2] = {ICM42688_WHO_AM_I | 0x80, 0x00};
    uint8_t rx_buf[2] = {0};

    for (int i = 0; i < 2; i++) {
        while (DL_SPI_isTXFIFOFull(SPI1_INST));
        DL_SPI_transmitData8(SPI1_INST, tx_buf[i]);
        while (DL_SPI_isRXFIFOEmpty(SPI1_INST));
        rx_buf[i] = DL_SPI_receiveData8(SPI1_INST);
    }

    // CS high
    delay_cycles(160);
    DL_GPIO_setPins(ICM42688_PORT, ICM42688_CS_PIN);

    uint8_t who_am_i = rx_buf[1];
    printf("WHO_AM_I = 0x%02X (expected 0x47)\n", who_am_i);

    if (who_am_i == ICM42688_ID) {
        printf("✓ SPI communication OK\n");
        return true;
    } else {
        printf("✗ FAILED: Wrong device ID or SPI error\n");
        printf("  Check: SPI wiring, CS pin, clock polarity/phase\n");
        return false;
    }
}

/**
 * @brief Test Step 2: Sensor initialization
 */
static bool test_sensor_init(void)
{
    printf("\n=== TEST 2: Sensor Initialization ===\n");
    printf("Binding MSPM0 adapter...\n");

    icm42688_mspm0_bind(&g_icm_config);
    printf("Calling icm42688_init()...\n");

    icm42688_status_t status = icm42688_init();

    printf("Init status: %d ", status);
    switch (status) {
        case ICM42688_STATUS_OK:
            printf("(OK)\n");
            printf("✓ Sensor initialized successfully\n");

            // Print configuration
            float acc_scale, gyro_scale;
            icm42688_get_scale_factors(&acc_scale, &gyro_scale);
            printf("  Accel scale: %.6f g/LSB\n", acc_scale);
            printf("  Gyro scale:  %.6f dps/LSB\n", gyro_scale);
            return true;

        case ICM42688_STATUS_BAD_ID:
            printf("(BAD_ID)\n");
            printf("✗ FAILED: Device ID verification failed\n");
            break;

        case ICM42688_STATUS_NOT_READY:
            printf("(NOT_READY)\n");
            printf("✗ FAILED: Sensor not responding or timeout\n");
            break;

        default:
            printf("(UNKNOWN: %d)\n", status);
            printf("✗ FAILED: Unknown error\n");
            break;
    }

    return false;
}

/**
 * @brief Test Step 3: Raw data reading
 */
static bool test_raw_read(void)
{
    printf("\n=== TEST 3: Raw Data Reading ===\n");
    printf("Reading 5 samples...\n\n");

    printf("Sample |   Accel (raw)    |    Gyro (raw)     | Temp\n");
    printf("-------|------------------|-------------------|------\n");

    for (int i = 0; i < 5; i++) {
        icm42688_status_t status = icm42688_read(&g_imu_data);

        if (status != ICM42688_STATUS_OK) {
            printf("✗ Read failed at sample %d, status: %d\n", i, status);
            return false;
        }

        printf("  %d    | %6d %6d %6d | %6d %6d %6d | %5d\n",
               i + 1,
               g_imu_data.acc_raw.x, g_imu_data.acc_raw.y, g_imu_data.acc_raw.z,
               g_imu_data.gyro_raw.x, g_imu_data.gyro_raw.y, g_imu_data.gyro_raw.z,
               g_imu_data.temperature_raw);

        delay_cycles(3200000); // ~100ms @ 32MHz
    }

    printf("\n✓ Raw data reading OK\n");
    printf("  Note: Accel Z should be ~±16384 (1g @ ±4g range)\n");
    printf("  Note: Gyro should be near zero if stationary\n");
    return true;
}

/**
 * @brief Test Step 4: Gyro calibration
 */
static bool test_calibration(void)
{
    printf("\n=== TEST 4: Gyro Calibration ===\n");
    printf("⚠ KEEP DEVICE STATIONARY!\n");
    printf("Calibrating with %d samples...\n", CALIBRATION_SAMPLES);

    icm42688_status_t status = icm42688_calibrate_gyro(
        CALIBRATION_SAMPLES,
        CALIBRATION_INTERVAL_MS
    );

    if (status != ICM42688_STATUS_OK) {
        printf("✗ Calibration failed, status: %d\n", status);
        return false;
    }

    icm42688_vector3f_t bias;
    icm42688_get_gyro_bias(&bias);

    printf("✓ Calibration complete\n");
    printf("  Gyro bias (dps): X=%.3f, Y=%.3f, Z=%.3f\n",
           bias.x, bias.y, bias.z);

    // Check if bias is reasonable (should be < 10 dps typically)
    if (bias.x < -50.0f || bias.x > 50.0f ||
        bias.y < -50.0f || bias.y > 50.0f ||
        bias.z < -50.0f || bias.z > 50.0f) {
        printf("⚠ WARNING: Bias seems large, device may have been moving\n");
    }

    return true;
}

/**
 * @brief Test Step 5: Continuous reading with calibration
 */
static void test_continuous(void)
{
    static uint32_t last_read_time = 0;
    uint32_t current_time = PlatformTime_GetUs64() / 1000; // Convert to ms

    if (current_time - last_read_time < TEST_READ_INTERVAL_MS) {
        return;
    }
    last_read_time = current_time;

    icm42688_status_t status = icm42688_read(&g_imu_data);

    if (status != ICM42688_STATUS_OK) {
        printf("[%u] Read error: %d\n", g_test_counter, status);
        return;
    }

    // Output in engineering units (with calibration applied)
    printf("[%4u] Accel(g): %7.3f %7.3f %7.3f | Gyro(dps): %8.2f %8.2f %8.2f | Temp: %.1f°C\n",
           g_test_counter,
           g_imu_data.acc_g.x, g_imu_data.acc_g.y, g_imu_data.acc_g.z,
           g_imu_data.gyro_dps.x, g_imu_data.gyro_dps.y, g_imu_data.gyro_dps.z,
           (g_imu_data.temperature_raw / 132.48f) + 25.0f);

    g_test_counter++;

    // Suggest next test after 50 samples (~5 seconds)
    if (g_test_counter == 50) {
        printf("\n>>> Try rotating the device slowly to see gyro response <<<\n\n");
    }
}

/**
 * @brief Main test loop
 */
void test_icm42688_main_loop(void)
{
    switch (g_test_step) {
        case TEST_STEP_INIT:
            printf("\n");
            printf("=====================================\n");
            printf(" ICM42688 Hardware Validation Test  \n");
            printf("=====================================\n");
            printf("Board: MSPM0G3507\n");
            printf("SPI: SPI1_INST\n");
            printf("CS:  ICM42688_PORT/ICM42688_CS_PIN\n");
            printf("\n");
            g_test_step = TEST_STEP_SPI_COMM;
            break;

        case TEST_STEP_SPI_COMM:
            if (test_spi_communication()) {
                g_test_step = TEST_STEP_SENSOR_INIT;
            } else {
                g_test_step = TEST_STEP_FAILED;
            }
            delay_cycles(32000000); // 1s delay
            break;

        case TEST_STEP_SENSOR_INIT:
            if (test_sensor_init()) {
                g_test_step = TEST_STEP_RAW_READ;
            } else {
                g_test_step = TEST_STEP_FAILED;
            }
            delay_cycles(32000000); // 1s delay
            break;

        case TEST_STEP_RAW_READ:
            if (test_raw_read()) {
                g_test_step = TEST_STEP_CALIBRATION;
            } else {
                g_test_step = TEST_STEP_FAILED;
            }
            delay_cycles(32000000); // 1s delay
            break;

        case TEST_STEP_CALIBRATION:
            if (test_calibration()) {
                printf("\n=== TEST 5: Continuous Monitoring ===\n");
                printf("Reading at %d Hz (press reset to stop)...\n\n",
                       1000 / TEST_READ_INTERVAL_MS);
                g_test_step = TEST_STEP_CONTINUOUS;
                g_test_counter = 0;
            } else {
                g_test_step = TEST_STEP_FAILED;
            }
            delay_cycles(32000000); // 1s delay
            break;

        case TEST_STEP_CONTINUOUS:
            test_continuous();
            break;

        case TEST_STEP_FAILED:
            printf("\n");
            printf("=====================================\n");
            printf("         TEST FAILED                 \n");
            printf("=====================================\n");
            printf("Check hardware connections and power\n");
            g_test_step = TEST_STEP_COMPLETE;
            break;

        case TEST_STEP_COMPLETE:
            // Do nothing, halt
            break;
    }
}
