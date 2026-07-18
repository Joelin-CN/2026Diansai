# Task 7 Final Acceptance Verification Report

## Status

**DONE**

Fresh final acceptance verification completed successfully. The full seven-program host suite passed, the required second non-incremental EIDE/unify_builder rebuild passed after its mandatory SysConfig prebuild, the resulting image passed artifact/link/object/exclusion/memory checks, and the post-prebuild generated configuration preserved the user-approved TIMG12 amendment and every documented physical mapping.

No production source or configuration file was edited, no commit was created, and no existing worktree change was reverted or cleaned. This report is the only manually created Task 7 file. The host test runner refreshed `tests/build` as part of its normal clean-run behavior; those binaries were preserved as instructed.

## 1. Fresh Host Suite

The exact brief was read first. Before running tests, `tests/run_tests.ps1` was inspected and confirmed to clean its own output directory:

```powershell
if (Test-Path -LiteralPath $build) {
    Remove-Item -Recurse -Force -LiteralPath $build
}
New-Item -ItemType Directory -Path $build | Out-Null
```

Exact command:

```powershell
powershell -ExecutionPolicy Bypass -File ".\tests\run_tests.ps1"
```

Result:

- Exit code: `0`.
- Final line: `Host tests: PASS`.
- Seven executables compiled and ran:
  - `test_platform_time.exe`
  - `test_icm42688.exe`
  - `test_mcp23017.exe`
  - `test_motion_control.exe`
  - `test_target_adapters.exe`
  - `test_square_path.exe`
  - `test_control_app.exe`

The only diagnostics were the two existing MCP23017 host-test warnings:

```text
tests/test_mcp23017.c:107:14: warning: unused variable 'mask' [-Wunused-variable]
tests/test_mcp23017.c:118:14: warning: unused variable 'mask' [-Wunused-variable]
```

These warnings did not fail compilation or execution and were not changed during final verification.

## 2. Second Full EIDE Rebuild

Exact builder and parameter paths:

```text
C:\Users\Joelin\.vscode\extensions\cl.eide-3.27.2\res\tools\win32\unify_builder\unify_builder.exe
E:\B306\2026\电赛\2025e\m0_controller\build\Debug\builder.params
```

Exact command arguments:

```powershell
& "C:\Users\Joelin\.vscode\extensions\cl.eide-3.27.2\res\tools\win32\unify_builder\unify_builder.exe" --params-file "E:\B306\2026\电赛\2025e\m0_controller\build\Debug\builder.params" --rebuild --no-color
```

Fresh execution evidence:

- Start: `2026-07-18T18:42:34.0505272+08:00`.
- End: `2026-07-18T18:42:36.2691832+08:00`.
- Exit code: `0`.
- Builder result: `[ DONE ] build successfully !`.
- Builder elapsed time: `0:0:2`.
- Builder explicitly reported `file statistics (rebuild mode)`.
- Statistics: 35 C files, 0 C++ files, 1 assembly file, 1 library/object input, 37 total inputs.
- The console showed all 35 C files and the startup assembly being compiled before link; this was not accepted as an incremental build.

Mandatory prebuild ordering and result:

```text
[ INFO ] pre-build tasks ...

>> Generate SysConfig        [done]

Running script...
Validating...
0 error(s), 1 warning(s)
Generating Code (NewProject1.syscfg)...
Unchanged ...\Debug\ti_msp_dl_config.c...
Unchanged ...\Debug\ti_msp_dl_config.h...
Unchanged ...\Debug\Event.dot...
```

`build/Debug/compiler.log` independently records:

- `Builder Mode: Rebuild`.
- `>>> prebuild` before `>>> cc` before `>>> ld`.
- SysConfig `0 error(s), 1 warning(s)`.
- Program size `Code=29884 RO-data=620 RW-data=120 ZI-data=10120`.

The one current SysConfig warning is:

```text
warning: /ti/project_config/ProjectConfig: Project Configuration File generation is disabled for this project. SysConfig Project Migration is not currently supported for this project.
```

