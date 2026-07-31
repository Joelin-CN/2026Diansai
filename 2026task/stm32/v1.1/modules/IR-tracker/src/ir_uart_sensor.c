/**
 * @file ir_uart_sensor.c
 * @brief UART sensor frame receiver for 8-way infrared module
 * @date 2026-07-23
 */

#include "ir_uart_sensor.h"
#include "usart.h"
#include "stm32f4xx_hal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Use USART2 for IR sensor communication (PA2=TX, PA3=RX)
#define IR_UART_HANDLE  (&huart2)
#define IR_UART_IRQn    USART2_IRQn

typedef enum {
    IR_UART_SENSOR_MODE_IDLE = 0,
    IR_UART_SENSOR_MODE_ANALOG,
    IR_UART_SENSOR_MODE_DIGITAL,
} ir_uart_sensor_mode_t;

// DOUBLE BUFFERING: Separate buffers for interrupt (write) and main loop (read)
// This completely eliminates race conditions
static volatile uint8_t g_rx_buffer[2][IR_UART_SENSOR_FRAME_MAX];
static volatile uint8_t g_rx_buffer_length[2];
static volatile uint8_t g_rx_write_index = 0;  // Interrupt writes to this buffer
static volatile uint8_t g_rx_read_index = 1;   // Main loop reads from this buffer
static volatile bool g_rx_receiving = false;
static volatile bool g_rx_overflow = false;
static volatile bool g_frame_ready = false;

static uint8_t g_frame[IR_UART_SENSOR_FRAME_MAX];
static uint16_t g_analog[IR_UART_SENSOR_COUNT];
static bool g_analog_valid = false;
static ir_uart_sensor_mode_t g_mode = IR_UART_SENSOR_MODE_IDLE;

static void IrUartSensor_SendString(const char *str)
{
    HAL_UART_Transmit(IR_UART_HANDLE, (uint8_t *)str, strlen(str), HAL_MAX_DELAY);
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

    // Enable UART receive interrupt
    __HAL_UART_ENABLE_IT(IR_UART_HANDLE, UART_IT_RXNE);

    // Note: USART2 interrupt priority is already configured in usart.c (HAL_MspInit)
    // as Priority 3 (above FreeRTOS boundary) for low-latency IR sensor data reception.
    // Do NOT reconfigure here to avoid conflicts.

    // Ensure interrupt is enabled (in case it was disabled elsewhere)
    HAL_NVIC_EnableIRQ(IR_UART_IRQn);

    // WORKAROUND: Force enable RXNEIE by directly setting CR1 register
    // (Some HAL versions have bugs with __HAL_UART_ENABLE_IT macro)
    SET_BIT(IR_UART_HANDLE->Instance->CR1, USART_CR1_RXNEIE);
}

void IrUartSensor_Reset(void)
{
    uint8_t i;

    g_rx_buffer_length[0] = 0U;
    g_rx_buffer_length[1] = 0U;
    g_rx_write_index = 0;
    g_rx_read_index = 1;
    g_rx_receiving = false;
    g_rx_overflow = false;
    g_frame_ready = false;
    g_analog_valid = false;
    g_mode = IR_UART_SENSOR_MODE_IDLE;

    for (i = 0U; i < IR_UART_SENSOR_COUNT; i++) {
        g_analog[i] = 0U;
    }

    memset((void *)g_rx_buffer[0], 0, sizeof(g_rx_buffer[0]));
    memset((void *)g_rx_buffer[1], 0, sizeof(g_rx_buffer[1]));
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
    uint8_t write_idx = g_rx_write_index;
    uint8_t current_length = g_rx_buffer_length[write_idx];

    if (byte == '$') {
        g_rx_receiving = true;
        g_rx_buffer_length[write_idx] = 0U;
        current_length = 0U;
        g_rx_overflow = false;
    }

    if (!g_rx_receiving) {
        return;
    }

    if (current_length < IR_UART_SENSOR_FRAME_MAX) {
        g_rx_buffer[write_idx][current_length] = byte;
        g_rx_buffer_length[write_idx] = current_length + 1U;
    } else {
        g_rx_overflow = true;
        g_rx_receiving = false;
        g_rx_buffer_length[write_idx] = 0U;
        return;
    }

    if (byte == '#') {
        g_rx_receiving = false;

        // BUFFER SWAP: Switch buffers atomically
        // Interrupt continues writing to the other buffer
        // Main loop can safely read from this completed buffer
        uint8_t completed_buffer = write_idx;
        uint8_t new_write_buffer = 1U - write_idx;  // Toggle 0↔1

        g_rx_read_index = completed_buffer;
        g_rx_write_index = new_write_buffer;
        g_frame_ready = true;

        // Prepare new write buffer
        g_rx_buffer_length[new_write_buffer] = 0U;
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

    // Check frame header: $A, (note the comma after A!)
    // and frame tail: #
    if ((frame[0] != '$') || (frame[1] != 'A') || (frame[2] != ',') || (frame[length - 1U] != '#')) {
        return false;
    }

    // Start parsing after "$A,"
    cursor = (const char *)&frame[3];
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

        // Skip 'x' prefix if present (format: x1:248)
        if (*field_start == 'x') {
            field_start++;
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

        // Parse the value after the colon
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
    uint8_t read_idx;
    uint8_t frame_length;

    // Check overflow status
    if (g_rx_overflow) {
        g_rx_overflow = false;
        g_frame_ready = false;
        return IR_UART_SENSOR_STATUS_OVERFLOW;
    }

    // Check if frame is ready
    if (!g_frame_ready) {
        return IR_UART_SENSOR_STATUS_NO_FRAME;
    }

    // Read from the completed buffer (no interrupt conflict!)
    // Interrupt is writing to the other buffer
    read_idx = g_rx_read_index;
    frame_length = g_rx_buffer_length[read_idx];

    // Copy frame data (safe - interrupt won't touch this buffer)
    memcpy(g_frame, (const void *)g_rx_buffer[read_idx], frame_length);

    // Clear the ready flag
    g_frame_ready = false;

    // Parse the frame
    parsed_ok = IrUartSensor_ParseAnalogFrame(g_frame, frame_length);
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

// Note: UART4_IRQHandler is defined in stm32f4xx_it.c
// The IR sensor byte reception is handled there in the USER CODE section
