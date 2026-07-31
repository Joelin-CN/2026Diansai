#ifndef EMM_V5_UART_H
#define EMM_V5_UART_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "balance_motor.h"
#include "main.h"

typedef enum {
    EMM_V5_UART_IDLE,
    EMM_V5_UART_ACTIVE,
    EMM_V5_UART_COMPLETE,
    EMM_V5_UART_TIMEOUT,
    EMM_V5_UART_PROTOCOL_ERROR,
    EMM_V5_UART_HAL_ERROR
} EmmV5UartState;

typedef struct {
    EmmV5UartState state;
    const uint8_t *response;
    size_t response_length;
    uint8_t expected_function;
} EmmV5UartResult;

typedef struct {
    UART_HandleTypeDef *handle;
    EmmV5UartState state;
    uint8_t tx_storage[EMM_V5_MAX_FRAME_SIZE];
    uint8_t rx_storage[EMM_V5_MAX_FRAME_SIZE];
    size_t expected_length;
    uint32_t timeout_ms;
    uint32_t deadline_ms;
    uint8_t expected_function;
    volatile size_t event_rx_length;
    volatile uint8_t tx_complete_event;
    volatile uint8_t rx_event;
    volatile uint8_t error_event;
    EmmV5UartResult result;
} EmmV5Uart;

void emm_v5_uart_init(EmmV5Uart *uart, UART_HandleTypeDef *handle,
                      uint32_t timeout_ms);
BalanceMotorTxResult emm_v5_uart_send(void *context, const uint8_t *frame,
                                      size_t length, uint8_t expected_function,
                                      size_t expected_length);
void emm_v5_uart_poll(EmmV5Uart *uart, uint32_t now_ms);
bool emm_v5_uart_take_result(EmmV5Uart *uart, EmmV5UartResult *result);
void emm_v5_uart_on_tx_complete(EmmV5Uart *uart);
void emm_v5_uart_on_rx_event(EmmV5Uart *uart, size_t received_length);
void emm_v5_uart_on_error(EmmV5Uart *uart);

#endif