No compile or link errors occurred.

## 3. Image Acceptance

### Fresh Artifacts

All mandatory paths exist and have timestamps within the fresh rebuild window:

| Artifact | Size | Last write |
|---|---:|---|
| `E:\B306\2026\电赛\2025e\m0_controller\build\Debug\NewProject1.axf` | 346952 bytes | `2026-07-18T18:42:36.0802580+08:00` |
| `E:\B306\2026\电赛\2025e\m0_controller\build\Debug\NewProject1.map` | 249631 bytes | `2026-07-18T18:42:36.0822580+08:00` |
| `E:\B306\2026\电赛\2025e\m0_controller\build\Debug\unify_builder.log` | 4028 bytes | `2026-07-18T18:42:36.2435994+08:00` |
| `E:\B306\2026\电赛\2025e\m0_controller\build\Debug\compiler.log` | 1878 bytes | `2026-07-18T18:42:36.2435994+08:00` |

`unify_builder.log` ends with the fresh entry at `2026-07-18 18:42:36` and `build successfully !`. Its older incremental-history lines were not used as rebuild evidence; current rebuild mode is established by the exact `--rebuild` invocation, console rebuild statistics, `compiler.log`, and fresh output timestamps.

### Exact fromelf Inspection

Command:

```powershell
& "C:\Keil_v5\ARM\ARMCLANG\bin\fromelf.exe" --text --info=sizes ".\build\Debug\NewProject1.axf"
```

Exit code: `0`.

Exact output:

```text
========================================================================

** Object/Image Component Sizes

      Code (inc. data)   RO Data    RW Data    ZI Data      Debug   Object Name

     29884       2406        620        120      10120     273805   .\build\Debug\NewProject1.axf
     29884       2406        620        120          0          0   ROM Totals for .\build\Debug\NewProject1.axf

========================================================================

      Code (inc. data)   RO Data    RW Data    ZI Data      Debug

     29884       2406        620        120      10120     273805   Grand Totals
     29884       2406        620        120          0          0   ROM Totals

========================================================================

    Total RO  Size (Code + RO Data)                30504 (  29.79kB)
    Total RW  Size (RW Data + ZI Data)             10240 (  10.00kB)
    Total ROM Size (Code + RO Data + RW Data)      30624 (  29.91kB)

========================================================================
```

Memory acceptance:

- Total ROM: `30,624 / 131,072` bytes, leaving `100,448` bytes.
- Total RW: `10,240 / 32,768` bytes, leaving `22,528` bytes.
- Both configured 128 KiB Flash and 32 KiB SRAM limits pass.

### Objects and Symbols

`build/Debug/NewProject1.objlist` contains exactly 37 linker inputs: all intended 36 compiled objects and one `driverlib.a`. The 36 objects are:

```text
list.o
port.o
portasm.o
heap_4.o
queue.o
tasks.o
startup_mspm0g350x_uvision.o
ti_msp_dl_config.o
ahrs_hal.o
icm42688_hal.o
icm42688_mspm0.o
mcp23017.o
motion_control.o
motion_feedback.o
motion_feedforward.o
motion_kinematics.o
EKF.o
behavior_planner.o
config.o
interface.o
perception.o
preprocess.o
state_evaluate.o
trajectory_generate.o
utils.o
control_app.o
encoder.o
encoder_adapter.o
encoder_hw_bridge.o
line_sensor.o
main.o
motor.o
motor_adapter.o
platform_time.o
sensor_adapter.o
square_path.o
```

Every object appears in `NewProject1.map`; missing count is `0`. Required FreeRTOS providers and addresses are:

| Symbol | Address | Provider |
|---|---|---|
| `vStartFirstTask` | `0x000003ad` | `portasm.o` |
| `PendSV_Handler` | `0x000003cf` | `portasm.o` |
| `SVC_Handler` | `0x00000411` | `portasm.o` |

