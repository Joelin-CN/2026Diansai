### Task 4: USART2 DMA Transaction Layer

**Files:**
- Create: `App/Inc/emm_v5_uart.h`
- Create: `App/Src/emm_v5_uart.c`
- Create: `tests/stubs/main.h`
- Create: `tests/stubs/stm32f1xx_hal.h`
- Create: `tests/test_emm_v5_uart.c`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: STM32 HAL UART/DMA calls and the `BalanceMotorSendFn` signature from Task 3.
- Produces: `EmmV5Uart`, `emm_v5_uart_init()`, `emm_v5_uart_send()`, `emm_v5_uart_poll()`, `emm_v5_uart_take_result()`, `emm_v5_uart_on_tx_complete()`, `emm_v5_uart_on_rx_event()`, and `emm_v5_uart_on_error()`. The send function has the exact injected-transport signature `BalanceMotorTxResult emm_v5_uart_send(void *context, const uint8_t *frame, size_t length, uint8_t expected_function, size_t expected_length)` and casts `context` to `EmmV5Uart *` internally.

- [ ] **Step 1: Add failing host tests around fake HAL calls**

Provide just enough HAL declarations in `tests/stubs` for the transport source to compile. Fake `HAL_UART_Transmit_DMA`, `HAL_UARTEx_ReceiveToIdle_DMA`, and `HAL_UART_AbortReceive` and record arguments.

Register `test_emm_v5_uart` with `tests/stubs` before `../App/Inc` in `target_include_directories()`, so the host-only `main.h` and HAL declarations are selected while the firmware build continues using `Core/Inc/main.h` and the real HAL.

Test these transitions:

- Initialization is idle and starts no transfer.
- Accepted send copies caller bytes into internal TX storage before calling HAL.
- A second send while active returns `BALANCE_MOTOR_TX_BUSY`.
- RX DMA starts before TX DMA so a fast ACK cannot be missed.
- TX completion does not complete a response-bearing request.
- RX event completes only at the configured exact length and preserves bytes for application retrieval.
- Wrong/short receive becomes a protocol result rather than reading beyond received data.
- `(uint32_t)(now_ms - deadline_ms) < 0x80000000U` style deadline handling remains correct over tick wrap.
- UART error and HAL start failure produce terminal error results and return to idle after result retrieval.

Core assertion:

```c
static void test_send_copies_frame_and_arms_rx_first(void)
{
    uint8_t caller_frame[] = {0x01, 0x36, 0x6B};
    CHECK_TRUE(emm_v5_uart_send(&uart, caller_frame, sizeof(caller_frame),
                                0x36U, 8U) == BALANCE_MOTOR_TX_ACCEPTED);
    caller_frame[0] = 0xFFU;
    CHECK_TRUE(fake.calls[0] == FAKE_CALL_RECEIVE_TO_IDLE_DMA);
    CHECK_TRUE(fake.calls[1] == FAKE_CALL_TRANSMIT_DMA);
    CHECK_TRUE(fake.tx_data[0] == 0x01U);
}
```

- [ ] **Step 2: Run UART transport tests and verify failure**

Run `cmake --build build/host-tests --target test_emm_v5_uart`.

Expected: compilation fails because the UART transaction module does not exist.

- [ ] **Step 3: Implement one in-flight DMA transaction**

Use fixed internal arrays sized `EMM_V5_MAX_FRAME_SIZE`. `emm_v5_uart_send()` validates lengths, copies TX bytes, records expected function/response length/deadline, arms Receive-to-Idle DMA, then starts TX DMA. If TX start fails, abort RX and publish a HAL error result.

After starting Receive-to-Idle DMA, disable the RX DMA half-transfer interrupt with `__HAL_DMA_DISABLE_IT(uart->handle->hdmarx, DMA_IT_HT)`. Emm responses are short variable-length protocol frames; processing a half-buffer event as a terminal receive would create false short-frame errors.

Callbacks only set volatile event flags and received length. `emm_v5_uart_poll(now_ms)` consumes flags, validates terminal lengths, handles timeout, and stores a result object:

```c
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
```

For response-bearing commands, remain active until RX completes or timeout occurs. Do not call `HAL_Delay`. `emm_v5_uart_take_result()` copies the result to the caller and resets the object to idle, at which point the adapter may submit its latest queued target.

- [ ] **Step 4: Run all host tests**

Run `cmake --build build/host-tests; ctest --test-dir build/host-tests --output-on-failure`.

Expected: four CTest tests pass with no warnings.

- [ ] **Step 5: Commit the UART transaction layer**

```powershell
git add App/Inc/emm_v5_uart.h App/Src/emm_v5_uart.c tests/stubs tests/test_emm_v5_uart.c tests/CMakeLists.txt
git commit -m "feat: add Emm V5 USART DMA transport"
```

