#include <stdio.h>
#include <string.h>

#include "emm_v5_uart.h"

static int failures;

#define CHECK_TRUE(expr) do { \
    if (!(expr)) { \
        printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        failures++; \
    } \
} while (0)

typedef enum {
    FAKE_CALL_RECEIVE_TO_IDLE_DMA,
    FAKE_CALL_TRANSMIT_DMA,
    FAKE_CALL_ABORT_RECEIVE,
    FAKE_CALL_ABORT_TRANSMIT
} FakeCall;

typedef struct {
    FakeCall calls[8];
    size_t call_count;
    HAL_StatusTypeDef receive_status;
    HAL_StatusTypeDef transmit_status;
    HAL_StatusTypeDef abort_receive_status;
    HAL_StatusTypeDef abort_transmit_status;
    uint8_t *rx_data;
    uint16_t rx_size;
    const uint8_t *tx_data;
    uint16_t tx_size;
    uint32_t tick;
} FakeHal;

static FakeHal fake;
static DMA_HandleTypeDef rx_dma;
static UART_HandleTypeDef handle;
static EmmV5Uart uart;

HAL_StatusTypeDef HAL_UART_Transmit_DMA(UART_HandleTypeDef *uart_handle,
                                        const uint8_t *data, uint16_t size)
{
    CHECK_TRUE(uart_handle == &handle);
    fake.calls[fake.call_count++] = FAKE_CALL_TRANSMIT_DMA;
    fake.tx_data = data;
    fake.tx_size = size;
    return fake.transmit_status;
}

HAL_StatusTypeDef HAL_UARTEx_ReceiveToIdle_DMA(UART_HandleTypeDef *uart_handle,
                                               uint8_t *data, uint16_t size)
{
    CHECK_TRUE(uart_handle == &handle);
    fake.calls[fake.call_count++] = FAKE_CALL_RECEIVE_TO_IDLE_DMA;
    fake.rx_data = data;
    fake.rx_size = size;
    return fake.receive_status;
}

HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef *uart_handle)
{
    CHECK_TRUE(uart_handle == &handle);
    fake.calls[fake.call_count++] = FAKE_CALL_ABORT_RECEIVE;
    return fake.abort_receive_status;
}

HAL_StatusTypeDef HAL_UART_AbortTransmit(UART_HandleTypeDef *uart_handle)
{
    CHECK_TRUE(uart_handle == &handle);
    fake.calls[fake.call_count++] = FAKE_CALL_ABORT_TRANSMIT;
    return fake.abort_transmit_status;
}

uint32_t HAL_GetTick(void)
{
    return fake.tick;
}

uint32_t __get_PRIMASK(void)
{
    return 0U;
}

void __disable_irq(void)
{
}

void __set_PRIMASK(uint32_t primask)
{
    (void)primask;
}

static void setup(uint32_t timeout_ms)
{
    memset(&fake, 0, sizeof(fake));
    memset(&rx_dma, 0, sizeof(rx_dma));
    handle.hdmarx = &rx_dma;
    fake.receive_status = HAL_OK;
    fake.transmit_status = HAL_OK;
    fake.abort_receive_status = HAL_OK;
    fake.abort_transmit_status = HAL_OK;
    emm_v5_uart_init(&uart, &handle, timeout_ms);
}

static void start_request(size_t expected_length)
{
    const uint8_t frame[] = {0x01U, 0x36U, 0x6BU};
    CHECK_TRUE(emm_v5_uart_send(&uart, frame, sizeof(frame), 0x36U,
                                expected_length) == BALANCE_MOTOR_TX_ACCEPTED);
}

static EmmV5UartResult take_result(void)
{
    EmmV5UartResult result = {0};
    CHECK_TRUE(emm_v5_uart_take_result(&uart, &result));
    return result;
}

static void test_initialization_is_idle(void)
{
    EmmV5UartResult result;

    setup(20U);
    CHECK_TRUE(uart.state == EMM_V5_UART_IDLE);
    CHECK_TRUE(fake.call_count == 0U);
    CHECK_TRUE(!emm_v5_uart_take_result(&uart, &result));
}

