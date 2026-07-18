/**
 * @file test_target_adapters.c
 * @brief Target adapter tests: encoder, motor, and sensor HAL
 * @date 2026-07-18
 */

#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

/* Motion Control interfaces */
#include "../modules/Motion Control/inc/motion_feedback.h"
#include "../modules/Motion Control/inc/motion_control.h"

/* Sens-Decision interfaces */
#include "../modules/Sens-Decision/inc/interface.h"
#include "../modules/Sens-Decision/inc/config.h"

/* Adapter headers (to be created) */
#include "../inc/encoder_adapter.h"
#include "../inc/motor_adapter.h"
#include "../inc/sensor_adapter.h"

/* ============================================================================
 * Fake hardware state for testing
 * ============================================================================ */

static int32_t g_fake_encoder_count[4] = {0};
static int16_t g_fake_motor_speeds[4] = {0};
static bool g_fake_motor_stopped = false;
static bool g_fake_motor_initialized = false;

/* ICM42688 fake data */
static int16_t g_fake_icm_accel[3] = {100, 200, 300};
static int16_t g_fake_icm_gyro[3] = {10, 20, 30};
static int16_t g_fake_icm_temperature = 2500;
static bool g_fake_icm_fail = false;

/* MCP23017 fake data */
static uint16_t g_fake_mcp_inputs = 0x0FFF;
static int g_fake_mcp_status = 0; /* 0=OK, 1=TIMEOUT, 2=IO_ERROR */

/* ============================================================================
 * Fake low-level driver implementations
 * ============================================================================ */

#include "../inc/encoder.h"
#include "../inc/motor.h"
#include "../modules/ICM42688/inc/icm42688_hal.h"
#include "../modules/MCP23017/inc/mcp23017.h"

/* Override encoder functions */
int32_t Encoder_GetCount(Encoder_Id encoder) {
    if ((uint8_t)encoder < 4) {
        return g_fake_encoder_count[encoder];
    }
    return 0;
}

void Encoder_ResetCount(Encoder_Id encoder) {
    if ((uint8_t)encoder < 4) {
        g_fake_encoder_count[encoder] = 0;
    }
}

/* Override motor functions */
void Motor_Init(void) {
    g_fake_motor_initialized = true;
}

void Motor_SetSpeed(int16_t leftSpeed, int16_t rightSpeed) {
    g_fake_motor_speeds[0] = leftSpeed;
    g_fake_motor_speeds[1] = leftSpeed;
    g_fake_motor_speeds[2] = rightSpeed;
    g_fake_motor_speeds[3] = rightSpeed;
}

void Motor_Stop(void) {
    g_fake_motor_stopped = true;
    memset(g_fake_motor_speeds, 0, sizeof(g_fake_motor_speeds));
}

/* Override ICM42688 function */
icm42688_status_t icm42688_read(icm42688_data_t *data) {
    if (g_fake_icm_fail || data == NULL) {
        return ICM42688_STATUS_NOT_READY;
    }
    data->temperature_raw = g_fake_icm_temperature;
    data->acc_raw.x = g_fake_icm_accel[0];
    data->acc_raw.y = g_fake_icm_accel[1];
    data->acc_raw.z = g_fake_icm_accel[2];
    data->gyro_raw.x = g_fake_icm_gyro[0];
    data->gyro_raw.y = g_fake_icm_gyro[1];
    data->gyro_raw.z = g_fake_icm_gyro[2];
    /* acc_g and gyro_dps are not used by sensor_adapter */
    return ICM42688_STATUS_OK;
}

/* Override MCP23017 function */
mcp23017_status_t MCP23017_ReadInputs(uint16_t *inputs) {
    if (inputs == NULL) {
        return MCP23017_STATUS_INVALID_ARGUMENT;
    }
    if (g_fake_mcp_status == 1) {
        return MCP23017_STATUS_TIMEOUT;
    }
    if (g_fake_mcp_status == 2) {
        return MCP23017_STATUS_IO_ERROR;
    }
    *inputs = g_fake_mcp_inputs;
    return MCP23017_STATUS_OK;
}

/* ============================================================================
 * Test: Encoder Adapter Mapping
 * ============================================================================ */

