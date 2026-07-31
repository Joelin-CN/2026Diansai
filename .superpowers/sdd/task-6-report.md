# Task 6 Report: Safety Supervisor

## Status

Implemented the pure-C safety supervisor from base commit `6b079c8`. The
implementation was committed as `7107807`.

## Files

- `App/Inc/balance_supervisor.h`
- `App/Src/balance_supervisor.c`
- `tests/CMakeLists.txt`
- `tests/test_balance.c`

No HAL, GPIO, previous balance module, or task ledger was modified.

## RED Evidence

After registering the supervisor source and adding the startup, approval, and
fault tests, ran:

```powershell
cmake --build build/host-tests
```

The build failed as expected because production code was still absent:

```text
Cannot find source file:
  ../App/Src/balance_supervisor.c
No SOURCES given to target: test_balance
```

## GREEN Evidence

After adding the minimal header and implementation, ran:

```powershell
cmake --build build/host-tests
ctest --test-dir build/host-tests --output-on-failure
```

Result:

```text
[100%] Built target test_balance
1/1 Test #1: balance_core ..................... Passed
100% tests passed, 0 tests failed out of 1
```

The combined host target compiles with `-Wall -Wextra -Werror`.

## Coverage

- Startup waits for explicit manual zero.
- Closed-loop entry is rejected before manual zero and approved open-loop check.
- Rejected open-loop checks do not advance to ready.
- Approved checks advance through ready to closed-loop.
- Faults enter the latched fault state and preserve the fault reason.
- Fault acknowledgement clears the reason and requires a new manual-zero cycle.

## Self-Review

- The supervisor depends only on standard C `stdbool.h`.
- State changes occur only through explicit operator/subsystem transition APIs.
- Fault state blocks normal transitions until acknowledgement.
- `git diff --check` reported no whitespace errors; Git emitted only existing
  line-ending conversion warnings for tracked test files.
- No Critical or Important findings remain.

## Commit

```text
7107807 feat: add balance safety supervisor
```

## Review Fix: First-Fault Latching

The review identified that `balance_supervisor_raise_fault` unconditionally
replaced the active fault and treated `BALANCE_FAULT_NONE` as a real fault.
The function now ignores `NONE` and ignores all new fault reports while the
supervisor is already faulted. The first non-`NONE` reason therefore remains
latched until `balance_supervisor_acknowledge_fault` clears it.

Focused regression coverage was added for:

- Repeated fault reports preserving the first reason.
- `BALANCE_FAULT_NONE` having no effect before or during a fault latch.
- Manual-zero, open-loop, closed-loop, and stop transitions being blocked
  while faulted.
- Stop transitioning `CLOSED_LOOP` back to `READY`.

### Review Fix RED

Ran before changing production code:

```powershell
cmake --build build/host-tests; ctest --test-dir build/host-tests --output-on-failure
```

Result:

```text
[100%] Built target test_balance
1/1 Test #1: balance_core .....................***Failed
FAIL tests/test_balance.c:348: supervisor.fault == BALANCE_FAULT_CAMERA_TIMEOUT
FAIL tests/test_balance.c:356: supervisor.state == BALANCE_STATE_WAIT_MANUAL_ZERO
FAIL tests/test_balance.c:361: supervisor.fault == BALANCE_FAULT_CAMERA_DATA
0% tests passed, 1 tests failed out of 1
```

### Review Fix GREEN

Ran after adding the fault guard:

```powershell
cmake --build build/host-tests; ctest --test-dir build/host-tests --output-on-failure
```

Result:

```text
[100%] Built target test_balance
1/1 Test #1: balance_core ..................... Passed
100% tests passed, 0 tests failed out of 1
Total Test time (real) = 0.01 sec
```

No `tests/CMakeLists.txt` change was required.

```text
84164dd fix: preserve first balance fault
```