static void test_send_copies_frame_and_arms_rx_first(void)
{
    uint8_t caller_frame[] = {0x01U, 0x36U, 0x6BU};

    setup(20U);
    CHECK_TRUE(emm_v5_uart_send(&uart, caller_frame, sizeof(caller_frame),
                                0x36U, 8U) == BALANCE_MOTOR_TX_ACCEPTED);
    caller_frame[0] = 0xFFU;
    CHECK_TRUE(fake.call_count == 2U);
    CHECK_TRUE(fake.calls[0] == FAKE_CALL_RECEIVE_TO_IDLE_DMA);
    CHECK_TRUE(fake.calls[1] == FAKE_CALL_TRANSMIT_DMA);
    CHECK_TRUE(fake.tx_data[0] == 0x01U);
    CHECK_TRUE(fake.tx_size == sizeof(caller_frame));
    CHECK_TRUE(fake.rx_size == 8U);
    CHECK_TRUE((rx_dma.disabled_interrupts & DMA_IT_HT) != 0U);
    CHECK_TRUE(emm_v5_uart_send(&uart, caller_frame, sizeof(caller_frame),
                                0x36U, 8U) == BALANCE_MOTOR_TX_BUSY);
}

static void test_early_response_cannot_release_tx_storage(void)
{
    EmmV5UartResult result;
    const uint8_t next_frame[] = {0x02U, 0x36U, 0x6BU};
    const uint8_t response[] = {0x01U, 0x36U, 0U, 0U, 0U, 1U, 2U, 0x6BU};

    setup(20U);
    start_request(sizeof(response));
    memcpy(fake.rx_data, response, sizeof(response));
    emm_v5_uart_on_rx_event(&uart, sizeof(response));
    emm_v5_uart_poll(&uart, 0U);
    CHECK_TRUE(uart.state == EMM_V5_UART_ACTIVE);
    CHECK_TRUE(!emm_v5_uart_take_result(&uart, &result));
    CHECK_TRUE(emm_v5_uart_send(&uart, next_frame, sizeof(next_frame), 0x36U,
                                sizeof(response)) == BALANCE_MOTOR_TX_BUSY);
    CHECK_TRUE(fake.tx_data[0] == 0x01U);

    emm_v5_uart_on_tx_complete(&uart);
    emm_v5_uart_poll(&uart, 0U);
    if (emm_v5_uart_take_result(&uart, &result)) {
        CHECK_TRUE(result.state == EMM_V5_UART_COMPLETE);
        CHECK_TRUE(result.response_length == sizeof(response));
        CHECK_TRUE(result.expected_function == 0x36U);
        CHECK_TRUE(result.response != NULL);
        if (result.response != NULL) {
            CHECK_TRUE(memcmp(result.response, response, sizeof(response)) == 0);
        }
    } else {
        CHECK_TRUE(false);
    }
    CHECK_TRUE(uart.state == EMM_V5_UART_IDLE);
}

static void test_short_and_wrong_function_are_protocol_errors(void)
{
    EmmV5UartResult result;

    setup(20U);
    start_request(8U);
    fake.rx_data[0] = 0x01U;
    emm_v5_uart_on_rx_event(&uart, 1U);
    emm_v5_uart_on_tx_complete(&uart);
    emm_v5_uart_poll(&uart, 0U);
    result = take_result();
    CHECK_TRUE(result.state == EMM_V5_UART_PROTOCOL_ERROR);
    CHECK_TRUE(result.response_length == 1U);

    start_request(4U);
    memcpy(fake.rx_data, (const uint8_t[]){0x01U, 0xFDU, 0x02U, 0x6BU}, 4U);
    emm_v5_uart_on_rx_event(&uart, 4U);
    emm_v5_uart_on_tx_complete(&uart);
    emm_v5_uart_poll(&uart, 0U);
    result = take_result();
    CHECK_TRUE(result.state == EMM_V5_UART_PROTOCOL_ERROR);
    CHECK_TRUE(result.response_length == 4U);
}

static void test_oversized_rx_event_never_exposes_length_beyond_storage(void)
{
    EmmV5UartResult result;

    setup(20U);
    start_request(8U);
    emm_v5_uart_on_rx_event(&uart, EMM_V5_MAX_FRAME_SIZE + 1U);
    emm_v5_uart_on_tx_complete(&uart);
    emm_v5_uart_poll(&uart, 0U);
    result = take_result();
    CHECK_TRUE(result.state == EMM_V5_UART_PROTOCOL_ERROR);
    CHECK_TRUE(result.response_length == EMM_V5_MAX_FRAME_SIZE);
}

static void test_timeout_handles_tick_wrap(void)
{
    EmmV5UartResult result;

    setup(20U);
    fake.tick = UINT32_MAX - 9U;
    start_request(8U);
    emm_v5_uart_on_tx_complete(&uart);
    emm_v5_uart_poll(&uart, 5U);
    CHECK_TRUE(!emm_v5_uart_take_result(&uart, &result));
    emm_v5_uart_poll(&uart, 10U);
    result = take_result();
    CHECK_TRUE(result.state == EMM_V5_UART_TIMEOUT);
    CHECK_TRUE(fake.calls[fake.call_count - 1U] == FAKE_CALL_ABORT_RECEIVE);
}

