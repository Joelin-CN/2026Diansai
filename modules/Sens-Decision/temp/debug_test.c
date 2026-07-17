#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include "config.h"
#include "interface.h"
#include "preprocess.h"
#include "state_evaluate.h"
#include "utils.h"

static int32_t g_encoder_counts[4] = {0};
static uint16_t g_ir_mask = 0;
typedef struct { int16_t accel[3]; int16_t gyro[3]; int16_t temperature; } imu_raw_data_t;
static imu_raw_data_t g_imu_raw = {0};

static sd_status_t virtual_read_encoder(uint8_t index, int32_t *count) {
    if (index >= 4 || count == NULL) return SD_ERR_INVALID_ARGUMENT;
    *count = g_encoder_counts[index];
    return SD_OK;
}

static sd_status_t virtual_read_imu(imu_raw_data_t *data) {
    if (data == NULL) return SD_ERR_INVALID_ARGUMENT;
    *data = g_imu_raw;
    return SD_OK;
}

static sd_status_t virtual_read_ir(uint16_t *mask) {
    if (mask == NULL) return SD_ERR_INVALID_ARGUMENT;
    *mask = g_ir_mask;
    return SD_OK;
}

static const sensor_hal_t g_virtual_hal = {
    virtual_read_encoder, virtual_read_imu, virtual_read_ir
};

int main(void) {
    state_evaluator_t evaluator;
    sensor_frame_t frame;
    sd_status_t status;
    size_t i;
    
    sd_config_reset_defaults();
    sensors_configure_hal(&g_virtual_hal);
    sensors_init_all();
    
    state_evaluator_init(&evaluator, &g_sens_decision_config.ekf);
    
    status = preprocess_update(1000000U, &frame);
    printf("First preprocess: %d\n", status);
    for (i = 0; i < 4; ++i) {
        printf("  encoder[%zu] valid=%d speed=%f\n", i, frame.encoder_valid[i], frame.encoders[i].speed_mps);
    }
    printf("  imu valid=%d\n", frame.imu_valid);
    
    status = state_evaluator_update(&evaluator, &frame);
    printf("First update: %d\n", status);
    
    for (i = 0; i < 4; ++i) {
        g_encoder_counts[i] = 1024 * 5;
    }
    g_imu_raw.gyro[2] = 0;
    
    status = preprocess_update(2000000U, &frame);
    printf("Second preprocess: %d\n", status);
    for (i = 0; i < 4; ++i) {
        printf("  encoder[%zu] valid=%d speed=%f\n", i, frame.encoder_valid[i], frame.encoders[i].speed_mps);
    }
    printf("  imu valid=%d\n", frame.imu_valid);
    
    status = state_evaluator_update(&evaluator, &frame);
    printf("Second update: %d\n", status);
    
    return 0;
}