static void test_encoder_adapter_mapping(void) {
    printf("TEST: Encoder adapter logical-to-physical mapping\n");
    
    /* Setup fake encoder counts */
    g_fake_encoder_count[0] = 1000;  /* M1 = LEFT_FRONT */
    g_fake_encoder_count[1] = 2000;  /* M2 = LEFT_REAR */
    g_fake_encoder_count[2] = 3000;  /* M3 = RIGHT_FRONT */
    g_fake_encoder_count[3] = 4000;  /* M4 = RIGHT_REAR */
    
    EncoderInterface_t *encoder = EncoderAdapter_GetInterface();
    assert(encoder != NULL);
    assert(encoder->getCount != NULL);
    assert(encoder->resetCount != NULL);
    
    /* Verify explicit mappings */
    assert(encoder->getCount(ENCODER_LEFT_FRONT) == 1000);
    assert(encoder->getCount(ENCODER_LEFT_REAR) == 2000);
    assert(encoder->getCount(ENCODER_RIGHT_FRONT) == 3000);
    assert(encoder->getCount(ENCODER_RIGHT_REAR) == 4000);
    
    /* Test reset */
    encoder->resetCount(ENCODER_LEFT_FRONT);
    assert(g_fake_encoder_count[0] == 0);
    assert(encoder->getCount(ENCODER_LEFT_FRONT) == 0);
    
    printf("  PASS: All four encoder mappings verified\n");
}

/* ============================================================================
 * Test: Motor Adapter Differential Fan-out
 * ============================================================================ */

static void test_motor_adapter_fanout(void) {
    printf("TEST: Motor adapter differential PWM fan-out\n");
    
    memset(g_fake_motor_speeds, 0, sizeof(g_fake_motor_speeds));
    g_fake_motor_stopped = false;
    g_fake_motor_initialized = false;
    
    MotorInterface_t *motor = MotorAdapter_GetInterface();
    assert(motor != NULL);
    assert(motor->setDifferentialPWM != NULL);
    assert(motor->stop != NULL);
    assert(motor->init != NULL);
    
    /* Test initialization */
    motor->init();
    assert(g_fake_motor_initialized == true);
    
    /* Test differential PWM: left=321, right=-456 */
    motor->setDifferentialPWM(321, -456);
    assert(g_fake_motor_speeds[0] == 321);  /* M1 left */
    assert(g_fake_motor_speeds[1] == 321);  /* M2 left */
    assert(g_fake_motor_speeds[2] == -456); /* M3 right */
    assert(g_fake_motor_speeds[3] == -456); /* M4 right */
    
    /* Test stop */
    motor->stop();
    assert(g_fake_motor_stopped == true);
    
    printf("  PASS: Motor fan-out (M1/M2=left, M3/M4=right) verified\n");
}

/* ============================================================================
 * Test: Sensor HAL - IMU Raw Data Passthrough
 * ============================================================================ */

static void test_sensor_hal_imu_raw(void) {
    printf("TEST: Sensor HAL IMU raw data passthrough\n");
    
    g_fake_icm_accel[0] = 123;
    g_fake_icm_accel[1] = 456;
    g_fake_icm_accel[2] = 789;
    g_fake_icm_gyro[0] = -100;
    g_fake_icm_gyro[1] = -200;
    g_fake_icm_gyro[2] = -300;
    g_fake_icm_temperature = 3210;
    g_fake_icm_fail = false;
    
    const sensor_hal_t *hal = SensorAdapter_GetHal();
    assert(hal != NULL);
    assert(hal->read_imu_raw != NULL);
    
    imu_raw_data_t imu;
    sd_status_t status = hal->read_imu_raw(&imu);
    
    assert(status == SD_OK);
    assert(imu.accel[0] == 123);
    assert(imu.accel[1] == 456);
    assert(imu.accel[2] == 789);
    assert(imu.gyro[0] == -100);
    assert(imu.gyro[1] == -200);
    assert(imu.gyro[2] == -300);
    assert(imu.temperature == 3210);
    
    printf("  PASS: IMU raw LSB values preserved\n");
}

/* ============================================================================
 * Test: Sensor HAL - IMU Failure Propagation
 * ============================================================================ */

