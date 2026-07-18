# Task 4 Status Report

## Status: DONE

## Deliverables

### 1. `temp/tests/run_tests.ps1`
- PowerShell script with `$ErrorActionPreference = 'Stop'`.
- Resolves module root from script location (invokable from anywhere).
- Builds and runs three test programs in sequence with the exact GCC commands from the brief.
- Exits 1 on first compile or test failure.
- Prints `ICM42688 host tests: PASS` on full success.

### 2. `README.md` (Chinese prose, English code/pins/terms)
Sections included:
- 目录结构
- 引脚连接（pin table with PA16/PA17/PA18/PB20, verbatim from brief）
- SysConfig 配置：SPI1 (Controller, MOTO3 POL0/PHA0, 8-bit, MSB first, 1 MHz SCR=15, software CS)
- GPIO：PB20 push-pull, init high
- TIMG0：实例名 `ICM42688_TIMER`, BUSCLK 32 MHz, prescale 31, 1 MHz, 32-bit periodic/free-running, max period, start on power-up, no interrupt; explains `ICM42688_TIMER_INST` generation
- EIDE 集成（add src to srcDirs, inc to include paths; do not modify module's .eide; user does it in controller）
- 初始化序列（verbatim `imu_config` + call sequence from brief）
- 主循环示例（dps→rad/s = PI/180, ahrs_update_6axis, ahrs_get_attitude）
- FreeRTOS 注意事项（vTaskDelayUntil OK for scheduling, but dt must come from TIMG0; do not use xTaskGetTickCount; SysTick used by FreeRTOS; TIMG0 independent）
- 六轴局限（yaw drifts, not absolute heading）
- 上板验证清单（WHO_AM_I=0x47, ~1 g static, gyro ~0 after cal, roll/pitch orientation）
- 故障排查（0x00/0xFF wiring, ID error CS/mode, NaN dt/cal, reversed axis mapping, FreeRTOS timing TIMG0 reconfig）
- 主机测试（run_tests.ps1 command and expected output）

## Verification

### Step 3 — Run host test runner
Command: `powershell -ExecutionPolicy Bypass -File .\temp\tests\run_tests.ps1`
Result:
- test_icm42688: PASS (10 cases)
- test_ahrs: PASS (16 assertions)
- test_mspm0_adapter: PASS (9 cases)
- Final line: `ICM42688 host tests: PASS`
- Tests passed: 3/3 programs

### Step 4 — Strict compile checks
- `gcc -std=c99 -Wall -Wextra -Werror -Iinc -c src/icm42688_hal.c -o temp/icm42688_hal.o` → exit 0, no warnings
- `gcc -std=c99 -Wall -Wextra -Werror -Iinc -c src/ahrs_hal.c -o temp/ahrs_hal.o` → exit 0, no warnings
- `gcc -std=c99 -Wall -Wextra -Werror -Itemp/tests/fakes -Iinc -c src/icm42688_mspm0.c -o temp/icm42688_mspm0.o` → exit 0, no warnings

### Step 5 — Scope/worktree boundaries
`git status --short` shows changes only under `modules/ICM42688/`:
- New: inc/ahrs_hal.h, inc/icm42688_hal.h, inc/icm42688_mspm0.h, src/ahrs_hal.c, src/icm42688_hal.c, src/icm42688_mspm0.c, temp/, README.md
- `controller/` has NO modifications.
- No `.syscfg` files modified.
- Unrelated entries (`.superpowers/sdd/progress.md`, `modules/motor/documents/sdk`, `pcb/pro-api-sdk`, old `icm42688.h`/`icm42688.c` deletions from prior task renames) are outside this task's scope and not touched by Task 4.

## Toolchain
- gcc 8.1.0 (x86_64-posix-seh-rev0, MinGW-W64) at `E:\Softwares\mingw64\bin\gcc.exe`

## Concerns
None. All steps pass as specified.
