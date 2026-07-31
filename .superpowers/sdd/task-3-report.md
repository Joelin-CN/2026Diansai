# Task 3 Report: Position and Velocity Feedback Controller

## Status

Implemented and committed.

## Commit

- `41c9058 feat: add balance state feedback controller`

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

    ../App/Src/balance_controller.c

CMake Error at CMakeLists.txt:7 (add_executable):
  No SOURCES given to target: test_balance
```

The build failed because the controller implementation referenced by the new tests did not exist.

### GREEN

Command:

```powershell
cmake --build build/host-tests; if ($?) { ctest --test-dir build/host-tests --output-on-failure }
```

Relevant output:

```text
[100%] Built target test_balance
1/1 Test #1: balance_core .....................   Passed
100% tests passed, 0 tests failed out of 1
```

## Implementation

- Added the controller configuration, state, output, initialization, reset, and step APIs.
- Computes position error and proportional feedback with velocity damping.
- Integrates only when enabled and the position error is inside the configured zone.
- Clamps integral state and controller output to their configured symmetric limits.
- Rejects candidate integration when it would push farther into same-direction output saturation.
- Reports raw output, limited output, and saturation state.
- Added the controller to the combined host test target and preserved prior observer and target tests.

## Files

- `App/Inc/balance_controller.h`
- `App/Src/balance_controller.c`
- `tests/CMakeLists.txt`
- `tests/test_balance.c`

This report is intentionally not part of the implementation commit.

## Self-Review

- Compared the implementation and tests line-by-line with `task-3-brief.md`.
- Confirmed no observer or target source changes relative to base `49335e0`.
- Confirmed the commit contains only the four prescribed task files.
- `git diff --cached --check` was clean before commit.
- Anti-windup rejects only integration that deepens saturation, allowing integral action that can reduce saturation.

## Concerns

- None within the briefed scope.
- Configuration and pointer validation are intentionally absent because the exact brief does not specify them.