static void test_uart_error_is_published_from_poll(void)
{
    EmmV5UartResult result;

    setup(20U);
    start_request(8U);
    emm_v5_uart_on_tx_complete(&uart);
    emm_v5_uart_on_error(&uart);
    CHECK_TRUE(uart.state == EMM_V5_UART_ACTIVE);
    emm_v5_uart_poll(&uart, 0U);
    result = take_result();
    CHECK_TRUE(result.state == EMM_V5_UART_HAL_ERROR);
    CHECK_TRUE(uart.state == EMM_V5_UART_IDLE);
}

static void test_error_only_callback_aborts_tx_and_publishes_hal_error(void)
{
    EmmV5UartResult result;

    setup(20U);
    start_request(8U);
    emm_v5_uart_on_error(&uart);
    emm_v5_uart_poll(&uart, 0U);
    result = take_result();
    CHECK_TRUE(result.state == EMM_V5_UART_HAL_ERROR);
    CHECK_TRUE(fake.calls[fake.call_count - 1U] == FAKE_CALL_ABORT_TRANSMIT);
    CHECK_TRUE(uart.state == EMM_V5_UART_IDLE);
}

static void test_failed_tx_abort_quarantines_until_retry_succeeds(void)
{
    EmmV5UartResult result;
    const uint8_t frame[] = {0x02U, 0x36U, 0x6BU};

    setup(20U);
    start_request(8U);
    fake.abort_transmit_status = HAL_ERROR;
    emm_v5_uart_on_error(&uart);
    emm_v5_uart_poll(&uart, 0U);
    CHECK_TRUE(!emm_v5_uart_take_result(&uart, &result));
    CHECK_TRUE(emm_v5_uart_send(&uart, frame, sizeof(frame), 0x36U, 8U) ==
               BALANCE_MOTOR_TX_BUSY);

    fake.abort_transmit_status = HAL_OK;
    emm_v5_uart_poll(&uart, 1U);
    result = take_result();
    CHECK_TRUE(result.state == EMM_V5_UART_HAL_ERROR);
    CHECK_TRUE(uart.state == EMM_V5_UART_IDLE);
}

static void test_rx_side_error_does_not_release_unaborted_tx_storage(void)
{
    EmmV5UartResult result;
    const uint8_t next_frame[] = {0x02U, 0x36U, 0x6BU};

    setup(20U);
    start_request(8U);
    fake.rx_data[0] = 0x01U;
    emm_v5_uart_on_rx_event(&uart, 1U);
    fake.abort_transmit_status = HAL_ERROR;
    emm_v5_uart_on_error(&uart);
    emm_v5_uart_poll(&uart, 0U);
    CHECK_TRUE(!emm_v5_uart_take_result(&uart, &result));
    CHECK_TRUE(emm_v5_uart_send(&uart, next_frame, sizeof(next_frame), 0x36U,
                                8U) == BALANCE_MOTOR_TX_BUSY);
    CHECK_TRUE(fake.tx_data[0] == 0x01U);
}

static void test_failed_abort_quarantines_until_recovery(void)
{
    EmmV5UartResult result;
    const uint8_t frame[] = {0x01U, 0x36U, 0x6BU};

    setup(20U);
    start_request(8U);
    emm_v5_uart_on_tx_complete(&uart);
    fake.abort_receive_status = HAL_ERROR;
    emm_v5_uart_poll(&uart, 20U);
    CHECK_TRUE(!emm_v5_uart_take_result(&uart, &result));
    CHECK_TRUE(emm_v5_uart_send(&uart, frame, sizeof(frame), 0x36U, 8U) ==
               BALANCE_MOTOR_TX_BUSY);

    fake.rx_data[0] = 0x01U;
    emm_v5_uart_on_rx_event(&uart, 1U);
    fake.abort_receive_status = HAL_OK;
    emm_v5_uart_poll(&uart, 21U);
    result = take_result();
    CHECK_TRUE(result.state == EMM_V5_UART_HAL_ERROR);
    CHECK_TRUE(uart.state == EMM_V5_UART_IDLE);
}

