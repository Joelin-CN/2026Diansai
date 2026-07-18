# ICM42688 Hardware Integration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Connect the controller-local ICM42688 driver to the MSPM0 SPI hardware and make its +/-8 g, +/-1000 dps, 1 kHz range, conversion scales, calibration bias, and timestamps consistent with the unchanged Sens-Decision algorithms.

**Architecture:** The production `icm42688_config_t` is the only source of IMU range and ODR. The ICM HAL explicitly encodes FS_SEL fields and exposes read-only scale/bias metadata; `ControlApp_Init()` binds the MSPM0 adapter, initializes/calibrates the sensor, and copies converted metadata into the existing Sens-Decision configuration before sensor initialization. TIMG12 supplies the monotonic microsecond clock, and the 50 Hz pipeline short-circuits on any failed stage without issuing a new velocity command.

**Tech Stack:** C99, TI MSPM0G3507 DriverLib/SysConfig 1.26.2+4477, SDK 2.10.00.04, FreeRTOS, GCC host tests, EIDE/AC6 target build, PowerShell 5.1.

## Global Constraints

- Do not change Sens-Decision interfaces, filtering, EKF, perception, behavior, trajectory algorithms, or control gains.
- The production IMU mode is exactly +/-8 g, +/-1000 dps, and 1 kHz.
- For that mode, `ACCEL_CONFIG0` and `GYRO_CONFIG0` must both be `0x26`; FS_SEL `1` occupies bits 7:5 and ODR `0x06` occupies bits 3:0.
- Keep the raw signed 16-bit `SensorAdapter -> Sens-Decision` data boundary.
- Do not add UART/USB/BLE communication, AHRS usage, SPI DMA, a second ICM42688, or pin remapping.
- Keep SPI1 Mode 0 at 1 MHz, software CS on PB20, and TIMG12 at 1 MHz from MFCLK/4.
- `NewProject1.syscfg` remains the only hardware-resource authority; do not manually edit generated `Debug/ti_msp_dl_config.c` or `.h`.
- Every initialization failure and every repeated runtime pipeline failure must leave the motors in a safe stopped state.
- Do not claim hardware success without physical WHO_AM_I, register readback, static sensor, timestamp, and axis-orientation evidence.
- Do not modify unrelated existing worktree changes. Stage only files named in the current task.
- The active firmware uses `2025e/m0_controller/modules/ICM42688`; do not silently change the separate reusable `modules/ICM42688` source implementation in this plan.

## File Structure

- `modules/ICM42688/inc/icm42688_hal.h`: public active-scale and gyro-bias metadata API.
- `modules/ICM42688/inc/icm42688_mspm0.h`: target-independent declaration of the MSPM0 binding API.
- `modules/ICM42688/src/icm42688_hal.c`: enum-to-register encoding, configuration validation, scale metadata, and calibrated bias metadata.
- `modules/ICM42688/src/icm42688_mspm0.c`: owns the generated DriverLib include used by the target adapter implementation.
- `tests/test_icm42688.c`: exact register, scale, calibration, and invalid-configuration tests.
- `inc/platform_time.h`: final TIMG12 timebase contract.
- `src/platform_time.c`: TIMG12 start/read and interrupt-safe 32-to-64-bit extension.
- `tests/fakes/ti_msp_dl_config.h`: host DriverLib/CMSIS declarations for platform-time tests, preserving existing I2C fakes.
- `tests/test_platform_time.c`: target-wrapper tests using fake TIMG12 and interrupt state.
- `src/control_app.c`: production IMU configuration, MSPM0 binding, Sens-Decision synchronization, MCP status fix, and sequential failure handling.
- `inc/control_app.h`: accurate startup and failure-flow documentation.
- `tests/test_control_app.c`: startup ordering/configuration and 50 Hz short-circuit tests.
- `tests/run_tests.ps1`: existing suite runner; only change if a new test source or compile include is required.
- `README.md`: resolved software limitations and remaining hardware-validation boundary.
- `WIRING_AND_SYSCONFIG.md`: selected ranges, exact register values, TIMG12 ownership, and acceptance checklist.
- `../../modules/ICM42688/README.md`: adapter-neutral timer contract, with this controller identified as a TIMG12 integration.
- `docs/superpowers/specs/2026-07-18-icm42688-hardware-integration-design.md`: approved design; keep register values consistent at `0x26`.

---

### Task 1: Make ICM Range Encoding and Metadata Authoritative

**Files:**
- Modify: `modules/ICM42688/inc/icm42688_hal.h:58-197`
- Modify: `modules/ICM42688/src/icm42688_hal.c:39-174,219-281`
- Modify: `tests/test_icm42688.c:14-156`

**Interfaces:**
- Consumes: existing `icm42688_config_t`, `icm42688_vector3f_t`, and injected `icm42688_comm_t`/`icm42688_system_t`.
- Produces: `icm42688_status_t icm42688_get_scale_factors(float *accel_g_per_lsb, float *gyro_dps_per_lsb)`.
- Produces: `icm42688_status_t icm42688_get_gyro_bias(icm42688_vector3f_t *bias_dps)`.
- Produces: exact range encoding used later by `ControlApp_Init()`.

- [ ] **Step 1: Add failing tests for exact production register values**

Extend the mock register test with a reusable initializer and assert the selected production mode writes `0x26`:

