# Task 2 Report: Ramped Position Target

## Status

Implemented competition target validation and rate-limited position target slew while preserving the Task 1 observer interfaces and host test executable.

## TDD Evidence

### RED

Command:

```powershell
cmake --build build/host-tests
```

Relevant output:

```text
CMake Error at CMakeLists.txt:7 (add_executable):
  Cannot find source file:

    ../App/Src/balance_target.c

CMake Generate step failed.
mingw32-make.exe: *** [Makefile:216: cmake_check_build_system] Error 1
```

The failure occurred during CMake source validation before compilation could report the brief's predicted missing `balance_target.h`; both failures have the same cause: the target module did not yet exist.

### GREEN

Command:

```powershell
cmake --build build/host-tests; if ($?) { ctest --test-dir build/host-tests --output-on-failure }
```

Relevant output:

```text
[100%] Built target test_balance
1/1 Test #1: balance_core .....................   Passed    0.06 sec

100% tests passed, 0 tests failed out of 1
```

## Files

- `App/Inc/balance_target.h`: declares target state and init/select/step/reset API.
- `App/Src/balance_target.c`: accepts only `-5.0f`, `0.0f`, and `5.0f`; slews without overshoot.
- `tests/CMakeLists.txt`: adds the target implementation to the existing host test executable.
- `tests/test_balance.c`: adds invalid-selection and no-overshoot ramp tests.

## Self-Review

- Compared the implementation and tests line-by-line with the Task 2 brief.
- Confirmed rejected selections leave the initialized request unchanged.
- Confirmed the ramp advances at `max_rate_cm_s * dt_s`, clamps at the request, and remains there.
- Confirmed Task 1 observer tests and interfaces remain unchanged.
- Ran `git diff --check` on all four task files; no whitespace errors were reported.
- Reviewed the scoped diff and status to ensure only the four task files will be committed.

## Concerns

- The specified API does not validate null pointers, negative `dt_s`, or negative `max_rate_cm_s`; callers must provide valid objects and non-negative rate/time values. No extra behavior was added beyond the brief.
- The RED diagnostic differed from the expected header error because CMake checks the newly listed source path first.
