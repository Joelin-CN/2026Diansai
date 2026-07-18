# ICM42688 Hardware Integration Design

## Status

Approved on 2026-07-18. This design covers the on-board ICM42688 SPI data path in `2025e/m0_controller`. It does not add a UART host protocol or change Sens-Decision algorithms.

## Problem

The portable ICM42688 core is compiled and called, but the production startup path never binds the MSPM0 SPI callbacks. `icm42688_init()` therefore returns `ICM42688_STATUS_NOT_READY`, and the linked image removes `icm42688_mspm0.o` as unreachable.

Four additional integration defects prevent a usable hardware path:

- `ControlApp_Init()` treats `MCP23017_STATUS_OK == 0` as failure.
- `g_sens_decision_config` is validated before `sd_config_reset_defaults()` initializes it.
- `PlatformTime_GetUs32()` always returns zero and TIMG12 is never started.
- ICM range enums are shifted directly into FS_SEL fields even though their software order does not match the register encoding.

The current gyro calibration is also ineffective for the EKF path. The ICM HAL subtracts its bias only from `gyro_dps`, while `SensorAdapter` intentionally passes raw samples to Sens-Decision.

## Goals

- Make the production startup bind and initialize the physical ICM42688 over SPI1.
- Configure one authoritative operating mode: accelerometer +/-8 g, gyroscope +/-1000 dps, 1 kHz ODR.
- Keep Sens-Decision sensor interfaces, filtering, EKF, perception, behavior, and trajectory logic unchanged.
- Derive Sens-Decision scale and bias parameters from the active ICM configuration instead of maintaining independent constants.
- Restore a monotonic microsecond timestamp using the existing TIMG12 resource.
- Preserve fail-safe behavior: no motor motion after an initialization or decision-pipeline failure.
- Provide host-test, target-build, map-file, and hardware-acceptance evidence.

## Non-Goals

- No UART, USB, BLE, telemetry, or host-side ICM application.
- No AHRS integration; the current controller consumes gyro Z through Sens-Decision/EKF.
- No changes to control gains, trajectory behavior, IR semantics, encoder geometry, or Motion Control algorithms.
- No SPI DMA conversion; the existing blocking SPI adapter remains the transfer owner for this change.
- No second ICM42688 instance or pin remapping.

## Architecture

### Configuration Ownership

The ICM configuration is the single source of truth for IMU range and ODR. Production startup owns a static `icm42688_config_t` with:

- interface: SPI
- accelerometer range: +/-8 g
- gyroscope range: +/-1000 dps
- sample rate: 1 kHz

The ICM HAL exposes conversion metadata for the active configuration:

- acceleration scale in g/LSB
- angular-rate scale in degree/s/LSB
- calibrated angular-rate bias in degree/s for X, Y, and Z

`ControlApp_Init()` converts these values once into the existing Sens-Decision units:

```text
accel_scale_mps2_per_lsb = icm_accel_g_per_lsb * 9.80665
gyro_scale_radps_per_lsb = icm_gyro_dps_per_lsb * pi / 180
gyro_bias_radps          = icm_gyro_bias_dps * pi / 180
```

For the selected ranges, the expected values are:

```text
accel: 1 / 4096 g/LSB = 9.80665 / 4096 m/s^2/LSB
gyro:  1000 / 32768 degree/s/LSB = (pi / 180) * 1000 / 32768 rad/s/LSB
```

Sens-Decision continues receiving raw signed 16-bit samples. Only its existing configuration fields change; no preprocessing or estimation algorithm changes.

### Register Encoding

The ICM HAL maps each range enum explicitly to the device FS_SEL field. It must not derive register bits by shifting the enum ordinal.

For +/-8 g and +/-1000 dps at 1 kHz:

```text
ACCEL_CONFIG0 = 0x20 | 0x06 = 0x26
GYRO_CONFIG0  = 0x20 | 0x06 = 0x26
```

Tests assert these exact writes and cover all enum-to-FS_SEL mappings so future enum changes cannot silently alter hardware configuration.

### Initialization Order

`ControlApp_Init()` performs the following order while motors remain stopped:

1. Validate target laps.
2. Initialize and stop motors.
3. Initialize encoders.
4. Initialize and probe MCP23017 using explicit `MCP23017_STATUS_OK` comparisons.
5. Reset Sens-Decision configuration to defaults.
6. Initialize and start TIMG12 through `PlatformTime_Init()`.
7. Bind the MSPM0 ICM adapter with the production configuration.
8. Initialize ICM42688 and verify WHO_AM_I `0x47`.
9. Calibrate the gyroscope while the robot is stationary.
10. Copy the active ICM scales and calibrated bias into the existing Sens-Decision IMU configuration.
11. Configure and initialize the target sensor HAL.
12. Initialize Sens-Decision objects and square-path state.
13. Initialize and start Motion Control.

