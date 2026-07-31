# Task 4 Report: USART2 DMA Transaction Layer

## Status

Implemented a host-testable, one-in-flight Emm V5 UART DMA transaction layer using Task 3's exact `BalanceMotorSendFn` signature.

## Changes

- Added `EmmV5Uart`, terminal result states, initialization, send, poll, result retrieval, and ISR event callbacks.
- Copied outgoing frames into fixed internal storage and armed Receive-to-Idle DMA before starting TX DMA.
- Disabled RX DMA half-transfer interrupts after arming reception.
- Kept response-bearing requests active after TX completion until RX, timeout, or HAL error.
- Used volatile callback event fields and an interrupt-masked snapshot in `emm_v5_uart_poll()`.
- Used wrap-safe unsigned deadline comparison.
- Added host-only HAL stubs through test include ordering; firmware continues to resolve `main.h` and HAL from its normal include paths.
- Added focused coverage for start order, copying, busy handling, early RX, exact lengths, function mismatch, short/oversized RX, timeout wrap, and HAL failures.

## TDD Evidence

### RED: Missing Module

Command:

```powershell
cmake -S tests -B build/host-tests
cmake --build build/host-tests --target test_emm_v5_uart
```

Output:

```text
CMake Error at CMakeLists.txt:40 (add_executable):
  Cannot find source file:

    ../App/Src/emm_v5_uart.c

CMake Error at CMakeLists.txt:40 (add_executable):
  No SOURCES given to target: test_emm_v5_uart
```

### GREEN: Focused UART Test

Command:

```powershell
cmake --build build/host-tests --target test_emm_v5_uart
ctest --test-dir build/host-tests -R emm_v5_uart --output-on-failure
```

Output:

```text
[100%] Built target test_emm_v5_uart
1/1 Test #4: emm_v5_uart ......................   Passed
100% tests passed, 0 tests failed out of 1
```

### RED: Oversized Callback Length

Command:

```powershell
cmake --build build/host-tests --target test_emm_v5_uart
ctest --test-dir build/host-tests -R emm_v5_uart --output-on-failure
```

Output:

```text
FAIL tests/test_emm_v5_uart.c:190: result.response_length == EMM_V5_MAX_FRAME_SIZE
0% tests passed, 1 tests failed out of 1
```

### GREEN: Full Host Suite

Command:

```powershell
cmake --build build/host-tests
ctest --test-dir build/host-tests --output-on-failure
```

Output:

```text
[100%] Built target test_emm_v5_uart
1/4 Test #1: balance_core .....................   Passed
2/4 Test #2: emm_v5_protocol ..................   Passed
3/4 Test #3: balance_motor ....................   Passed
4/4 Test #4: emm_v5_uart ......................   Passed
100% tests passed, 0 tests failed out of 4
```

## Self-Review

- Confirmed all callback functions only publish event fields; terminal state changes occur in main context.
- Confirmed RX may arrive before TX completion without being discarded.
- Confirmed no response bytes are inspected unless at least two bytes were received.
- Confirmed callback lengths exposed to application code cannot exceed internal RX storage.
- Confirmed the host include path places `tests/stubs` before `App/Inc` and does not alter the firmware build generator.
- No unresolved critical or important findings.

## Concerns

- Host tests validate state transitions and HAL call ordering, but target integration still needs USART2 callback routing to these per-instance callbacks in a later task.

## Review Fixes - 2026-07-31

### Changes

- Early RX now latches its terminal result internally but remains `ACTIVE`; `take_result()` and a replacement send stay unavailable until TX completion releases `tx_storage`.
- Terminal timeout/error processing checks `HAL_UART_AbortReceive()`. Abort failure escalates to `HAL_ERROR`, quarantines the transaction, and retries recovery from later polls before allowing reuse.
- Completed response bytes are copied into `result_storage`, so the pointer returned by the unchanged `emm_v5_uart_take_result(EmmV5Uart *, EmmV5UartResult *)` API is not aliased by the next RX DMA transfer.
- Callback ownership is armed only for an active HAL transaction, disarmed at terminal publication, and atomically cleared before reuse. Terminal latching is monotonic, with only escalation to `HAL_ERROR`, so stale callback data cannot replace an established terminal cause.
- Public compatibility is unchanged: `emm_v5_uart_send()` still exactly matches Task 3's `BalanceMotorSendFn`, and no Task 5 API change is required.

### RED: Review Regressions

Command:

```powershell
cmake --build build/host-tests --target test_emm_v5_uart
& "build/host-tests/test_emm_v5_uart.exe"
```

Output before the transaction ownership fix:

```text
[100%] Built target test_emm_v5_uart
FAIL tests/test_emm_v5_uart.c:150: uart.state == EMM_V5_UART_ACTIVE
FAIL tests/test_emm_v5_uart.c:151: !emm_v5_uart_take_result(&uart, &result)
FAIL tests/test_emm_v5_uart.c:153: emm_v5_uart_send(&uart, next_frame, sizeof(next_frame), 0x36U, sizeof(response)) == BALANCE_MOTOR_TX_BUSY
FAIL tests/test_emm_v5_uart.c:154: fake.tx_data[0] == 0x01U
FAIL tests/test_emm_v5_uart.c:167: false
FAIL tests/test_emm_v5_uart.c:169: uart.state == EMM_V5_UART_IDLE
FAIL tests/test_emm_v5_uart.c:251: !emm_v5_uart_take_result(&uart, &result)
FAIL tests/test_emm_v5_uart.c:253: emm_v5_uart_send(&uart, frame, sizeof(frame), 0x36U, 8U) == BALANCE_MOTOR_TX_BUSY
FAIL tests/test_emm_v5_uart.c:258: result.state == EMM_V5_UART_HAL_ERROR
FAIL tests/test_emm_v5_uart.c:277: memcmp(first.response, response, sizeof(response)) == 0
FAIL
```

Output after adding stale RX during abort recovery:

```text
[100%] Built target test_emm_v5_uart
FAIL tests/test_emm_v5_uart.c:260: result.state == EMM_V5_UART_HAL_ERROR
FAIL
```

### GREEN: Focused UART Test

Command:

```powershell
cmake --build build/host-tests --target test_emm_v5_uart
ctest --test-dir build/host-tests -R "^emm_v5_uart$" --output-on-failure
```

Output:

```text
[100%] Built target test_emm_v5_uart
1/1 Test #4: emm_v5_uart ......................   Passed    0.01 sec
100% tests passed, 0 tests failed out of 1
Total Test time (real) =   0.01 sec
```

### GREEN: Full Host Suite

Command:

```powershell
cmake --build build/host-tests
ctest --test-dir build/host-tests --output-on-failure
```

Output:

```text
[ 47%] Built target test_balance
[ 63%] Built target test_emm_v5_protocol
[ 84%] Built target test_balance_motor
[100%] Built target test_emm_v5_uart
1/4 Test #1: balance_core .....................   Passed    0.01 sec
2/4 Test #2: emm_v5_protocol ..................   Passed    0.01 sec
3/4 Test #3: balance_motor ....................   Passed    0.01 sec
4/4 Test #4: emm_v5_uart ......................   Passed    0.01 sec
100% tests passed, 0 tests failed out of 4
Total Test time (real) =   0.05 sec
```

## Final TX Error Recovery Fix - 2026-07-31

### Changes

- Added explicit `HAL_UART_AbortTransmit()` recovery when a HAL error is latched while TX DMA still owns `tx_storage`.
- TX abort is state-aware: normal response and protocol-error paths still require TX completion, while a UART error may release TX ownership only after a successful HAL TX abort.
- Failed TX abort leaves the transaction `ACTIVE`, keeps new sends `BUSY`, and is retried by later `emm_v5_uart_poll()` calls.
- Extended only the host HAL stub and fake; firmware continues to use the real STM32 HAL declaration.
- Public Task 3/Task 5-facing APIs remain unchanged.

### RED: Error-Only TX Ownership

Command:

```powershell
cmake --build build/host-tests --target test_emm_v5_uart
& "build/host-tests/test_emm_v5_uart.exe"
```

Output:

```text
[100%] Built target test_emm_v5_uart
FAIL tests/test_emm_v5_uart.c:116: emm_v5_uart_take_result(&uart, &result)
FAIL tests/test_emm_v5_uart.c:260: result.state == EMM_V5_UART_HAL_ERROR
FAIL tests/test_emm_v5_uart.c:261: fake.calls[fake.call_count - 1U] == FAKE_CALL_ABORT_TRANSMIT
FAIL tests/test_emm_v5_uart.c:262: uart.state == EMM_V5_UART_IDLE
FAIL tests/test_emm_v5_uart.c:116: emm_v5_uart_take_result(&uart, &result)
FAIL tests/test_emm_v5_uart.c:282: result.state == EMM_V5_UART_HAL_ERROR
FAIL tests/test_emm_v5_uart.c:283: uart.state == EMM_V5_UART_IDLE
FAIL
```

### GREEN: Focused UART Test

Command:

```powershell
cmake --build build/host-tests --target test_emm_v5_uart
ctest --test-dir build/host-tests -R "^emm_v5_uart$" --output-on-failure
```

Output:

```text
[100%] Built target test_emm_v5_uart
1/1 Test #4: emm_v5_uart ......................   Passed    0.02 sec
100% tests passed, 0 tests failed out of 1
Total Test time (real) =   0.02 sec
```

### GREEN: Full Host Suite

Command:

```powershell
cmake --build build/host-tests
ctest --test-dir build/host-tests --output-on-failure
```

Output:

```text
[ 47%] Built target test_balance
[ 63%] Built target test_emm_v5_protocol
[ 84%] Built target test_balance_motor
[100%] Built target test_emm_v5_uart
1/4 Test #1: balance_core .....................   Passed    0.01 sec
2/4 Test #2: emm_v5_protocol ..................   Passed    0.01 sec
3/4 Test #3: balance_motor ....................   Passed    0.01 sec
4/4 Test #4: emm_v5_uart ......................   Passed    0.01 sec
100% tests passed, 0 tests failed out of 4
Total Test time (real) =   0.04 sec
```