```c
static const icm42688_config_t production_config = {
    .interface_type = ICM42688_INTERFACE_SPI,
    .acc_sample = ICM42688_ACC_SAMPLE_SGN_8G,
    .gyro_sample = ICM42688_GYRO_SAMPLE_SGN_1000DPS,
    .sample_rate = ICM42688_SAMPLE_RATE_1000,
};

static void reset_mock(void)
{
    memset(mock_registers, 0, sizeof(mock_registers));
    mock_who_am_i_value = ICM42688_ID;
}

static void test_production_range_registers(void)
{
    reset_mock();
    icm42688_hal_init(&mock_comm, &mock_system, &production_config);

    assert(icm42688_init() == ICM42688_STATUS_OK);
    assert(mock_registers[ICM42688_ACCEL_CONFIG0] == 0x26U);
    assert(mock_registers[ICM42688_GYRO_CONFIG0] == 0x26U);
}
```

Call `test_production_range_registers()` from `main()` before the existing burst-read test.

- [ ] **Step 2: Run the ICM host test and verify the current ordinal shift fails**

Run:

```powershell
gcc -std=c99 -Wall -Wextra -I"modules/ICM42688/inc" tests/test_icm42688.c modules/ICM42688/src/icm42688_hal.c -o tests/build/test_icm42688.exe
& ".\tests\build\test_icm42688.exe"
```

Expected: FAIL at the `ACCEL_CONFIG0 == 0x26U` or `GYRO_CONFIG0 == 0x26U` assertion because the current code shifts enum ordinals directly.

- [ ] **Step 3: Add failing table tests for every FS_SEL mapping and configuration validation**

Record writes in `mock_registers`, then table-test these mappings with `ICM42688_SAMPLE_RATE_1000`:

```c
static const uint8_t accel_fs_sel[] = {3U, 2U, 1U, 0U};
static const uint8_t gyro_fs_sel[] = {7U, 6U, 5U, 4U, 3U, 2U, 1U, 0U};

for (unsigned i = 0U; i < sizeof(accel_fs_sel); ++i) {
    icm42688_config_t config = production_config;
    config.acc_sample = (icm42688_acc_sample_t)i;
    reset_mock();
    icm42688_hal_init(&mock_comm, &mock_system, &config);
    assert(icm42688_init() == ICM42688_STATUS_OK);
    assert(mock_registers[ICM42688_ACCEL_CONFIG0] ==
           (uint8_t)((accel_fs_sel[i] << 5) | 0x06U));
}

for (unsigned i = 0U; i < sizeof(gyro_fs_sel); ++i) {
    icm42688_config_t config = production_config;
    config.gyro_sample = (icm42688_gyro_sample_t)i;
    reset_mock();
    icm42688_hal_init(&mock_comm, &mock_system, &config);
    assert(icm42688_init() == ICM42688_STATUS_OK);
    assert(mock_registers[ICM42688_GYRO_CONFIG0] ==
           (uint8_t)((gyro_fs_sel[i] << 5) | 0x06U));
}
```

Also bind a config with `(icm42688_acc_sample_t)99` and assert `icm42688_init() == ICM42688_STATUS_INVALID_ARGUMENT` before any reset/wake write.

- [ ] **Step 4: Implement explicit enum-to-register mapping and reject invalid configurations**

Replace ordinal shifting/default fallbacks with status-returning mapping helpers:

```c
static bool acc_fs_sel(icm42688_acc_sample_t range, uint8_t *value)
{
    switch (range) {
        case ICM42688_ACC_SAMPLE_SGN_2G:  *value = 3U; return true;
        case ICM42688_ACC_SAMPLE_SGN_4G:  *value = 2U; return true;
        case ICM42688_ACC_SAMPLE_SGN_8G:  *value = 1U; return true;
        case ICM42688_ACC_SAMPLE_SGN_16G: *value = 0U; return true;
        default: return false;
    }
}

static bool gyro_fs_sel(icm42688_gyro_sample_t range, uint8_t *value)
{
    switch (range) {
        case ICM42688_GYRO_SAMPLE_SGN_15_125DPS: *value = 7U; return true;
        case ICM42688_GYRO_SAMPLE_SGN_31_25DPS:  *value = 6U; return true;
        case ICM42688_GYRO_SAMPLE_SGN_62_5DPS:   *value = 5U; return true;
        case ICM42688_GYRO_SAMPLE_SGN_125DPS:    *value = 4U; return true;
        case ICM42688_GYRO_SAMPLE_SGN_250DPS:    *value = 3U; return true;
        case ICM42688_GYRO_SAMPLE_SGN_500DPS:    *value = 2U; return true;
        case ICM42688_GYRO_SAMPLE_SGN_1000DPS:   *value = 1U; return true;
        case ICM42688_GYRO_SAMPLE_SGN_2000DPS:   *value = 0U; return true;
        default: return false;
    }
}
```

Make `sample_rate_odr()` return `bool` through an output pointer rather than silently defaulting. At the top of `icm42688_init()`, validate `interface_type == ICM42688_INTERFACE_SPI`, all three mappings, and only then touch hardware:

```c
uint8_t accel_fs;
uint8_t gyro_fs;
uint8_t odr;

if (!comm_is_bound() || !system_is_bound() || p_config == NULL) {
    return ICM42688_STATUS_NOT_READY;
}
if (p_config->interface_type != ICM42688_INTERFACE_SPI ||
    !acc_fs_sel(p_config->acc_sample, &accel_fs) ||
    !gyro_fs_sel(p_config->gyro_sample, &gyro_fs) ||
    !sample_rate_odr(p_config->sample_rate, &odr)) {
    return ICM42688_STATUS_INVALID_ARGUMENT;
}
```

