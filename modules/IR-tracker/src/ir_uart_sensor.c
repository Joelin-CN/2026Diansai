/**
 * @file ir_uart_sensor.c
 * @brief UART sensor frame receiver for 8-way infrared module
 * @date 2026-07-23
 */

#include "ir_uart_sensor.h"

#include "ti_msp_dl_config.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef enum {
    IR_UART_SENSOR_MODE_IDLE = 0,
    IR_UART_SENSOR_MODE_ANALOG,
    IR_UART_SENSOR_MODE_DIGITAL,
} ir_uart_sensor_mode_t;

static volatile uint8_t g_rx_frame[IR_UART_SENSOR_FRAME_MAX];
static volatile uint8_t g_rx_length = 0U;
static volatile bool g_rx_receiving = false;
static volatile bool g_rx_overflow = false;
static volatile bool g_frame_ready = false;

static uint8_t g_frame[IR_UART_SENSOR_FRAME_MAX];
static uint16_t g_analog[IR_UART_SENSOR_COUNT];
static bool g_analog_valid = false;
static ir_uart_sensor_mode_t g_mode = IR_UART_SENSOR_MODE_IDLE;

static void IrUartSensor_SendString(const char *str)
{
    while (*str != '\0') {
        while (DL_UART_Main_isBusy(UART1_INST)) {
        }
        DL_UART_Main_transmitData(UART1_INST, (uint8_t)*str);
        str++;
    }
}

static uint16_t IrUartSensor_ParseDecimal(const char *start, const char *end, bool *ok)
{
    uint32_t value = 0U;
    bool local_ok = true;

    if ((start == NULL) || (end == NULL) || (start >= end)) {
        *ok = false;
        return 0U;
    }

    while (start < end) {
        char c = *start++;
        if ((c < '0') || (c > '9')) {
            local_ok = false;
            break;
        }
        value = (value * 10U) + (uint32_t)(c - '0');
        if (value > 0xFFFFU) {
            local_ok = false;
            break;
        }
    }

    *ok = local_ok;
    return (uint16_t)value;
}

void IrUartSensor_Init(void)
{
    IrUartSensor_Reset();
    DL_UART_Main_enableInterrupt(UART1_INST, DL_UART_MAIN_INTERRUPT_RX);
    NVIC_ClearPendingIRQ(UART1_INST_INT_IRQN);
    NVIC_EnableIRQ(UART1_INST_INT_IRQN);
}

void IrUartSensor_Reset(void)
{
    uint8_t i;

    g_rx_length = 0U;
    g_rx_receiving = false;
    g_rx_overflow = false;
    g_frame_ready = false;
    g_analog_valid = false;
    g_mode = IR_UART_SENSOR_MODE_IDLE;

    for (i = 0U; i < IR_UART_SENSOR_COUNT; i++) {
        g_analog[i] = 0U;
    }

    memset((void *)g_rx_frame, 0, sizeof(g_rx_frame));
    memset(g_frame, 0, sizeof(g_frame));
}

void IrUartSensor_RequestAnalogMode(void)
{
    g_mode = IR_UART_SENSOR_MODE_ANALOG;
    IrUartSensor_SendString("$0,1,0#");
}

void IrUartSensor_RequestDigitalMode(void)
{
    g_mode = IR_UART_SENSOR_MODE_DIGITAL;
    IrUartSensor_SendString("$0,0,1#");
}

void IrUartSensor_RxByte(uint8_t byte)
{
    if (byte == '$') {
        g_rx_receiving = true;
        g_rx_length = 0U;
        g_rx_overflow = false;
    }

    if (!g_rx_receiving) {
        return;
    }

    if (g_rx_length < IR_UART_SENSOR_FRAME_MAX) {
        g_rx_frame[g_rx_length] = byte;
        g_rx_length++;
    } else {
        g_rx_overflow = true;
        g_rx_receiving = false;
        g_rx_length = 0U;
        return;
    }

    if (byte == '#') {
        g_rx_receiving = false;
        g_frame_ready = true;
    }
}

static bool IrUartSensor_ParseAnalogFrame(const uint8_t *frame, uint8_t length)
{
    uint8_t i;
    const char *cursor;
    const char *frame_end;

    if ((frame == NULL) || (length < 4U)) {
        return false;
    }

    if ((frame[0] != '$') || (frame[1] != 'A') || (frame[length - 1U] != '#')) {
        return false;
    }

    cursor = (const char *)&frame[2];
    frame_end = (const char *)&frame[length - 1U];

    for (i = 0U; i < IR_UART_SENSOR_COUNT; i++) {
        const char *field_start;
        const char *field_mid;
        const char *field_end;
        bool ok;
        uint16_t value;

        field_start = cursor;
        if (field_start >= frame_end) {
            return false;
        }

        field_mid = NULL;
        field_end = field_start;
        while (field_end < frame_end) {
            if (*field_end == ':') {
                field_mid = field_end;
            } else if (*field_end == ',') {
                break;
            }
            field_end++;
        }

        if (field_mid == NULL) {
            return false;
        }

        value = IrUartSensor_ParseDecimal(field_mid + 1, field_end, &ok);
        if (!ok) {
            return false;
        }

        g_analog[i] = value;

        if (field_end < frame_end && *field_end == ',') {
            cursor = field_end + 1;
        } else {
            cursor = field_end;
        }
    }

    return true;
}

ir_uart_sensor_status_t IrUartSensor_Process(void)
{
    bool parsed_ok;

    if (g_rx_overflow) {
        g_rx_overflow = false;
        g_frame_ready = false;
        return IR_UART_SENSOR_STATUS_OVERFLOW;
    }

    if (!g_frame_ready) {
        return IR_UART_SENSOR_STATUS_NO_FRAME;
    }

    g_frame_ready = false;
    memcpy(g_frame, (const void *)g_rx_frame, g_rx_length);

    parsed_ok = IrUartSensor_ParseAnalogFrame(g_frame, g_rx_length);
    if (!parsed_ok) {
        g_analog_valid = false;
        return IR_UART_SENSOR_STATUS_BAD_FRAME;
    }

    g_analog_valid = true;
    return IR_UART_SENSOR_STATUS_OK;
}

bool IrUartSensor_GetAnalog(uint16_t values[IR_UART_SENSOR_COUNT])
{
    uint8_t i;

    if ((values == NULL) || !g_analog_valid) {
        return false;
    }

    for (i = 0U; i < IR_UART_SENSOR_COUNT; i++) {
        values[i] = g_analog[i];
    }

    return true;
}

bool IrUartSensor_GetFrameReady(void)
{
    return g_frame_ready;
}
