# Task 3 RED/GREEN Report: Software-Zero Balance Motor Adapter

## Scope

Implemented the HAL-independent `BalanceMotor` adapter using only the Task 3 source/test files:

- `App/Inc/balance_motor.h`
- `App/Src/balance_motor.c`
- `tests/test_balance_motor.c`
- `tests/CMakeLists.txt`

No temporary reference files were read or modified.

## RED 1: Missing Adapter

Added the fake-transport adapter tests and the `test_balance_motor` CMake target before adding production code.

Command:

```powershell
cmake -S tests -B build/host-tests
cmake --build build/host-tests --target test_balance_motor
```

Observed expected failure:

```text
CMake Error at CMakeLists.txt:30 (add_executable):
  Cannot find source file:
    ../App/Src/balance_motor.c

CMake Error at CMakeLists.txt:30 (add_executable):
  No SOURCES given to target: test_balance_motor
```

The failure was caused by the absent Task 3 implementation, not by a test typo.

## GREEN 1: Minimal Adapter

Added the adapter and transport boundary with:

- Software-zero acquisition through `emm_v5_encode_position_query()` and `emm_v5_parse_position()`.
- Absolute target conversion from zero plus rounded command offset.
- Full finite/range/overflow validation for position, speed, and acceleration.
- Signed target conversion to direction plus unsigned magnitude without signed `INT32_MIN` negation.
- One overwriteable pending target and one priority stop/disable slot.
- Transport acceptance semantics where accepted frames immediately cease to be pending.
- ACK completion parsing through `emm_v5_parse_ack()`.
- Consecutive transport/protocol failure lockout, pending-work clearing, and zero invalidation.
- Fault clearing that deliberately requires zero acquisition again.

Focused command:

```powershell
cmake -S tests -B build/host-tests
cmake --build build/host-tests --target test_balance_motor
ctest --test-dir build/host-tests -R balance_motor --output-on-failure
```

Observed: `balance_motor` passed.

## RED/GREEN 2: Float-to-int32 Boundary

Self-review identified that `(float)INT32_MAX` equals `2147483648.0f` on the host. A comparison against `(float)INT32_MAX` could therefore allow an out-of-range float-to-`int32_t` cast.

Added `test_float_value_above_int32_max_is_rejected_before_cast()` and observed:

```text
FAIL ... test_balance_motor.c:166:
balance_motor_submit(&motor, &command) == BALANCE_MOTOR_OVERFLOW
```

Changed scaled-position bounds checks to compare in `double`, preserving the exact `int32_t` limits. The focused test then passed.

## RED/GREEN 3: Priority Interleaving

Self-review identified an ordering case where a stop was pending while busy, transport became available, and a later motion submission could be sent before the stop.

Added `test_pending_stop_precedes_later_motion()` and observed four expected assertion failures showing that motion was sent prematurely and the stop frame was not first.

Changed `balance_motor_submit()` to retain the normal target without sending whenever priority work is pending. The focused test then passed and process order became stop/disable first, normal motion second.

## Coverage

The adapter tests cover:

- Initialization without traffic or zero validity.
- Motion rejection before zero acquisition.
- Exact zero-query frame and response metadata.
- Invalid versus valid zero responses.
- Complete positive and negative-offset absolute `0xFD` frames.
- `NAN`, `INFINITY`, negative speed/acceleration, conversion overflow, rounded range overflow, and zero-plus-offset overflow.
- Latest-target retention while transport is busy.
- Stop/disable replacement of pending motion and priority ordering.
- Transport acceptance removing pending work.
- Mixed transport/protocol failure lockout after three consecutive failures.
- Zero invalidation on lockout and mandatory re-zero after fault clearing.

## Final Verification

Commands:

```powershell
cmake --build build/host-tests
ctest --test-dir build/host-tests --output-on-failure
git diff --check -- App/Inc/balance_motor.h App/Src/balance_motor.c tests/test_balance_motor.c tests/CMakeLists.txt
```

Results:

```text
[ 56%] Built target test_balance
[ 75%] Built target test_emm_v5_protocol
[100%] Built target test_balance_motor

1/3 balance_core       Passed
2/3 emm_v5_protocol    Passed
3/3 balance_motor      Passed
100% tests passed, 0 tests failed out of 3
```

`git diff --check` reported no whitespace errors. Git emitted only the repository's existing LF-to-CRLF working-copy warning for `tests/CMakeLists.txt`.

## Self-Review

- Full generated frames are asserted instead of relying on the brief's illustrative byte offsets.
- Accepted transport commands are not retained as pending; later completion/error arrives through callbacks.
- Busy normal commands overwrite the prior normal slot.
- Stop and disable overwrite the single priority slot, clear normal work, and cannot be bypassed by later motion.
- Lockout clears both pending slots and invalidates zero.
- No HAL dependency or unrelated source change was introduced.

No blocking concern remains. `BALANCE_MOTOR_PROTOCOL_ERROR` is part of the required public result enum but is not returned by the `void` response callback; parser failures are instead recorded in the failure counter as required.