Write `(accel_fs << 5) | odr` and `(gyro_fs << 5) | odr` to the two CONFIG0 registers.

- [ ] **Step 5: Add failing tests for scale and calibrated-bias metadata**

Add these declarations to the test only after adding matching prototypes to the header in the next implementation step. Test unbound access first, then production scales:

```c
static void test_scale_and_bias_metadata(void)
{
    float accel_scale = 0.0f;
    float gyro_scale = 0.0f;
    icm42688_vector3f_t bias;

    icm42688_hal_init(NULL, NULL, NULL);
    assert(icm42688_get_scale_factors(&accel_scale, &gyro_scale) ==
           ICM42688_STATUS_NOT_READY);
    assert(icm42688_get_gyro_bias(&bias) == ICM42688_STATUS_NOT_READY);

    reset_mock();
    icm42688_hal_init(&mock_comm, &mock_system, &production_config);
    assert(icm42688_init() == ICM42688_STATUS_OK);
    assert(icm42688_get_scale_factors(&accel_scale, &gyro_scale) ==
           ICM42688_STATUS_OK);
    assert(fabsf(accel_scale - (1.0f / 4096.0f)) < 1e-9f);
    assert(fabsf(gyro_scale - (1.0f / 32.768f)) < 1e-6f);

    setup_14byte_burst(0, 0, 0, 0, 328, -656, 984);
    assert(icm42688_calibrate_gyro(2U, 0U) == ICM42688_STATUS_OK);
    assert(icm42688_get_gyro_bias(&bias) == ICM42688_STATUS_OK);
    assert(fabsf(bias.x - 10.009765625f) < 1e-5f);
    assert(fabsf(bias.y + 20.01953125f) < 1e-5f);
    assert(fabsf(bias.z - 30.029296875f) < 1e-5f);

    assert(icm42688_get_scale_factors(NULL, &gyro_scale) ==
           ICM42688_STATUS_INVALID_ARGUMENT);
    assert(icm42688_get_gyro_bias(NULL) == ICM42688_STATUS_INVALID_ARGUMENT);
}
```

Include `<math.h>` and call the test from `main()`.

- [ ] **Step 6: Add the read-only metadata API**

Add to `icm42688_hal.h`:

```c
icm42688_status_t icm42688_get_scale_factors(float *accel_g_per_lsb,
                                              float *gyro_dps_per_lsb);
icm42688_status_t icm42688_get_gyro_bias(icm42688_vector3f_t *bias_dps);
```

Track successful initialization explicitly:

```c
static bool initialized = false;
```

Reset it and all factors/biases in `icm42688_hal_init()`. Set it true only at the end of a successful `icm42688_init()`. Implement:

```c
icm42688_status_t icm42688_get_scale_factors(float *accel_g_per_lsb,
                                              float *gyro_dps_per_lsb)
{
    if (accel_g_per_lsb == NULL || gyro_dps_per_lsb == NULL) {
        return ICM42688_STATUS_INVALID_ARGUMENT;
    }
    if (!initialized) {
        return ICM42688_STATUS_NOT_READY;
    }
    *accel_g_per_lsb = acc_factor;
    *gyro_dps_per_lsb = gyro_factor;
    return ICM42688_STATUS_OK;
}

icm42688_status_t icm42688_get_gyro_bias(icm42688_vector3f_t *bias_dps)
{
    if (bias_dps == NULL) {
        return ICM42688_STATUS_INVALID_ARGUMENT;
    }
    if (!initialized) {
        return ICM42688_STATUS_NOT_READY;
    }
    bias_dps->x = gyro_bias_x;
    bias_dps->y = gyro_bias_y;
    bias_dps->z = gyro_bias_z;
    return ICM42688_STATUS_OK;
}
```

- [ ] **Step 7: Run the focused and complete controller host suites**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File ".\tests\run_tests.ps1"
```

Expected: `test_icm42688` passes the complete FS_SEL table, exact `0x26` writes, metadata, calibration, and existing 14-byte burst assertions; final line is `Host tests: PASS`.

- [ ] **Step 8: Commit Task 1 if commits were explicitly requested for the execution session**

```powershell
git add -- "modules/ICM42688/inc/icm42688_hal.h" "modules/ICM42688/src/icm42688_hal.c" "tests/test_icm42688.c"
git commit -m "fix: unify ICM42688 range metadata"
```

---

### Task 2: Replace the Zero Time Stub with TIMG12

**Files:**
- Modify: `inc/platform_time.h:1-67`
- Modify: `src/platform_time.c:1-76`
- Modify: `tests/fakes/ti_msp_dl_config.h:1-43`
- Modify: `tests/test_platform_time.c:1-44`
- Modify if required: `tests/run_tests.ps1:23-31`

**Interfaces:**
- Consumes: generated `ICM42688_TIMER_INST`, `DL_TimerG_startCounter()`, `DL_TimerG_getTimerCount()`, and CMSIS PRIMASK intrinsics.
- Produces: a running `uint32_t PlatformTime_GetUs32(void)` and interrupt-safe `uint64_t PlatformTime_GetUs64(void)` for ICM and Sens-Decision.

- [ ] **Step 1: Extend the fake DriverLib header without disturbing MCP23017 tests**

Append timer and interrupt declarations to `tests/fakes/ti_msp_dl_config.h`; retain every existing I2C definition:

```c
#define ICM42688_TIMER_INST ((void *)1)

