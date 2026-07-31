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