The map includes the linked `portasm.o(.text)` contribution.

### Exclusions

Case-insensitive checks of the complete map found none of the following:

- `OLED`.
- `example_usage` source/object/token.
- Any of the seven host-test source/object names: `test_platform_time`, `test_icm42688`, `test_mcp23017`, `test_motion_control`, `test_target_adapters`, `test_square_path`, `test_control_app`.
- Any `tests/` or `tests\` path.
- Any `temp/` or `temp\` path.

Forbidden match count: `0`.

## 4. Post-Prebuild Generated Invariants

The mandatory prebuild reported the generated files unchanged. Their current SHA-256 values remain the previously validated candidate hashes:

| File | SHA-256 |
|---|---|
| `Debug/ti_msp_dl_config.c` | `4598730686e889e1aa4f551e29a4e79d8a839e1cdc25430dc4270c57087cac80` |
| `Debug/ti_msp_dl_config.h` | `5b9a3b851e547ea5ff838e77b8dab6e4845d018bc6d1fac44d9f8507c4bc1bf0` |
| `Debug/Event.dot` | `b058cff0e469cb126dcca593f6fbcb2f28921d8d06aa742cb32964648e56e1af` |

The authoritative `NewProject1.syscfg` SHA-256 is currently `498154f1b8519adc7c67992f84ec62828cbf9a8b52c0855eac0efd9f6691a311`. This differs from the earlier Task 5 report hash because the approved MFCLK configuration was subsequently normalized by removing the redundant locked `SYSCTL.MFCLKEn` assignment; the binding clock-tree gate and generated behavior remain intact. The fresh mandatory prebuild validates this current file with zero errors and leaves generated output unchanged.

Timer and clock results:

- `CONTROL_TIMER`: TIMG0, 32 MHz BUSCLK, 2 ms, load `63999U`, periodic, no autostart, ZERO interrupt.
- `ICM42688_TIMER`: TIMG12, fixed 4 MHz MFCLK divided by 4 with configured prescale 1, effective 1 MHz, maximum load `4294967295U`, periodic, no autostart, no interrupt.
- Generated register prescale is `0U`, representing divide-by-1 and therefore matching configured prescale 1.
- Generated `SYSCFG_DL_ICM42688_TIMER_init` contains no interrupt-enable, `startCounter`, or start call.
- Generated SYSCTL initialization calls `DL_SYSCTL_enableMFCLK()`.
- No timer or peripheral mapping was changed.

Complete physical mapping audit:

| Function | Accepted mapping |
|---|---|
| Motor PWM M1 | TIMA0 CCP0, PA21, PINCM46 |
| Motor PWM M2 | TIMA0 CCP1, PA22, PINCM47 |
| Motor PWM M3 | TIMA1 CCP1, PA24, PINCM54 |
| Motor PWM M4 | TIMA0 CCP3, PA25, PINCM55 |
| PWM behavior | 32 MHz / 1000 = 32 kHz; initial duty 0%; timers start as configured |
| M1_IN1 / M1_IN2 | PB6/PINCM23, PB7/PINCM24 |
| M2_IN1 / M2_IN2 | PB8/PINCM25, PA7/PINCM14 |
| M3_IN1 / M3_IN2 | PA15/PINCM37, PB9/PINCM26 |
| M4_IN1 / M4_IN2 | PB19/PINCM45, PB24/PINCM52 |
| ENC1 A/B | PA12/PINCM34, PA13/PINCM35 |
| ENC2 A/B | PA2/PINCM7, PA26/PINCM59 |
| ENC3 A/B | PA27/PINCM60, PA28/PINCM3 |
| ENC4 A/B | PA31/PINCM6, PB18/PINCM44 |
| Encoder behavior | GPIO inputs, interrupts enabled, both-edge `RISE_FALL` |
| I2C0 | SDA PA0/PINCM1, SCL PA1/PINCM2, controller at 100 kHz |
| SPI1 | POCI PA16/PINCM38, PICO PA18/PINCM40, SCLK PA17/PINCM39 |
| SPI1 behavior | Controller, 8 bit, MSB first, MOTO3 mode 0, 1 MHz |
| ICM42688 CS | GPIO PB20/PINCM48, software-controlled output, initially high |
| SPI1 RX DMA | DMA channel 2, `DMA_SPI1_RX_TRIG` |
| SPI1 TX DMA | DMA channel 3, `DMA_SPI1_TX_TRIG` |
| UART0 | TX PA10/PINCM21, RX PA11/PINCM22, 115200 |
| UART1 | TX PA8/PINCM19, RX PA9/PINCM20, 115200 |
| UART3 | TX PB2/PINCM15, RX PB3/PINCM16, 115200 |
| SWD | SWDIO PA19, SWCLK PA20, DEBUGSS reserved |
| Reset | Dedicated NRST remains reserved, not claimed as GPIO |
| TB6612 STBY / PA23 | STBY remains externally pulled up; PA23 remains unclaimed |

All documented timer, PWM, encoder, I2C, SPI, DMA, UART, motor GPIO, software-CS, SWD, reset, and documented unassigned-GPIO invariants pass after the prebuild. No mapping change was found or ignored.

## 5. Repository Difference Checks

Repair-owned paths checked, including the approved amendment and planning documents:

```text
inc/FreeRTOSConfig.h
modules/Sens-Decision/src/behavior_planner.c
.eide/eide.yml
NewProject1.syscfg
Debug/ti_msp_dl_config.c
Debug/ti_msp_dl_config.h
Debug/Event.dot
docs/superpowers/specs/2026-07-18-firmware-build-repair-design.md
docs/superpowers/plans/2026-07-18-firmware-build-repair.md
```

`git diff --check -- <paths>` result:

- Exit code: `0`.
- No whitespace errors.
- Git emitted only LF-to-CRLF working-copy notices for `.eide/eide.yml`, `NewProject1.syscfg`, `inc/FreeRTOSConfig.h`, and `behavior_planner.c`.

Repair diff stat:

```text
2025e/m0_controller/.eide/eide.yml                 | 10 +++---
2025e/m0_controller/Debug/ti_msp_dl_config.c       | 40 ++++++++++++++++++++++
2025e/m0_controller/Debug/ti_msp_dl_config.h       |  7 ++++
2025e/m0_controller/NewProject1.syscfg             | 12 ++++---
2025e/m0_controller/inc/FreeRTOSConfig.h           |  1 +
.../modules/Sens-Decision/src/behavior_planner.c   |  1 +
6 files changed, 61 insertions(+), 10 deletions(-)
```

The design and plan documents are untracked, so they do not appear in ordinary `git diff --stat`; they are present in `git status --short`. `Debug/Event.dot` is unchanged.

Related current status:

```text
 M .eide/eide.yml
 M Debug/ti_msp_dl_config.c
 M Debug/ti_msp_dl_config.h
 M NewProject1.syscfg
 M inc/FreeRTOSConfig.h
 M modules/Sens-Decision/src/behavior_planner.c
