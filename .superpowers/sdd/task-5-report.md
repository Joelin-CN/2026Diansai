# Task 5 Report: Generic Camera Measurement Guard

## Status

Implemented the protocol-independent decoded camera sample guard from base commit
`90db1ad`. The implementation was committed as `6b079c8`.

## Files

- `App/Inc/balance_measurement.h`
- `App/Src/balance_measurement.c`
- `tests/CMakeLists.txt`
- `tests/test_balance.c`

No byte parser, UART code, task ledger, or earlier balance module was modified.

## RED Evidence

After adding measurement tests and the host CMake source entry, ran:

```powershell
cmake --build build/host-tests
```

The command failed during CMake generation because the production module was
still absent:

```text
Cannot find source file:
  ../App/Src/balance_measurement.c
No SOURCES given to target: test_balance
```

This confirmed the host target depended on the missing Task 5 implementation.

## GREEN Evidence

After adding the minimal measurement header and implementation, ran:

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

## Coverage

- Rejects invalid and non-finite decoded positions.
- Classifies duplicate and stale sequences separately.
- Accepts sequence wrap from `UINT32_MAX` to zero using half-range ordering.
- Rejects positions outside the configured physical range.
- Rejects jumps beyond the configured threshold from the last accepted sample.
- Keeps rejected samples from updating guard state or refreshing timeout state.
- Applies an inclusive timeout boundary and wrap-safe timestamp subtraction.
- Reports no timeout before the first accepted sample.

## Self-Review

- Validation is independent of camera protocol, byte parsing, and transport.
- Sequence ordering avoids implementation-defined unsigned-to-signed conversion.
- Only accepted samples replace the stored sequence, position, and timestamp.
- `git diff --check` reported no whitespace errors; Git emitted only existing
  line-ending conversion warnings for tracked test files.
- No Critical or Important findings remain.

## Commit

```text
6b079c8 feat: validate balance camera measurements
```
