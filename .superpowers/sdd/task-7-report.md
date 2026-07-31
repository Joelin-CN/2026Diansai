# Task 7 Report: Closed-Loop Coordinator

## Status

Implemented the HAL/UART-independent closed-loop coordinator and integration tests.

## RED Evidence

- `cmake --build build/host-tests`
- Failed during CMake generation because `../App/Src/balance_loop.c` did not exist.
- Added telemetry fault-state assertions after the initial implementation; CTest then failed at `telemetry.state == BALANCE_STATE_FAULT` and `telemetry.fault == BALANCE_FAULT_MOTOR_COMMUNICATION`.

## GREEN Evidence

- `cmake --build build/host-tests`
- Result: `Built target test_balance` with `-Wall -Wextra -Werror` and no compiler warnings.
- `ctest --test-dir build/host-tests --output-on-failure`
- Result: `100% tests passed, 0 tests failed out of 1`.
- `git diff --check -- App/Inc/balance_loop.h App/Src/balance_loop.c tests/CMakeLists.txt tests/test_balance.c`
- Result: no whitespace errors; Git only reported the repository's LF-to-CRLF conversion warning for existing test files.

## Deviations From Brief Example

- `reset_control_state` does not invalidate telemetry. The explicit requirement says telemetry reflects the last emitted command, so rejected frames, stops, and faults preserve the last command snapshot.
- `balance_loop_get_telemetry` overlays the current supervisor state and first preserved fault onto that snapshot. This preserves emitted command data while accurately reporting a later fault.
- Added integration coverage beyond the six sample tests for duplicate/stale/invalid rejection, accepted-only target integral reset, first-fault preservation, and telemetry preservation across faults.
- Factored repeated fault handling into a private `raise_fault` helper. It delegates first-fault selection to the Task 6 supervisor and consistently resets control state.

## Files

- Created `App/Inc/balance_loop.h`.
- Created `App/Src/balance_loop.c`.
- Modified `tests/CMakeLists.txt`.
- Modified `tests/test_balance.c`.

## Self-Review

- READY accepts guarded measurements and warms the observer.
- Start remains blocked until `BALANCE_OBSERVER_UPDATED`, requiring two suitably spaced accepted frames.
- CLOSED_LOOP emits at most once per accepted frame; duplicate, stale, invalid, observer-rejected, and observer-reset frames do not emit.
- Out-of-range/jump data, end-zone entry, timeout, sustained saturation, motor failure, and emergency stop enter fault handling and clear observer/controller/actuator/target state.
- Invalid target selection leaves integral state unchanged; accepted selection resets it.
- Supervisor remains authoritative for preserving the first fault.
- No HAL or UART dependency was introduced.
- Public interfaces from Tasks 1-6 were unchanged.

## Concerns

- Host coverage is integration-focused and passes, but embedded compiler/toolchain integration is outside this task's HAL-free scope.

## Review Fixes - 2026-07-31

Commit: `cc5dc84c503e276f6c9198a0cb76839332565160` (`fix: harden balance loop edge cases`)

### Changes

- Observer long-gap resets now revoke camera readiness until a later normal update.
- Telemetry is fully zero-initialized before the first emitted command and remains preserved after an emission across faults/stops.
- Coordinator tests cover out-of-range and jump camera-data faults without command emission, including first-fault preservation.
- `saturation_frame_limit == 0` disables sustained-saturation monitoring.

### Files In Commit

- `App/Src/balance_loop.c`
- `tests/test_balance.c`

### Exact Test Evidence

Command:

```powershell
cmake --build build/host-tests
```

Output:

```text
[100%] Built target test_balance
```

Command:

```powershell
ctest --test-dir build/host-tests --output-on-failure
```

Output:

```text
Test project E:/B306/2026/diansai/2026task/stm32/balanceBall/balanceBall/build/host-tests
    Start 1: balance_core
1/1 Test #1: balance_core .....................   Passed    0.01 sec

100% tests passed, 0 tests failed out of 1

Total Test time (real) =   0.03 sec
```