static void test_result_bytes_survive_next_receive(void)
{
    EmmV5UartResult first;
    const uint8_t response[] = {0x01U, 0x36U, 0U, 0U, 0U, 1U, 2U, 0x6BU};

    setup(20U);
    start_request(sizeof(response));
    memcpy(fake.rx_data, response, sizeof(response));
    emm_v5_uart_on_rx_event(&uart, sizeof(response));
    emm_v5_uart_on_tx_complete(&uart);
    emm_v5_uart_poll(&uart, 0U);
    first = take_result();

    start_request(sizeof(response));
    memset(fake.rx_data, 0xA5, sizeof(response));
    CHECK_TRUE(memcmp(first.response, response, sizeof(response)) == 0);
}

static void test_stale_callbacks_after_terminal_do_not_affect_next_request(void)
{
    EmmV5UartResult result;
    const uint8_t response[] = {0x01U, 0x36U, 0U, 0U, 0U, 1U, 2U, 0x6BU};

    setup(20U);
    start_request(sizeof(response));
    emm_v5_uart_on_tx_complete(&uart);
    emm_v5_uart_poll(&uart, 20U);
    result = take_result();
    CHECK_TRUE(result.state == EMM_V5_UART_TIMEOUT);

    emm_v5_uart_on_rx_event(&uart, 1U);
    emm_v5_uart_on_error(&uart);
    fake.tick = 21U;
    start_request(sizeof(response));
    emm_v5_uart_poll(&uart, 21U);
    CHECK_TRUE(!emm_v5_uart_take_result(&uart, &result));

    memcpy(fake.rx_data, response, sizeof(response));
    emm_v5_uart_on_rx_event(&uart, sizeof(response));
    emm_v5_uart_on_tx_complete(&uart);
    emm_v5_uart_poll(&uart, 21U);
    result = take_result();
    CHECK_TRUE(result.state == EMM_V5_UART_COMPLETE);
}

static void test_hal_start_failures_publish_errors(void)
{
    EmmV5UartResult result;

    setup(20U);
    fake.receive_status = HAL_ERROR;
    CHECK_TRUE(emm_v5_uart_send(&uart, (const uint8_t[]){1U}, 1U, 0x36U,
                                8U) == BALANCE_MOTOR_TX_FAILED);
    result = take_result();
    CHECK_TRUE(result.state == EMM_V5_UART_HAL_ERROR);

    fake.receive_status = HAL_OK;
    fake.transmit_status = HAL_ERROR;
    CHECK_TRUE(emm_v5_uart_send(&uart, (const uint8_t[]){1U}, 1U, 0x36U,
                                8U) == BALANCE_MOTOR_TX_FAILED);
    CHECK_TRUE(fake.calls[fake.call_count - 1U] == FAKE_CALL_ABORT_RECEIVE);
    result = take_result();
    CHECK_TRUE(result.state == EMM_V5_UART_HAL_ERROR);
    CHECK_TRUE(uart.state == EMM_V5_UART_IDLE);
}

static void test_invalid_lengths_are_rejected_without_hal(void)
{
    uint8_t frame[EMM_V5_MAX_FRAME_SIZE + 1U] = {0};

    setup(20U);
    CHECK_TRUE(emm_v5_uart_send(&uart, NULL, 1U, 0x36U, 8U) ==
               BALANCE_MOTOR_TX_FAILED);
    CHECK_TRUE(emm_v5_uart_send(&uart, frame, 0U, 0x36U, 8U) ==
               BALANCE_MOTOR_TX_FAILED);
    CHECK_TRUE(emm_v5_uart_send(&uart, frame, sizeof(frame), 0x36U, 8U) ==
               BALANCE_MOTOR_TX_FAILED);
    CHECK_TRUE(emm_v5_uart_send(&uart, frame, 1U, 0x36U,
                                EMM_V5_MAX_FRAME_SIZE + 1U) ==
               BALANCE_MOTOR_TX_FAILED);
    CHECK_TRUE(fake.call_count == 0U);
}

int main(void)
{
    test_initialization_is_idle();
    test_send_copies_frame_and_arms_rx_first();
    test_early_response_cannot_release_tx_storage();
    test_short_and_wrong_function_are_protocol_errors();
    test_oversized_rx_event_never_exposes_length_beyond_storage();
    test_timeout_handles_tick_wrap();
    test_uart_error_is_published_from_poll();
    test_error_only_callback_aborts_tx_and_publishes_hal_error();
    test_failed_tx_abort_quarantines_until_retry_succeeds();
    test_rx_side_error_does_not_release_unaborted_tx_storage();
    test_failed_abort_quarantines_until_recovery();
    test_result_bytes_survive_next_receive();
    test_stale_callbacks_after_terminal_do_not_affect_next_request();
    test_hal_start_failures_publish_errors();
    test_invalid_lengths_are_rejected_without_hal();
    printf("%s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