Any failed step stops motors and returns `false`. Binding itself remains a void operation because validation is enforced by `icm42688_init()` and the metadata getters.

### Timebase

TIMG12 remains the sole microsecond time source:

- 4 MHz MFCLK divided by 4
- 1 MHz timer tick
- 32-bit periodic down-counter
- reload value `UINT32_MAX`
- no timer interrupt

`PlatformTime_Init()` resets wrap-extension state and starts the generated `ICM42688_TIMER_INST`. `PlatformTime_GetUs32()` reads the down-counter and converts it to an increasing 32-bit microsecond count. `PlatformTime_GetUs64()` extends wrap under a short interrupt-safe critical section while preserving the caller's previous interrupt-enabled state.

This path supplies monotonically increasing timestamps to Sens-Decision and remains independent of the FreeRTOS tick.

### Runtime Failure Handling

The 50 Hz pipeline is sequential. If any of these stages fails, later stages are not executed and no new velocity command is submitted for that cycle:

- preprocessing
- state evaluation
- perception
- behavior planning
- trajectory generation

A failure increments the existing critical-failure counter. After the configured threshold, Motion Control enters emergency stop. A successful complete pipeline resets the counter. The 500 Hz Motion Control update still runs so its stop state is applied deterministically.

## Interfaces

The ICM HAL gains read-only metadata functions for the active bound configuration and calibrated bias. They do not expose mutable internal state and return an explicit status for unbound or invalid output arguments.

The MSPM0 adapter remains responsible only for SPI/GPIO/delay/time callbacks. Application policy, chosen ranges, calibration count, and Sens-Decision synchronization remain in `ControlApp_Init()`.

No new source module is required.

## Test Strategy

### ICM HAL Tests

- Assert +/-8 g, +/-1000 dps, and 1 kHz produce `ACCEL_CONFIG0 == 0x26` and `GYRO_CONFIG0 == 0x26`.
- Table-test every accelerometer and gyroscope enum against its FS_SEL value.
- Verify active scale metadata before and after binding.
- Verify calibration metadata matches the averaged raw samples in degree/s.
- Verify invalid arguments and unbound access return errors.

### Control Application Tests

- Replace Boolean MCP fakes with the real status enum semantics.
- Record and assert `PlatformTime_Init -> ICM bind -> ICM init -> calibration -> sensor init` ordering.
- Assert `sd_config_reset_defaults()` is called before sensor validation.
- Assert selected ICM configuration is +/-8 g, +/-1000 dps, and 1 kHz.
- Assert Sens-Decision receives the derived scale and calibrated bias values.
- Verify each initialization failure leaves motors stopped.
- Verify each 50 Hz stage failure prevents later stages and velocity-command submission.

### Platform Time Tests

- Build the production target implementation against fake DriverLib.
- Verify timer start, current-count conversion, monotonic reads, 32-bit wrap, and interrupt-state restoration.
- Verify successive 50 Hz timestamps are non-zero and increasing.

### Verification

Run:

```powershell
powershell -ExecutionPolicy Bypass -File ".\tests\run_tests.ps1"
```

Then run SysConfig generation and an EIDE/AC6 full rebuild. Inspect `build/Debug/NewProject1.map` to confirm that `icm42688_mspm0.o`, `icm42688_mspm0_bind`, SPI adapter interfaces, and platform-time target calls are retained.

Hardware acceptance remains mandatory and distinct from build success:

- WHO_AM_I reads `0x47`.
- `ACCEL_CONFIG0` and `GYRO_CONFIG0` read back as `0x26`.
- Stationary acceleration magnitude is approximately 1 g.
- Calibrated stationary gyro is approximately zero without changing the robot orientation during startup.
- Timestamp increments at approximately 1 us per tick and remains monotonic.
- Axis signs are checked against the installed sensor orientation.
- Motors remain disabled for failed sensor initialization and during sensor-only acceptance.

## Documentation

Update after implementation and verification:

- `README.md`: replace resolved linker/timebase limitations with the exact initialization and verification status; retain all unverified hardware caveats.
- `WIRING_AND_SYSCONFIG.md`: document +/-8 g, +/-1000 dps, 1 kHz, `0x26` register values, TIMG12 runtime ownership, and hardware acceptance results.
- `modules/ICM42688/README.md` in the reusable module source: replace obsolete TIMG0 assumptions with an adapter-neutral contract and point the controller integration to TIMG12.
- Existing migration plan/report files: mark steps complete only when their corresponding test, build, map, or hardware evidence exists.

## Risks

- The selected startup calibration blocks for approximately one second. This is acceptable before scheduler start, but the robot must be stationary.
- A board-level SPI wiring, power, or mode defect will still fail WHO_AM_I; firmware must not mask that failure.
- Real sensor noise and mounting orientation require hardware validation even when unit tests and register reads pass.
- Other known encoder, IR-unit, and control-parameter inconsistencies are outside this design and must not be silently changed as part of the ICM integration.