void DL_TimerG_startCounter(void *timer);
uint32_t DL_TimerG_getTimerCount(void *timer);
uint32_t __get_PRIMASK(void);
void __disable_irq(void);
void __enable_irq(void);
```

- [ ] **Step 2: Add failing tests for timer start, reads, wrap, and interrupt restoration**

Define fake state and functions in `tests/test_platform_time.c`:

```c
static uint32_t fake_down_count = UINT32_MAX;
static uint32_t fake_primask = 0U;
static unsigned timer_start_calls = 0U;
static unsigned disable_irq_calls = 0U;
static unsigned enable_irq_calls = 0U;

void DL_TimerG_startCounter(void *timer)
{
    assert(timer == ICM42688_TIMER_INST);
    ++timer_start_calls;
}

uint32_t DL_TimerG_getTimerCount(void *timer)
{
    assert(timer == ICM42688_TIMER_INST);
    return fake_down_count;
}

uint32_t __get_PRIMASK(void) { return fake_primask; }
void __disable_irq(void) { fake_primask = 1U; ++disable_irq_calls; }
void __enable_irq(void) { fake_primask = 0U; ++enable_irq_calls; }
```

Add assertions:

```c
PlatformTime_Init();
assert(timer_start_calls == 1U);

fake_down_count = UINT32_MAX - 25U;
assert(PlatformTime_GetUs32() == 25U);

fake_down_count = UINT32_MAX - 20000U;
assert(PlatformTime_GetUs64() == UINT64_C(20000));
assert(disable_irq_calls == 1U);
assert(enable_irq_calls == 1U);

fake_primask = 1U;
fake_down_count = UINT32_MAX - 40000U;
assert(PlatformTime_GetUs64() == UINT64_C(40000));
assert(fake_primask == 1U);
assert(enable_irq_calls == 1U);
```

Keep the existing pure conversion and wrap tests, but call `PlatformTime_Init()` before each independent wrap scenario so static extension state is reset.

- [ ] **Step 3: Update the platform-time test build to use the fake generated header**

Add `"-I$PSScriptRoot\fakes"` to the `test_platform_time` arguments in `tests/run_tests.ps1`. Run:

```powershell
powershell -ExecutionPolicy Bypass -File ".\tests\run_tests.ps1"
```

Expected: compilation or assertions fail because `platform_time.c` does not yet include DriverLib, start TIMG12, or read the timer.

- [ ] **Step 4: Implement the target timer path and preserve interrupt state**

In `src/platform_time.c`, include the generated contract and replace the stub:

```c
#include "platform_time.h"
#include "ti_msp_dl_config.h"

void PlatformTime_Init(void)
{
    previous32 = 0U;
    high_word = 0U;
    DL_TimerG_startCounter(ICM42688_TIMER_INST);
}

uint32_t PlatformTime_GetUs32(void)
{
    return PlatformTime_UpCountFromDownCount(
        DL_TimerG_getTimerCount(ICM42688_TIMER_INST));
}

uint64_t PlatformTime_GetUs64(void)
{
    uint32_t primask = __get_PRIMASK();
    uint64_t result;

    __disable_irq();
    result = PlatformTime_Extend32(PlatformTime_GetUs32());
    if (primask == 0U) {
        __enable_irq();
    }
    return result;
}
```

Update `inc/platform_time.h` comments to state that SysConfig configures TIMG12 and `PlatformTime_Init()` starts it. Remove every Task 7/stub statement.

- [ ] **Step 5: Run focused and complete host tests**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File ".\tests\run_tests.ps1"
```

Expected: platform-time timer/start/read/PRIMASK assertions pass and final line is `Host tests: PASS`.

- [ ] **Step 6: Commit Task 2 if commits were explicitly requested for the execution session**

```powershell
git add -- "inc/platform_time.h" "src/platform_time.c" "tests/fakes/ti_msp_dl_config.h" "tests/test_platform_time.c" "tests/run_tests.ps1"
git commit -m "fix: connect platform time to TIMG12"
```

---

### Task 3: Bind ICM42688 and Synchronize Sens-Decision Configuration

**Files:**
- Modify: `modules/ICM42688/inc/icm42688_mspm0.h:7-23`
- Modify: `modules/ICM42688/src/icm42688_mspm0.c:7-9`
- Modify: `src/control_app.c:7-143`
- Modify: `inc/control_app.h:23-55`
- Modify: `tests/test_control_app.c:23-386`

**Interfaces:**
- Consumes: `void icm42688_mspm0_bind(const icm42688_config_t *config)`.
- Consumes: `icm42688_get_scale_factors()` and `icm42688_get_gyro_bias()` from Task 1.
- Consumes: `void sd_config_reset_defaults(void)` and mutable `g_sens_decision_config.imu`.
- Produces: a production startup that reaches actual SPI initialization and gives Sens-Decision matching SI-unit scales/biases.

- [ ] **Step 1: Correct the MCP fake to expose the existing status-enum bug**

Move `#include "ti_msp_dl_config.h"` from `icm42688_mspm0.h` to `icm42688_mspm0.c`. The public adapter header only needs `icm42688_hal.h` and `ahrs_hal.h`; this lets host tests include the real bind declaration without importing target DriverLib.

Include the real `icm42688_hal.h`, `icm42688_mspm0.h`, `mcp23017.h`, and Sens-Decision `config.h` in `tests/test_control_app.c`. Remove the test-local declarations of `mcp23017_status_t`, `icm42688_status_t`, `icm42688_data_t`, `sd_status_t`, `sens_decision_config_t`, `sd_ekf_config_t`, and `sd_trajectory_config_t`; use the public definitions instead. Retain the test-local opaque stand-ins for unrelated modules. Replace Boolean MCP fakes with exact signatures:

