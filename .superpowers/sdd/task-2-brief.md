### Task 2: Add a Monotonic Timebase and Complete ICM Raw Samples

**Files:**
- Create: `inc/platform_time.h`
- Create: `src/platform_time.c`
- Create: `tests/test_platform_time.c`
- Modify: `modules/ICM42688/inc/icm42688_hal.h`
- Modify: `modules/ICM42688/src/icm42688_hal.c`
- Modify: `modules/ICM42688/src/icm42688_mspm0.c`
- Modify: `tests/run_tests.ps1`

**Interfaces:**
- Consumes: dedicated 1 MHz timer count after Task 7 generates `ICM42688_TIMER_INST`.
- Produces: `void PlatformTime_Init(void)`, `uint32_t PlatformTime_GetUs32(void)`, `uint64_t PlatformTime_GetUs64(void)`, and `icm42688_data_t.temperature_raw`.

- [ ] **Step 1: Write failing wrap and timer-direction tests**

Create `tests/test_platform_time.c` that injects fake down-counter samples and asserts:

```c
assert(PlatformTime_UpCountFromDownCount(UINT32_MAX) == 0U);
assert(PlatformTime_UpCountFromDownCount(UINT32_MAX - 25U) == 25U);
assert(PlatformTime_Extend32(0xFFFFFFF0U) == UINT64_C(0xFFFFFFF0));
assert(PlatformTime_Extend32(0x00000010U) == UINT64_C(0x100000010));
assert(PlatformTime_Extend32(0x00000020U) == UINT64_C(0x100000020));
```

Declare the tested pure helpers in `platform_time.h` so they are testable without DriverLib.

- [ ] **Step 2: Run the test and verify it fails**

Add a GCC invocation for `test_platform_time.c` and `src/platform_time.c` to `tests/run_tests.ps1`, then run:

```powershell
powershell -ExecutionPolicy Bypass -File ".\tests\run_tests.ps1"
```

Expected: compile failure because `platform_time.h` and its functions do not exist.

- [ ] **Step 3: Implement the pure monotonic extension and target wrapper**

Use these exact public signatures:

```c
void PlatformTime_Init(void);
uint32_t PlatformTime_UpCountFromDownCount(uint32_t down_count);
uint64_t PlatformTime_Extend32(uint32_t now32);
uint32_t PlatformTime_GetUs32(void);
uint64_t PlatformTime_GetUs64(void);
```

Implement down-count inversion as `UINT32_MAX - down_count`. Implement wrap extension by retaining the previous 32-bit sample and a 64-bit high word; when `now32 < previous32`, add `UINT64_C(1) << 32`. Protect the shared extension state with a short critical section in target code. `PlatformTime_Init()` starts the dedicated timer and resets extension state.

- [ ] **Step 4: Run the platform-time test**

Run `powershell -ExecutionPolicy Bypass -File ".\tests\run_tests.ps1"`.

Expected: platform-time test passes, including one 32-bit wrap.

- [ ] **Step 5: Write a failing ICM temperature test**

Extend the migrated ICM host test so a 14-byte burst beginning at temperature registers produces:

```c
assert(data.temperature_raw == expected_temperature_raw);
assert(data.acc_raw.x == expected_accel_x);
assert(data.gyro_raw.z == expected_gyro_z);
```

Expected before implementation: compile failure because `temperature_raw` is absent.

- [ ] **Step 6: Read temperature and expose raw samples without changing units**

Add this field to `icm42688_data_t`:

```c
int16_t temperature_raw;
```

Change `icm42688_read()` to burst-read temperature plus accelerometer and gyro bytes from `TEMP_DATA1`, decode all signed big-endian fields, and keep the existing `acc_g` and calibrated `gyro_dps` outputs for AHRS users. The Sens-Decision adapter will consume only `temperature_raw`, `acc_raw`, and `gyro_raw`.

- [ ] **Step 7: Bind AHRS to the platform clock**

Replace the adapter's direct `DL_TimerG_getTimerCount(ICM42688_TIMER_INST)` callback with:

```c
static uint32_t timer_get_time_us(void)
{
    return PlatformTime_GetUs32();
}
```

Do not let `icm42688_mspm0.c` configure or start TIMG0.

- [ ] **Step 8: Run ICM and platform-time tests**

Run the target-local test runner and the original ICM suite.

Expected: all tests pass; a raw temperature sample is available; the AHRS callback sees increasing microseconds.
