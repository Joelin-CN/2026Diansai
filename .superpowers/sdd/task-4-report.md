# Task 4 Report: Relative Stepper Command Limiter

## Status

Implemented the protocol-independent actuator command limiter from base commit
`41c9058`.

## Files

- `App/Inc/balance_actuator.h`
- `App/Src/balance_actuator.c`
- `tests/CMakeLists.txt`
- `tests/test_balance.c`

No driver protocol or earlier balance module was modified. The task ledger was
not edited.

## RED Evidence

After adding the actuator tests and host CMake source entry, ran:

```powershell
cmake --build build/host-tests
```

The command failed during CMake generation because the production module was
still absent:

```text
Cannot find source file:
  ../App/Src/balance_actuator.c
No SOURCES given to target: test_balance
```

This confirmed the test target depended on the missing Task 4 implementation.

## GREEN Evidence

After adding the minimal actuator header and implementation, ran:

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

The host target compiles with `-Wall -Wextra -Werror`.

## Self-Review

- Applies configured actuator direction before limiting.
- Clamps the signed target to the absolute position limit.
- Clamps each emitted position change to the per-frame slew limit.
- Preserves the emitted position as state and resets that state to zero.
- Returns configured speed and acceleration without adding driver protocol.
- Reports absolute-position and slew limiting independently.
- `git diff --check` reported no whitespace errors.

## Concerns

The API assumes valid non-null pointers and non-negative limit configuration,
as specified by the brief. Input validation and non-finite float handling are
outside Task 4 scope.