```c
mcp23017_status_t MCP23017_Init(void)
{
    return inject_mcp_failure ? MCP23017_STATUS_IO_ERROR : MCP23017_STATUS_OK;
}

mcp23017_status_t MCP23017_ReadInputs(uint16_t *inputs)
{
    if (inputs == NULL) {
        return MCP23017_STATUS_INVALID_ARGUMENT;
    }
    *inputs = 0x0FFFU;
    return inject_mcp_failure ? MCP23017_STATUS_IO_ERROR : MCP23017_STATUS_OK;
}
```

Add `test_successful_init_accepts_zero_status()`:

```c
reset_call_tracking();
assert(ControlApp_Init(3U));
```

Run the suite and expect this assertion to fail at the current `if (!MCP23017_Init())` check.

- [ ] **Step 2: Add failing startup-order and production-config tests**

Add an event enum and recorder:

```c
typedef enum {
    EVENT_CONFIG_DEFAULTS,
    EVENT_TIME_INIT,
    EVENT_ICM_BIND,
    EVENT_ICM_INIT,
    EVENT_ICM_CALIBRATE,
    EVENT_SCALE_READ,
    EVENT_BIAS_READ,
    EVENT_SENSOR_INIT
} init_event_t;

static init_event_t init_events[16];
static size_t init_event_count;
static icm42688_config_t bound_icm_config;
```

Implement fakes that append their event and capture the bound config:

```c
void icm42688_mspm0_bind(const icm42688_config_t *config)
{
    init_events[init_event_count++] = EVENT_ICM_BIND;
    assert(config != NULL);
    bound_icm_config = *config;
}

void sd_config_reset_defaults(void)
{
    init_events[init_event_count++] = EVENT_CONFIG_DEFAULTS;
    memset(&g_sens_decision_config, 0, sizeof(g_sens_decision_config));
    g_sens_decision_config.imu.filter_alpha = 0.25f;
}
```

Return deterministic metadata:

```c
icm42688_status_t icm42688_get_scale_factors(float *accel, float *gyro)
{
    init_events[init_event_count++] = EVENT_SCALE_READ;
    *accel = 1.0f / 4096.0f;
    *gyro = 1.0f / 32.768f;
    return ICM42688_STATUS_OK;
}

icm42688_status_t icm42688_get_gyro_bias(icm42688_vector3f_t *bias)
{
    init_events[init_event_count++] = EVENT_BIAS_READ;
    bias->x = 1.0f;
    bias->y = -2.0f;
    bias->z = 3.0f;
    return ICM42688_STATUS_OK;
}
```

Assert exact startup order and configuration:

```c
assert(bound_icm_config.interface_type == ICM42688_INTERFACE_SPI);
assert(bound_icm_config.acc_sample == ICM42688_ACC_SAMPLE_SGN_8G);
assert(bound_icm_config.gyro_sample == ICM42688_GYRO_SAMPLE_SGN_1000DPS);
assert(bound_icm_config.sample_rate == ICM42688_SAMPLE_RATE_1000);

assert_event_before(EVENT_CONFIG_DEFAULTS, EVENT_SENSOR_INIT);
assert_event_before(EVENT_TIME_INIT, EVENT_ICM_BIND);
assert_event_before(EVENT_ICM_BIND, EVENT_ICM_INIT);
assert_event_before(EVENT_ICM_INIT, EVENT_ICM_CALIBRATE);
assert_event_before(EVENT_ICM_CALIBRATE, EVENT_SCALE_READ);
assert_event_before(EVENT_BIAS_READ, EVENT_SENSOR_INIT);
```

Assert unit synchronization without changing `filter_alpha`:

```c
const float degrees_to_radians = 0.017453292519943295f;
assert(fabsf(g_sens_decision_config.imu.accel_scale_mps2_per_lsb -
             9.80665f / 4096.0f) < 1e-8f);
assert(fabsf(g_sens_decision_config.imu.gyro_scale_radps_per_lsb -
             degrees_to_radians / 32.768f) < 1e-8f);
assert(fabsf(g_sens_decision_config.imu.gyro_bias_radps[0] -
             degrees_to_radians) < 1e-7f);
assert(fabsf(g_sens_decision_config.imu.gyro_bias_radps[1] +
             2.0f * degrees_to_radians) < 1e-7f);
assert(fabsf(g_sens_decision_config.imu.gyro_bias_radps[2] -
             3.0f * degrees_to_radians) < 1e-7f);
assert(g_sens_decision_config.imu.filter_alpha == 0.25f);
```

- [ ] **Step 3: Add failing metadata-error tests**

Add separate injection flags for scale and bias metadata failures. Assert each causes `ControlApp_Init()` to return false and call `Motor_Stop()`. This proves startup cannot validate one scale while leaving stale defaults for the other.

- [ ] **Step 4: Implement production startup configuration and synchronization**

Include the target adapter header:

```c
#include "../modules/ICM42688/inc/icm42688_mspm0.h"
```

Add file-scope constants:

```c
static const icm42688_config_t g_icm_config = {
    .interface_type = ICM42688_INTERFACE_SPI,
    .acc_sample = ICM42688_ACC_SAMPLE_SGN_8G,
    .gyro_sample = ICM42688_GYRO_SAMPLE_SGN_1000DPS,
    .sample_rate = ICM42688_SAMPLE_RATE_1000,
};

static const float STANDARD_GRAVITY_MPS2 = 9.80665f;
static const float DEGREES_TO_RADIANS = 0.017453292519943295f;
```

Change MCP checks to explicit comparisons:

