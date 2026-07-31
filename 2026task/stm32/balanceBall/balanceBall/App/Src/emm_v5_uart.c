#include "emm_v5_uart.h"

#include <string.h>

static void publish_result(EmmV5Uart *uart, EmmV5UartState state,
                           size_t response_length)
{
    uart->state = state;
    uart->result.state = state;
    uart->result.response = uart->rx_storage;
    uart->result.response_length = response_length;
    uart->result.expected_function = uart->expected_function;
}

void emm_v5_uart_init(EmmV5Uart *uart, UART_HandleTypeDef *handle,
                      uint32_t timeout_ms)
{
    memset(uart, 0, sizeof(*uart));
    uart->handle = handle;
    uart->timeout_ms = timeout_ms;
    uart->state = EMM_V5_UART_IDLE;
}

BalanceMotorTxResult emm_v5_uart_send(void *context, const uint8_t *frame,
                                      size_t length, uint8_t expected_function,
                                      size_t expected_length)
{
    EmmV5Uart *uart = context;

    if (uart == NULL || uart->handle == NULL || frame == NULL || length == 0U ||
        length > EMM_V5_MAX_FRAME_SIZE || expected_length == 0U ||
        expected_length > EMM_V5_MAX_FRAME_SIZE) {
        return BALANCE_MOTOR_TX_FAILED;
    }
    if (uart->state != EMM_V5_UART_IDLE) {
        return BALANCE_MOTOR_TX_BUSY;
    }

    memcpy(uart->tx_storage, frame, length);
    uart->expected_function = expected_function;
    uart->expected_length = expected_length;
    uart->deadline_ms = HAL_GetTick() + uart->timeout_ms;
    uart->event_rx_length = 0U;
    uart->tx_complete_event = 0U;
    uart->rx_event = 0U;
    uart->error_event = 0U;
    uart->state = EMM_V5_UART_ACTIVE;

    if (HAL_UARTEx_ReceiveToIdle_DMA(uart->handle, uart->rx_storage,
                                     (uint16_t)expected_length) != HAL_OK) {
        publish_result(uart, EMM_V5_UART_HAL_ERROR, 0U);
        return BALANCE_MOTOR_TX_FAILED;
    }
    __HAL_DMA_DISABLE_IT(uart->handle->hdmarx, DMA_IT_HT);

    if (HAL_UART_Transmit_DMA(uart->handle, uart->tx_storage,
                              (uint16_t)length) != HAL_OK) {
        (void)HAL_UART_AbortReceive(uart->handle);
        publish_result(uart, EMM_V5_UART_HAL_ERROR, 0U);
        return BALANCE_MOTOR_TX_FAILED;
    }

    return BALANCE_MOTOR_TX_ACCEPTED;
}

void emm_v5_uart_poll(EmmV5Uart *uart, uint32_t now_ms)
{
    uint8_t tx_complete;
    uint8_t rx_event;
    uint8_t error_event;
    size_t rx_length;
    uint32_t primask;

    if (uart == NULL || uart->state != EMM_V5_UART_ACTIVE) {
        return;
    }

    primask = __get_PRIMASK();
    __disable_irq();
    tx_complete = uart->tx_complete_event;
    rx_event = uart->rx_event;
    error_event = uart->error_event;
    rx_length = uart->event_rx_length;
    uart->tx_complete_event = 0U;
    uart->rx_event = 0U;
    uart->error_event = 0U;
    __set_PRIMASK(primask);

    if (error_event != 0U) {
        publish_result(uart, EMM_V5_UART_HAL_ERROR, 0U);
        return;
    }
    if (rx_event != 0U) {
        if (rx_length > EMM_V5_MAX_FRAME_SIZE) {
            rx_length = EMM_V5_MAX_FRAME_SIZE;
        }
        if (rx_length != uart->expected_length || rx_length < 2U ||
            uart->rx_storage[1] != uart->expected_function) {
            publish_result(uart, EMM_V5_UART_PROTOCOL_ERROR, rx_length);
        } else {
            publish_result(uart, EMM_V5_UART_COMPLETE, rx_length);
        }
        return;
    }
    if (uart->expected_length == 0U && tx_complete != 0U) {
        publish_result(uart, EMM_V5_UART_COMPLETE, 0U);
        return;
    }
    if ((uint32_t)(now_ms - uart->deadline_ms) < 0x80000000U) {
        (void)HAL_UART_AbortReceive(uart->handle);
        publish_result(uart, EMM_V5_UART_TIMEOUT, 0U);
    }
}

bool emm_v5_uart_take_result(EmmV5Uart *uart, EmmV5UartResult *result)
{
    if (uart == NULL || result == NULL || uart->state == EMM_V5_UART_IDLE ||
        uart->state == EMM_V5_UART_ACTIVE) {
        return false;
    }

    *result = uart->result;
    uart->state = EMM_V5_UART_IDLE;
    return true;
}

void emm_v5_uart_on_tx_complete(EmmV5Uart *uart)
{
    if (uart != NULL) {
        uart->tx_complete_event = 1U;
    }
}

void emm_v5_uart_on_rx_event(EmmV5Uart *uart, size_t received_length)
{
    if (uart != NULL) {
        uart->event_rx_length = received_length;
        uart->rx_event = 1U;
    }
}

void emm_v5_uart_on_error(EmmV5Uart *uart)
{
    if (uart != NULL) {
        uart->error_event = 1U;
    }
}
