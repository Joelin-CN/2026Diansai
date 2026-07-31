# Task 5 Report: Firmware Wiring and End-to-End Verification

## Status

Implemented USART2 firmware wiring, registered all three Emm V5 App sources in
the ARM target, and preserved inert startup behavior.

## Changes

- Added `emm_v5_protocol.c`, `balance_motor.c`, and `emm_v5_uart.c` to the root
  firmware target.
- Added global `g_emm_uart` and `g_balance_motor` instances in CubeMX USER CODE
  regions.
- Initialized the UART transport against `huart2` with a 100 ms timeout.
- Configured motor address `0x01`, three-failure lockout, and the required
  compile-time placeholder conversion of `1.0f` pulse per position unit.
- Configured `BalanceMotorTransport` with `emm_v5_uart_send` and
  `&g_emm_uart`.
- Polls UART, takes each terminal result once, routes complete responses with
  the result's expected function and payload, routes all other terminal states
  to the transport-error handler, then processes pending motor work.
- Added USART2-only HAL TX-complete, Receive-to-Idle, and error callback
  forwarding.
- Added startup regression coverage proving repeated processing does not send a
  frame or establish software zero.

## Startup Test Evidence

The startup test was added before firmware wiring and run against the existing
adapter implementation.

```powershell
cmake --build build/host-tests --target test_balance_motor
ctest --test-dir build/host-tests -R balance_motor --output-on-failure
```

```text
[100%] Built target test_balance_motor
1/1 Test #3: balance_motor .................... Passed
100% tests passed, 0 tests failed out of 1
```

This is a characterization/integration-safety assertion, so the brief expected
it to pass before firmware wiring rather than enter a production-code red phase.

## Host Verification

Commands:

```powershell
cmake -S tests -B build/host-tests
cmake --build build/host-tests
ctest --test-dir build/host-tests --output-on-failure
```

Result:

```text
1/4 Test #1: balance_core ..................... Passed
2/4 Test #2: emm_v5_protocol .................. Passed
3/4 Test #3: balance_motor .................... Passed
4/4 Test #4: emm_v5_uart ...................... Passed
100% tests passed, 0 tests failed out of 4
Total Test time (real) = 0.06 sec
```

The existing host cache and generator were retained as requested.

## ARM Verification

Commands:

```powershell
cmake --preset Debug
cmake --build --preset Debug --clean-first
```

Result:

```text
[33/33] Linking C executable balanceBall.elf
Memory region         Used Size  Region Size  %age Used
RAM                       2424 B        20 KB     11.84%
FLASH                    15456 B        64 KB     23.58%
```

Artifact: `build/Debug/balanceBall.elf` (776388 bytes).

## Source Hash Verification

Command:

```powershell
$hashes = Get-FileHash -Algorithm SHA256 "../temp/SM.c", "../temp/SM.h"
```

Results:

```text
SM.c  634421E0D84592FA447A77BD1C8AF8DD15CD7BC2794F49AD9C7CAD600E2D3F8F
SM.h  B586B830C9A682DC6AC8DD811CA0C7DE4D8F5ACA1F4F2B2AD300C280311C92B1
```

Both hashes match the required baseline. No `../temp` source or include path was
added to CMake.

## Diff and Self-Review

- `git diff --cached --check` completed with no output.
- Repository-wide `git diff --check` reports only pre-existing blank lines at
  EOF in unrelated `.superpowers/sdd/task-*-brief.md` files outside this task's
  working directory. Those user changes were not modified.
- Initialization calls only `emm_v5_uart_init()` and `balance_motor_init()`; it
  does not request zero, enable, home, set microstep, set zero, stop, disable, or
  submit position.
- The App driver path contains no `HAL_Delay`.
- Every callback checks `huart->Instance == USART2`; USART1 and USART3 are
  ignored.
- The polling path uses the actual `EmmV5UartResult` and
  `emm_v5_uart_take_result()` API from `emm_v5_uart.h`.
- `EMM_V5_UART_COMPLETE` routes to `balance_motor_on_response()` with
  `expected_function`, `response`, and `response_length`; timeout, protocol, and
  HAL error states route to `balance_motor_on_transport_error()`.
- No critical or important self-review findings remain.

## Concern

The `1.0f` pulse-per-position-unit value is intentionally only a compile-time
placeholder. It must be calibrated to the mechanism before any external caller
requests zero or submits real motion.

## Commit

Commit: `6f598df` (`build: integrate Emm V5 motor driver`).

The report remains an untracked task artifact because the required Task 5
commit stages only `CMakeLists.txt`, `Core/Src/main.c`, and
`tests/test_balance_motor.c`.