```c
if (MCP23017_Init() != MCP23017_STATUS_OK) { /* stop and return false */ }
if (MCP23017_ReadInputs(&ir_mask) != MCP23017_STATUS_OK) { /* stop */ }
```

Before `PlatformTime_Init()`, call `sd_config_reset_defaults()`. Then bind before initialization:

```c
PlatformTime_Init();
icm42688_mspm0_bind(&g_icm_config);
if (icm42688_init() != ICM42688_STATUS_OK) { /* stop */ }
if (icm42688_calibrate_gyro(100U, 10U) != ICM42688_STATUS_OK) { /* stop */ }
```

Synchronize metadata before `sensors_configure_hal()`/`sensors_init_all()`:

```c
float accel_g_per_lsb;
float gyro_dps_per_lsb;
icm42688_vector3f_t gyro_bias_dps;

if (icm42688_get_scale_factors(&accel_g_per_lsb, &gyro_dps_per_lsb) !=
        ICM42688_STATUS_OK ||
    icm42688_get_gyro_bias(&gyro_bias_dps) != ICM42688_STATUS_OK) {
    Motor_Stop();
    return false;
}

g_sens_decision_config.imu.accel_scale_mps2_per_lsb =
    accel_g_per_lsb * STANDARD_GRAVITY_MPS2;
g_sens_decision_config.imu.gyro_scale_radps_per_lsb =
    gyro_dps_per_lsb * DEGREES_TO_RADIANS;
g_sens_decision_config.imu.gyro_bias_radps[0] =
    gyro_bias_dps.x * DEGREES_TO_RADIANS;
g_sens_decision_config.imu.gyro_bias_radps[1] =
    gyro_bias_dps.y * DEGREES_TO_RADIANS;
g_sens_decision_config.imu.gyro_bias_radps[2] =
    gyro_bias_dps.z * DEGREES_TO_RADIANS;
```

Do not modify `modules/Sens-Decision/src/interface.c`; its existing raw-to-SI conversion and bias subtraction remain authoritative.

- [ ] **Step 5: Update the public startup contract**

Update `inc/control_app.h` initialization order to include default config reset, TIMG12 start, MSPM0 bind, ICM initialization/calibration, scale/bias synchronization, and sensor initialization. State that the robot must remain stationary during the approximately one-second gyro calibration.

- [ ] **Step 6: Run the complete host suite**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File ".\tests\run_tests.ps1"
```

Expected: successful initialization accepts `MCP23017_STATUS_OK == 0`; all ordering, exact mode, SI scale, SI bias, and failure-stop assertions pass; final line is `Host tests: PASS`.

- [ ] **Step 7: Commit Task 3 if commits were explicitly requested for the execution session**

```powershell
git add -- "modules/ICM42688/inc/icm42688_mspm0.h" "modules/ICM42688/src/icm42688_mspm0.c" "src/control_app.c" "inc/control_app.h" "tests/test_control_app.c"
git commit -m "fix: bind ICM42688 during startup"
```

---

### Task 4: Short-Circuit Failed 50 Hz Pipeline Stages

**Files:**
- Modify: `src/control_app.c:145-216`
- Modify: `inc/control_app.h:43-55`
- Modify: `tests/test_control_app.c:23-386`

**Interfaces:**
- Consumes: existing `sd_status_t` returns from all five Sens-Decision stages.
- Produces: no new `MotionControl_SetVelocityCommand()` call unless the entire 50 Hz pipeline succeeds.
- Produces: existing three-consecutive-failure emergency stop behavior based on complete-pipeline success.

- [ ] **Step 1: Add a per-stage fault injector and call counters**

Add:

```c
typedef enum {
    PIPELINE_FAIL_NONE,
    PIPELINE_FAIL_PREPROCESS,
    PIPELINE_FAIL_STATE,
    PIPELINE_FAIL_PERCEPTION,
    PIPELINE_FAIL_BEHAVIOR,
    PIPELINE_FAIL_TRAJECTORY
} pipeline_failure_t;

static pipeline_failure_t pipeline_failure;
static unsigned state_calls;
static unsigned perception_calls;
static unsigned behavior_calls;
static unsigned trajectory_calls;
static unsigned velocity_command_calls;
```

Make each stage fake increment its counter and return `SD_ERR_DATA_INVALID` when selected. Make `MotionControl_SetVelocityCommand()` increment `velocity_command_calls`.

- [ ] **Step 2: Add failing table tests for short-circuit behavior**

For each failure stage, reset tracking, select the stage, run one decision cycle, and assert exact downstream counts. The expected table is:

```text
failure       state perception behavior trajectory velocity
preprocess      0       0         0         0         0
state           1       0         0         0         0
perception      1       1         0         0         0
behavior        1       1         1         0         0
trajectory      1       1         1         1         0
none            1       1         1         1         1
```

Use a helper that advances exactly to a 50 Hz cycle after resetting application state through successful `ControlApp_Init(3U)`.

- [ ] **Step 3: Add a failing repeated-stage-failure emergency-stop test**

Select `PIPELINE_FAIL_STATE`, run enough fast cycles to execute three 50 Hz decisions, and assert:

```c
assert(emergency_stops == 1U);
assert(velocity_command_calls == 0U);
assert(motion_update_calls == 21U); /* decisions at cycles 0, 10, and 20 */
```

Then clear the failure, execute one successful decision, inject another single failure, and assert no second emergency stop. This proves only a fully successful pipeline resets the consecutive-failure counter.

- [ ] **Step 4: Implement one sequential status chain in `ControlApp_RunFastCycle()`**

Keep the logic in the existing function. Replace overwritten statuses with guarded calls:

```c
sd_status_t status = preprocess_update(PlatformTime_GetUs64(), &g_sensor_frame);

