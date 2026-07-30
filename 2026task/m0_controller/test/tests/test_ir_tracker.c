/**
 * @file test_ir_tracker.c
 * @brief 8-way IR-tracker UART analog test: prints per-channel raw values
 * @date 2026-07-28
 */

#include "../modules/IR-tracker/inc/ir_uart_sensor.h"
#include "ti_msp_dl_config.h"
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

void test_ir_tracker_main_loop(void)
{
    uint16_t raw[IR_UART_SENSOR_COUNT];
    uint8_t i;

    if (IrUartSensor_Process() != IR_UART_SENSOR_STATUS_OK) {
        return;
    }

    if (!IrUartSensor_GetAnalog(raw)) {
        return;
    }

    printf("IR:");
    for (i = 0U; i < IR_UART_SENSOR_COUNT; ++i) {
        printf(" %4u", (unsigned int)raw[i]);
    }
    printf("\n");
}