static void test_sensor_hal_imu_failure(void) {
    printf("TEST: Sensor HAL IMU failure propagation\n");
    
    g_fake_icm_fail = true;
    
    const sensor_hal_t *hal = SensorAdapter_GetHal();
    imu_raw_data_t imu;
    sd_status_t status = hal->read_imu_raw(&imu);
    
    assert(status == SD_ERR_READ);
    
    /* Test NULL argument */
    status = hal->read_imu_raw(NULL);
    assert(status == SD_ERR_INVALID_ARGUMENT);
    
    g_fake_icm_fail = false;
    printf("  PASS: IMU failure propagated as SD_ERR_READ\n");
}

/* ============================================================================
 * Test: Sensor HAL - IR Mask with Active Polarity
 * ============================================================================ */

static void test_sensor_hal_ir_polarity(void) {
    printf("TEST: Sensor HAL IR mask with active polarity\n");
    
    g_fake_mcp_inputs = 0x0ABC;  /* 12-bit value */
    g_fake_mcp_status = 0;
    
    const sensor_hal_t *hal = SensorAdapter_GetHal();
    assert(hal != NULL);
    assert(hal->read_ir_mask != NULL);
    
    uint16_t mask;
    sd_status_t status = hal->read_ir_mask(&mask);
    
    assert(status == SD_OK);
    /* Mask should be limited to 12 bits */
    assert(mask == 0x0ABC);
    
    printf("  PASS: IR mask masked to 12 bits\n");
}

/* ============================================================================
 * Test: Sensor HAL - IR Failure Propagation
 * ============================================================================ */

static void test_sensor_hal_ir_failure(void) {
    printf("TEST: Sensor HAL IR failure propagation\n");
    
    const sensor_hal_t *hal = SensorAdapter_GetHal();
    uint16_t mask;
    
    /* Test timeout */
    g_fake_mcp_status = 1;
    sd_status_t status = hal->read_ir_mask(&mask);
    assert(status == SD_ERR_TIMEOUT);
    
    /* Test IO error */
    g_fake_mcp_status = 2;
    status = hal->read_ir_mask(&mask);
    assert(status == SD_ERR_READ);
    
    /* Test NULL argument */
    g_fake_mcp_status = 0;
    status = hal->read_ir_mask(NULL);
    assert(status == SD_ERR_INVALID_ARGUMENT);
    
    printf("  PASS: IR failures propagated correctly\n");
}

/* ============================================================================
 * Test: Sensor HAL - Encoder via HAL
 * ============================================================================ */

static void test_sensor_hal_encoder(void) {
    printf("TEST: Sensor HAL encoder reading\n");
    
    g_fake_encoder_count[0] = 100;
    g_fake_encoder_count[1] = 200;
    g_fake_encoder_count[2] = 300;
    g_fake_encoder_count[3] = 400;
    
    const sensor_hal_t *hal = SensorAdapter_GetHal();
    assert(hal != NULL);
    assert(hal->read_encoder_count != NULL);
    
    int32_t count;
    
    /* Test valid indices */
    assert(hal->read_encoder_count(0, &count) == SD_OK);
    assert(count == 100);
    
    assert(hal->read_encoder_count(1, &count) == SD_OK);
    assert(count == 200);
    
    assert(hal->read_encoder_count(2, &count) == SD_OK);
    assert(count == 300);
    
    assert(hal->read_encoder_count(3, &count) == SD_OK);
    assert(count == 400);
    
    /* Test invalid index */
    assert(hal->read_encoder_count(4, &count) == SD_ERR_INVALID_ARGUMENT);
    
    /* Test NULL argument */
    assert(hal->read_encoder_count(0, NULL) == SD_ERR_INVALID_ARGUMENT);
    
    printf("  PASS: Encoder HAL validated\n");
}

/* ============================================================================
 * Main Test Runner
 * ============================================================================ */

int main(void) {
    printf("=== Target Adapter Tests ===\n\n");
    
    test_encoder_adapter_mapping();
    test_motor_adapter_fanout();
    test_sensor_hal_imu_raw();
    test_sensor_hal_imu_failure();
    test_sensor_hal_ir_polarity();
    test_sensor_hal_ir_failure();
    test_sensor_hal_encoder();
    
    printf("\n=== All Tests PASSED ===\n");
    return 0;
}
