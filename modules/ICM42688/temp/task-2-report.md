# Task 2 Report — Portable Six-Axis AHRS

## Status: DONE

## Files produced
- `inc/ahrs_hal.h` — public API (Euler angle, timer interface, prototypes)
- `src/ahrs_hal.c` — hardened Mahony implementation
- `temp/tests/test_ahrs.c` — host test harness

## Verification
```
gcc -std=c99 -Wall -Wextra -Werror -Iinc temp/tests/test_ahrs.c src/ahrs_hal.c -lm -o temp/tests/test_ahrs.exe
& .\temp\tests\test_ahrs.exe
```
Result: `test_ahrs: PASS (16 assertions)` — clean compile, exit code 0.

## Tests (9 functions, 16 assertions, all passing)
1. Level sensor stays level (roll/pitch < 0.05°)
2. Unsigned timestamp wrap is one tick: `ahrs_elapsed_us(UINT32_MAX, 0) == 1`
3. First frame only establishes time baseline (attitude unchanged after first call)
4. Zero accel produces no NaN (pure gyro integration works)
5. dt == 0 is dropped
6. dt > 100 ms is dropped
7. Positive X gyro makes roll increase
8. NULL output pointer is safe
9. `ahrs_reset_parameters` restores identity (verified via subsequent level frame)

## Original parameters preserved verbatim
- KP_ROLL 3.2, KP_PITCH 2.95, KP_YAW 1.0
- KI_ROLL 0.095, KI_PITCH 0.085, KI_YAW 0.065
- GYRO_DEADBAND 0.01, ERROR_DEADBAND 0.0050
- INT_LIMIT 0.1, INT_LIMIT_FACTOR 0.5
- US_TO_S 1000000.0, HALF_PERIOD_SCALE 2.0

## Hardening fixes applied (all six from the brief)
1. `inv_sqrt` uses C99 `memcpy` + `uint32_t` (no `long*` type-punning). ahrs_hal.c:60
2. `ahrs_elapsed_us` uses unsigned subtraction `current - previous` for auto-wrap. ahrs_hal.c:104
3. Timestamp (`lastTime`) is updated BEFORE the elapsed validation; only `1..100000` us accepted. ahrs_hal.c:158-176
4. Gravity feedback skipped when `accel_mag_sq == 0`, but quaternion integration still runs with valid dt. ahrs_hal.c:178-217
5. Quaternion `sum_sq > 0` checked before normalizing. ahrs_hal.c:226-233
6. `asinf` input clamped to `[-1, 1]`. ahrs_hal.c:243-249

## Items dropped per brief
- Magnetometer parameters (`mx, my, mz`) and the mag normalization block
- `ahrs_system_t` type and `p_system` pointer
- `ahrs_vector_float_t north/west` reference vectors and their computation
- `ahrs_init_attitude_offset()` (only delayed without updating AHRS)
- All STM32 headers (`main.h`, `myInterface.h`, `icm42688_hal.h`)

## Interface delivered (matches brief exactly)
- `ahrs_hal_init(const ahrs_timer_t *)` — bind timer (const-qualified per brief)
- `ahrs_init(void)` — identity quaternion, zero integral, baseline timestamp
- `ahrs_update_6axis(gx, gy, gz, ax, ay, az)` — gyro rad/s, accel g
- `ahrs_get_attitude(ahrs_euler_angle_t *)` — yaw/pitch/roll degrees, NULL-safe
- `ahrs_reset_parameters(void)` — quaternion=identity, integrals zeroed
- `ahrs_elapsed_us(previous, current)` — unsigned wrap-safe diff

## Platform independence
`ahrs_hal.c` includes only `ahrs_hal.h` and `<string.h>`. No STM32/MSPM0 headers. The platform is injected solely through the `ahrs_timer_t` function-pointer interface.

## Concerns
None. Build is warning-clean under `-Wall -Wextra -Werror`; all assertions pass.
