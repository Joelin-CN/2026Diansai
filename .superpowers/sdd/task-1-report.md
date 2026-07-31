# Task 1 Report: Alpha-Beta Ball-State Observer

## Implementation Summary

Implemented a protocol-independent alpha-beta observer for signed camera ball position measurements in centimeters and STM32-style `uint32_t` receive timestamps in milliseconds. The observer initializes from the first sample, uses the measured frame interval for alpha-beta correction, rejects samples below the configured minimum interval, and resets position/velocity after gaps above the configured maximum interval.

Added a native C11 CMake/CTest harness compiled with `-Wall -Wextra -Werror` and three focused behavior tests.

## Files Changed

- `App/Inc/balance_observer.h`: observer configuration, estimate, result, state, and function declarations.
- `App/Src/balance_observer.c`: initialization, reset, timestamp filtering, gap reset, and alpha-beta update implementation.
- `tests/CMakeLists.txt`: MinGW-compatible native test target and CTest registration.
- `tests/test_balance.c`: minimal runner and observer behavior tests.

No unrelated files were modified, staged, reverted, or committed. The SDD ledger was not modified.

## RED

### Command

```powershell
cmake -S tests -B build/host-tests -G "MinGW Makefiles"
cmake --build build/host-tests
```

### Output

```text
-- The C compiler identification is GNU 8.1.0
-- Configuring done (0.8s)
-- Generating done (0.0s)
-- Build files have been written to: E:/B306/2026/diansai/2026task/stm32/balanceBall/balanceBall/build/host-tests
[ 33%] Building C object CMakeFiles/test_balance.dir/test_balance.c.obj
E:\B306\2026\diansai\2026task\stm32\balanceBall\balanceBall\tests\test_balance.c:3:10: fatal error: balance_observer.h: No such file or directory
 #include "balance_observer.h"
          ^~~~~~~~~~~~~~~~~~~~
compilation terminated.
mingw32-make.exe[2]: *** [CMakeFiles\test_balance.dir\build.make:79: CMakeFiles/test_balance.dir/test_balance.c.obj] Error 1
mingw32-make.exe[1]: *** [CMakeFiles\Makefile2:86: CMakeFiles/test_balance.dir/all] Error 2
mingw32-make.exe: *** [Makefile:100: all] Error 2
```

### Reason

The host tests included the wished-for observer API before it existed. Compilation failed at the expected missing `balance_observer.h` include, proving the tests could not pass without adding the observer interface and implementation.

## GREEN

### Command

```powershell
cmake --build build/host-tests
ctest --test-dir build/host-tests --output-on-failure
```

### Output

```text
[ 33%] Building C object CMakeFiles/test_balance.dir/test_balance.c.obj
[ 66%] Building C object CMakeFiles/test_balance.dir/E_/B306/2026/diansai/2026task/stm32/balanceBall/balanceBall/App/Src/balance_observer.c.obj
[100%] Linking C executable test_balance.exe
[100%] Built target test_balance
Test project E:/B306/2026/diansai/2026task/stm32/balanceBall/balanceBall/build/host-tests
    Start 1: balance_core
1/1 Test #1: balance_core .....................   Passed    0.02 sec

100% tests passed, 0 tests failed out of 1

Total Test time (real) =   0.02 sec
```

## Self-Review

- Compared all four task files with the task brief; interfaces, constants, test cases, CMake settings, and implementation match the specified content.
- Verified first-sample behavior returns `BALANCE_OBSERVER_RESET`, adopts measured position, and clears velocity.
- Verified accepted updates derive `dt_s` from actual unsigned timestamp difference and apply the required alpha-beta equations.
- Verified too-fast samples return the unchanged estimate without advancing the accepted timestamp.
- Verified long gaps adopt the new position, clear velocity, advance timestamp, and return reset.
- Confirmed unsigned timestamp subtraction preserves normal STM32 `uint32_t` tick-wrap behavior.
- Confirmed the focused host target compiles cleanly under `-Wall -Wextra -Werror` and links `libm`.
- Reviewed repository status and ensured only the four named task files are selected for the task commit.

## Concerns

- The API assumes non-null pointers and valid configuration ranges, exactly as specified; it does not defensively validate caller input.
- Verification is limited to the requested native host tests. The task does not integrate this module into or rebuild the ARM firmware.
- The broader repository contains unrelated pre-existing modified, deleted, and untracked files; they were left untouched.