if (status == SD_OK) {
    status = state_evaluator_update(&g_state_evaluator, &g_sensor_frame);
}
if (status == SD_OK) {
    status = perception_update(&g_perception, &g_sensor_frame.ir,
                               g_sensor_frame.timestamp_us,
                               &g_perception_result);
}
if (status == SD_OK) {
    g_behavior_input.vehicle = &g_state_evaluator.state;
    g_behavior_input.perception = &g_perception_result;
    g_behavior_input.path_curvature = 0.0f;
    g_behavior_input.command =
        g_behavior_output.state == BEHAVIOR_STATE_IDLE
            ? BEHAVIOR_CMD_START
            : BEHAVIOR_CMD_NONE;
    status = behavior_planner_update(&g_behavior_planner, &g_behavior_input,
                                     &g_behavior_output);
}
if (status == SD_OK) {
    status = trajectory_generate(&g_trajectory_generator,
                                 &g_state_evaluator.state,
                                 &g_behavior_output, dt, &g_trajectory);
}
```

Only inside a final `if (status == SD_OK)` block should the code calculate corrected omega, submit the velocity command, update lap state, and reset `g_critical_failure_count`. The failure branch increments the counter and calls `MotionControl_EmergencyStop()` when it reaches 3. Leave `MotionControl_Update()` outside the 50 Hz branch so it still executes every 2 ms.

- [ ] **Step 5: Update the runtime contract and run all host tests**

Update `inc/control_app.h` to state that a failed 50 Hz stage skips every downstream stage and does not submit a new command.

Run:

```powershell
powershell -ExecutionPolicy Bypass -File ".\tests\run_tests.ps1"
```

Expected: all short-circuit, emergency-stop, startup, ICM, timer, adapter, Motion Control, and square-path tests pass; final line is `Host tests: PASS`.

- [ ] **Step 6: Commit Task 4 if commits were explicitly requested for the execution session**

```powershell
git add -- "src/control_app.c" "inc/control_app.h" "tests/test_control_app.c"
git commit -m "fix: stop failed decision pipelines"
```

---

### Task 5: Verify Target Linkage and Update Integration Documentation

**Files:**
- Modify: `README.md:101-168`
- Modify: `WIRING_AND_SYSCONFIG.md:5-46,130-136`
- Modify: `../../modules/ICM42688/README.md:64-77,89-115,143-171`
- Modify only for factual consistency: `docs/superpowers/specs/2026-07-18-icm42688-hardware-integration-design.md`
- Do not mark hardware-only completion without evidence: `docs/superpowers/plans/2026-07-18-modules-migration.md:716-780`

**Interfaces:**
- Consumes: passing host tests, generated target source, AC6 rebuild, final map file, and optional physical-board measurements.
- Produces: reproducible software evidence and documentation that distinguishes implementation from physical validation.

- [ ] **Step 1: Run the clean controller host suite and capture exact evidence**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File ".\tests\run_tests.ps1"
```

Expected: every executable passes and the final line is `Host tests: PASS`. Record the executable count and any added assertions in the final task report; do not cite old binaries already present in `tests/build`.

- [ ] **Step 2: Regenerate SysConfig from the authoritative file**

Before generation, inspect the existing user changes in `NewProject1.syscfg` and generated files; do not overwrite conflicting concurrent edits. When safe, run:

```powershell
& "C:\ti\sysconfig_1.26.2\sysconfig_cli.bat" `
  --product "..\..\controller\documents\sdk\.metadata\product.json" `
  --device MSPM0G3507 `
  --package "LQFP-48(PT)" `
  --script ".\NewProject1.syscfg" `
  --output ".\Debug" `
  --compiler keil
```

Expected: exit code 0; TIMG12 remains 1 MHz, periodic, maximum reload, and `startTimer = DL_TIMER_STOP`; SPI1 and PB20 mappings remain unchanged. Do not hand-edit generated output.

- [ ] **Step 3: Run a full AC6 rebuild**

Run:

```powershell
$builder = Join-Path $env:USERPROFILE ".vscode\extensions\cl.eide-3.27.2\res\tools\win32\unify_builder\unify_builder.exe"
$params = (Resolve-Path -LiteralPath ".\build\Debug\builder.params").Path
& $builder --params-file $params --rebuild --no-color
```

Expected: exit code 0 with updated `build/Debug/NewProject1.axf`, `.hex`, and `.map`. If the local toolchain path is unavailable, report this as an unverified environment dependency rather than claiming target success.

- [ ] **Step 4: Inspect the final map for the formerly disconnected adapter**

Run:

```powershell
rg -n "icm42688_mspm0_bind|icm42688_mspm0_comm|icm42688_mspm0_system|PlatformTime_GetUs32|DL_Timer_startCounter" ".\build\Debug\NewProject1.map"
rg -n "Removing Unused input sections|icm42688_mspm0\.o" ".\build\Debug\NewProject1.map"
```

Expected: binding and adapter symbols are retained in linked sections; `icm42688_mspm0.o` is not listed only as an entirely removed input section. `ahrs_hal` may remain otherwise unused because AHRS integration is a non-goal.

- [ ] **Step 5: Update controller documentation with software facts only**

In `README.md`:

- Replace the zero-timebase and removed-adapter limitations with the implemented startup order and host/build/map evidence.
- State selected mode as +/-8 g, +/-1000 dps, 1 kHz.
- State that Sens-Decision still consumes raw LSB values and receives scales/bias through configuration.
- Retain the UART limitation and all physical-hardware caveats.
- If no board was tested, keep SPI, ICM, timer accuracy, and closed-loop behavior under “尚未验证”.

In `WIRING_AND_SYSCONFIG.md`:

- State that `PlatformTime_Init()` starts TIMG12 and reads it as the microsecond timebase.
- Add `ACCEL_CONFIG0 = 0x26`, `GYRO_CONFIG0 = 0x26`, WHO_AM_I `0x47`, and the scale equations.
- Add an unchecked hardware checklist for static 1 g, zero-rate bias, monotonic time, and axis signs unless actual evidence exists.

- [ ] **Step 6: Correct the reusable module README without claiming its source was changed**

Rewrite the timer section in `../../modules/ICM42688/README.md` as an adapter contract:

- The adapter requires a 1 MHz free-running 32-bit time source named `ICM42688_TIMER_INST` only when AHRS uses that timer callback.
- The consuming controller owns the timer instance, source clock, start behavior, and collision avoidance.
- For `2025e/m0_controller`, that resource is TIMG12 from MFCLK/4 and `PlatformTime_Init()` starts it.
- Remove statements that TIMG0 is mandatory or starts on power-up.
- Keep the module’s generic AHRS example separate from the controller’s actual Sens-Decision/EKF usage.

Do not document the new controller-local metadata API as a reusable-module API because the separate reusable source implementation is intentionally outside this plan.

- [ ] **Step 7: Perform static quality checks and review only intended changes**

Run:

```powershell
git diff --check -- "2025e/m0_controller" "modules/ICM42688/README.md"
git status --short
git diff -- "2025e/m0_controller/modules/ICM42688" "2025e/m0_controller/src/control_app.c" "2025e/m0_controller/src/platform_time.c" "2025e/m0_controller/inc" "2025e/m0_controller/tests" "2025e/m0_controller/README.md" "2025e/m0_controller/WIRING_AND_SYSCONFIG.md" "modules/ICM42688/README.md"
```

Expected: `git diff --check` has no output; review confirms no Sens-Decision algorithm, control gain, pin mapping, UART, DMA, or unrelated worktree change was introduced.

- [ ] **Step 8: Execute the physical hardware acceptance checklist when the board is available**

Keep motors disabled and record:

```text
WHO_AM_I (0x75): expected 0x47
ACCEL_CONFIG0 (0x50): expected 0x26
GYRO_CONFIG0 (0x4F): expected 0x26
stationary |acceleration|: approximately 1 g
stationary calibrated gyro X/Y/Z: approximately 0 dps
timestamp delta: approximately 1 us per timer tick, monotonic
axis signs: documented for the installed orientation
sensor disconnect: initialization fails and motors remain stopped
```

Only after collecting these values may the corresponding Markdown checklist items be marked complete. Without a board, explicitly report every item as pending.

- [ ] **Step 9: Commit Task 5 if commits were explicitly requested for the execution session**

Stage only intended source, tests, generated outputs that genuinely changed from the authoritative SysConfig, and documentation:

```powershell
git add -- "modules/ICM42688/inc/icm42688_hal.h" "modules/ICM42688/inc/icm42688_mspm0.h" "modules/ICM42688/src/icm42688_hal.c" "modules/ICM42688/src/icm42688_mspm0.c" "inc/platform_time.h" "src/platform_time.c" "src/control_app.c" "inc/control_app.h" "tests/fakes/ti_msp_dl_config.h" "tests/test_platform_time.c" "tests/test_icm42688.c" "tests/test_control_app.c" "tests/run_tests.ps1" "README.md" "WIRING_AND_SYSCONFIG.md" "docs/superpowers/specs/2026-07-18-icm42688-hardware-integration-design.md" "docs/superpowers/plans/2026-07-18-icm42688-hardware-integration.md" "../../modules/ICM42688/README.md"
git commit -m "fix: connect ICM42688 to controller hardware"
```

Do not stage pre-existing modified binaries under `tests/build`, unrelated `.superpowers/sdd` reports, generated files changed by another worker, PCB archives, submodule dirtiness, or unrelated configuration files.

## Plan Self-Review

- Spec coverage: explicit FS_SEL mapping, +/-8 g and +/-1000 dps scales, calibration bias synchronization, MSPM0 binding, MCP status semantics, Sens-Decision defaults, TIMG12 ownership, runtime short-circuiting, host tests, target build/map evidence, hardware acceptance, and Markdown updates each map to a task.
- Placeholder scan: no TBD/TODO/future implementation placeholders remain; hardware-only steps have exact expected observations and stay explicitly pending without a board.
- Type consistency: metadata uses `float *` scale outputs and `icm42688_vector3f_t *` bias output; startup converts g/LSB to m/s^2/LSB and degree/s values to rad/s; timestamps remain `uint32_t`/`uint64_t`; MCP fakes use `mcp23017_status_t`.
- Register consistency: production +/-8 g and +/-1000 dps both map to FS_SEL `1`; at 1 kHz ODR `0x06`, both CONFIG0 values are `0x26` everywhere in this plan and the corrected design.
- Scope check: no Sens-Decision algorithm, AHRS, UART, DMA, dual-IMU, pin, encoder, IR-unit, or control-tuning work is included.
- Worktree safety: execution must isolate or carefully preserve the current dirty workspace, especially existing generated SysConfig files, test binaries, reports, PCB artifacts, and submodules.