?? docs/superpowers/plans/2026-07-18-firmware-build-repair.md
?? docs/superpowers/specs/2026-07-18-firmware-build-repair-design.md
```

Normal refreshed host-test output preserved:

```text
 M tests/build/test_control_app.exe
 M tests/build/test_icm42688.exe
 M tests/build/test_platform_time.exe
?? tests/build/test_mcp23017.exe
?? tests/build/test_motion_control.exe
?? tests/build/test_square_path.exe
?? tests/build/test_target_adapters.exe
```

Unrelated preexisting/user/agent status entries were left untouched:

- Modified `.superpowers/sdd/modules-migration-progress.md` and `.superpowers/sdd/task-8-report.md`.
- Untracked firmware task briefs/reports/review packages and other `.superpowers/sdd` artifacts.
- Modified `.vscode/settings.json`.
- Modified nested repositories/submodules `controller/documents/sdk` and `pcb/pro-api-sdk`.
- Existing migration/review artifacts outside this repair's production scope.

No revert, cleanup, staging, commit, or production edit was performed.

## 6. Tool and Parameter Integrity

### SysConfig

- Release: `1.26.2+4477`.
- Installed CLI: `C:\ti\sysconfig_1.26.2\sysconfig_cli.bat`.
- CLI exists: `True`.
- `sysconfig_cli.bat --version`: exit `0`, output `1.26.2+4477`.
- Installed metadata `C:\ti\sysconfig_1.26.2\dist\version.txt`: `1.26.2+4477`.
- Retained installer: `C:\Users\Joelin\AppData\Local\Temp\opencode\sysconfig-1.26.2_4477-setup.exe`.
- Installer SHA-256: `b40e31b01987c68c420ea90bb2095885cd97ee9ab23f2447fa37b4a33933fd31`.
- Required hash match: `True`.
- Authenticode status: `Valid`.
- Signer: `CN="Texas Instruments, Inc.", O="Texas Instruments, Inc.", L=Dallas, S=Texas, C=US`.
- Issuer: `DigiCert Trusted G4 Code Signing RSA4096 SHA384 2021 CA1`.

### builder.params

- Provenance: user generated through `F1 -> EIDE: Generate builder.params` before Task 6 resumed; Task 6 recorded it as current and explicitly not manually edited. Task 7 did not edit it.
- Path: `E:\B306\2026\电赛\2025e\m0_controller\build\Debug\builder.params`.
- Size: 8740 bytes.
- Last write: `2026-07-18T18:26:35.3432212+08:00`.
- SHA-256: `1378fbc64c8fde9a975e1b0f0f880f3851908bcf2089a8b8c031bbe52863426a`, exactly matching Task 6 provenance.
- Name: `NewProject1`.
- Target: `Debug`.
- Toolchain: `AC6` at `C:\Keil_v5\ARM\ARMCLANG`.
- Limits: RAM 32768 bytes, ROM 131072 bytes.
- Source list: 35 C, one assembly, one driver library; no test/example/OLED/temp input.
- Before-build task count: 1.
- Before-build task: `Generate SysConfig`.
- `stopBuildAfterFailed`: `true`.
- Complete command:

```text
C:/ti/sysconfig_1.26.2/sysconfig_cli.bat --product "${ProjectRoot}/../../controller/documents/sdk/.metadata/product.json" --device MSPM0G3507 --package "LQFP-48(PT)" --script "${ProjectRoot}/NewProject1.syscfg" --output "${ProjectRoot}/Debug" --compiler keil
```

EIDE 3.27.2 serializes `${ProjectRoot}` in the command and supplies its absolute value in `env.ProjectRoot`. The successful mandatory prebuild and absolute generated-file paths prove runtime expansion. `builder.params` is current and was accepted as EIDE-generated, per user instruction.

## 7. Residual Issues and Hardware Boundary

Known non-blocking residual diagnostics:

- SysConfig emits one ProjectConfig migration-support warning during the current prebuild.
- Host GCC emits the two existing MCP23017 unused-variable warnings at lines 107 and 118.
- SysConfig's generated TIMG12 comment displays an internally inconsistent source-side equation, but emitted selections are unambiguous: fixed 4 MHz MFCLK, divide 4, divide-by-1 prescale, effective 1 MHz.
- Git reports LF-to-CRLF notices on several repair-owned working-copy files; `git diff --check` reports no whitespace errors.

Hardware boundary:

- No firmware flashing was performed.
- No target board or physical robot was exercised.
- No on-device timer, interrupt, motor, encoder, I2C, SPI, DMA, UART, GPIO, sensor, scheduler, or control-loop behavior is claimed.
- Runtime hardware correctness, electrical wiring, measured timing, and end-to-end robot behavior remain hardware-only acceptance work outside this build-repair task.

Final software/build acceptance status: **DONE**.
