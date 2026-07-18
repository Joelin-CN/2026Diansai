### Task 4: README, Test Runner, and End-to-End Verification

**Files:**
- Create: `temp/tests/run_tests.ps1`
- Create: `README.md`
- Verify only: `inc/*.h`, `src/*.c`, `temp/tests/*`

**Interfaces:**
- Consumes: all public APIs and SysConfig symbol contract from Tasks 1-3.
- Produces: repeatable host verification and complete instructions for integrating into controller/mspm0 without modifying it.

- [ ] **Step 1: Add one-command host verification**

Create `temp/tests/run_tests.ps1` with `$ErrorActionPreference = 'Stop'`, running three GCC builds and three test programs in sequence. Any compile or test failure exits immediately. On all success, output:

```text
ICM42688 host tests: PASS
```

The three test programs and their build commands:
1. test_icm42688: `gcc -std=c99 -Wall -Wextra -Werror -Iinc temp/tests/test_icm42688.c src/icm42688_hal.c -lm -o temp/tests/test_icm42688.exe`
2. test_ahrs: `gcc -std=c99 -Wall -Wextra -Werror -Iinc temp/tests/test_ahrs.c src/ahrs_hal.c -lm -o temp/tests/test_ahrs.exe`
3. test_mspm0_adapter: `gcc -std=c99 -Wall -Wextra -Werror -Itemp/tests/fakes -Iinc temp/tests/test_mspm0_adapter.c src/icm42688_mspm0.c src/icm42688_hal.c src/ahrs_hal.c -lm -o temp/tests/test_mspm0_adapter.exe`

- [ ] **Step 2: Write the final README**

`README.md` must contain these actionable sections:

**Pin table:**
| Signal | MSPM0 Pin | Direction |
|---|---|---|
| SPI1_POCI | PA16 | ICM42688 to MSPM0 |
| SPI1_SCLK | PA17 | MSPM0 to ICM42688 |
| SPI1_PICO | PA18 | MSPM0 to ICM42688 |
| ICM42688_CS | PB20 | MSPM0 to ICM42688, active low |

**SPI1 SysConfig:** Controller mode, MOTO3 POL0/PHA0, 8-bit, MSB first, 1 MHz (SCR=15), software CS.

**GPIO:** PB20 push-pull output, initialized high.

**TIMG0 SysConfig:** Instance name `ICM42688_TIMER`, BUSCLK 32 MHz, prescale 31, 1 MHz, 32-bit periodic/free-running, max period, start on power-up, no interrupt. Explain this generates `ICM42688_TIMER_INST`.

**EIDE integration:** Add `../../modules/ICM42688/src` to srcDirs, add `../../modules/ICM42688/inc` to include paths. Do NOT modify .eide/eide.yml in this module — user does it in controller project.

**Initialization sequence:**
```c
static const icm42688_config_t imu_config = {
    .interface_type = ICM42688_INTERFACE_SPI,
    .acc_sample = ICM42688_ACC_SAMPLE_SGN_8G,
    .gyro_sample = ICM42688_GYRO_SAMPLE_SGN_1000DPS,
    .sample_rate = ICM42688_SAMPLE_RATE_1000,
};

SYSCFG_DL_init();
icm42688_mspm0_bind(&imu_config);
if (icm42688_init() != ICM42688_STATUS_OK) {
    for (;;) { __WFI(); }
}
ahrs_hal_init(&icm42688_mspm0_timer);
ahrs_init();
```

**Main loop example:** Read sensor, convert gyro dps to rad/s (multiply by PI/180), call ahrs_update_6axis, get attitude.

**FreeRTOS note:** Periodic task can use vTaskDelayUntil for scheduling, but AHRS dt must still come from TIMG0. Do NOT use xTaskGetTickCount() for dt (1ms resolution insufficient). SysTick is used by FreeRTOS; TIMG0 is independent.

**Six-axis limitation:** yaw drifts over time, not absolute heading.

**On-board verification checklist:**
- WHO_AM_I reads 0x47
- Stationary accel magnitude ~1 g
- After calibration, gyro near 0 dps
- roll/pitch direction matches mounting orientation

**Troubleshooting:**
- All 0x00 or all 0xFF: check SPI wiring
- ID error: check CS and SPI mode
- NaN attitude: check dt range, sensor stationary during calibration
- Reversed attitude: adjust axis mapping for mounting orientation
- FreeRTOS timing anomaly: verify TIMG0 not reconfigured by other code

- [ ] **Step 3: Run all host tests from the documented command**

```powershell
powershell -ExecutionPolicy Bypass -File .\temp\tests\run_tests.ps1
```
Expected final line: `ICM42688 host tests: PASS`

- [ ] **Step 4: Compile-check production C files with strict warnings**

Core and AHRS:
```powershell
gcc -std=c99 -Wall -Wextra -Werror -Iinc -c src/icm42688_hal.c -o temp/icm42688_hal.o
gcc -std=c99 -Wall -Wextra -Werror -Iinc -c src/ahrs_hal.c -o temp/ahrs_hal.o
```
Adapter with fake DriverLib:
```powershell
gcc -std=c99 -Wall -Wextra -Werror -Itemp/tests/fakes -Iinc -c src/icm42688_mspm0.c -o temp/icm42688_mspm0.o
```
Expected: all commands exit 0 with no warnings.

- [ ] **Step 5: Confirm scope and worktree boundaries**

```powershell
git status --short
```
Expected: only `modules/ICM42688` contains changes; `controller/` has no modifications.
