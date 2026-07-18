# MSPM0 Firmware Build Repair Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Restore a complete, reproducible EIDE and Arm Compiler 6 firmware build for the MSPM0G3507 controller.

**Architecture:** Keep the existing EIDE AC6 target and mandatory SysConfig pre-build stage. Make only the two required C source corrections and the minimal EIDE task-schema correction, install the exact SysConfig release expected by SDK 2.10.00.04, regenerate DriverLib configuration from `NewProject1.syscfg`, and verify the complete linked target image.

**Tech Stack:** C99, TI MSPM0 SDK 2.10.00.04, TI SysConfig 1.26.2.4477, FreeRTOS 11.2.0, EIDE 3.27.2, unify_builder 3.11.1, Arm Compiler for Embedded 6.21, MinGW GCC host tests, Windows PowerShell 5.1.

## Global Constraints

- Target is MSPM0G3507, Cortex-M0+, LQFP-48, with 128 KiB Flash and 32 KiB SRAM.
- Use repository SDK `../../controller/documents/sdk`; its `.metadata/product.json` version must remain `2.10.00.04`.
- Install SysConfig `1.26.2.4477` at `C:/ti/sysconfig_1.26.2`.
- Use Arm Compiler for Embedded 6.21 at `C:/Keil_v5/ARM/ARMCLANG`.
- Keep EIDE target `Debug`, toolchain `AC6`, and output root `build/Debug`.
- Keep the mandatory SysConfig pre-build step and `stopBuildAfterFailed: true`.
- Do not manually edit `Debug/ti_msp_dl_config.c` or `Debug/ti_msp_dl_config.h`.
- Stop for user review if SysConfig changes a documented pin or peripheral assignment, reports a conflict, or produces an unexpected broad rewrite.
- Do not change control, perception, trajectory, planner, pin-assignment, scheduling, heap, interrupt-priority, or uploader behavior.
- Do not add a wrapper build system, GCC target build, CI pipeline, or fallback that skips SysConfig.
- Do not modify or revert unrelated worktree changes.
- Use `C:/Users/Joelin/AppData/Local/Temp/opencode` for diagnostic object files and downloaded installers outside the workspace.
- Do not modify files under `pictures/`.
- Do not commit unless the user explicitly requests a commit during execution.

## File Map

**Production source repairs**

- `inc/FreeRTOSConfig.h`: explicitly disables the unused FreeRTOS MPU mode required by the selected Cortex-M0+ port.
- `modules/Sens-Decision/src/behavior_planner.c`: includes the standard definition of `NULL`.

**Build integration repair**

- `.eide/eide.yml`: stores the complete SysConfig invocation in EIDE's supported `command` field and uses `${ProjectRoot}`.
- `NewProject1.syscfg`: authoritative peripheral and pin configuration; no planned edit.
- `Debug/ti_msp_dl_config.c`: generated implementation; changed only by SysConfig.
- `Debug/ti_msp_dl_config.h`: generated declarations; changed only by SysConfig.
- `Debug/Event.dot`: generated dependency graph; changed only if emitted by SysConfig.

**Verification inputs and outputs**

- `tests/run_tests.ps1`: existing host-test entry point; no planned edit.
- `build/Debug/builder.params`: EIDE-generated build snapshot; ignored build output, not manually edited.
- `build/Debug/NewProject1.axf`: expected target image.
- `build/Debug/NewProject1.map`: expected link map.
- `build/Debug/unify_builder.log` and `build/Debug/compiler.log`: target-build evidence.

---

### Task 1: Repair the FreeRTOS Port Configuration

**Files:**
- Modify: `inc/FreeRTOSConfig.h:6-17`
- Reference: `../../controller/documents/sdk/kernel/freertos/Source/portable/GCC/ARM_CM0/portmacro.h:48-50`
- Reference: `../../controller/documents/sdk/kernel/freertos/builds/LP_MSPM0G3507/release/FreeRTOSConfig.h:76-78`

**Interfaces:**
- Consumes: FreeRTOS preprocessor contract `configENABLE_MPU` with value `0` or `1`.
- Produces: non-MPU Cortex-M0+ FreeRTOS configuration with `configENABLE_MPU == 0`.

- [ ] **Step 1: Reproduce the missing FreeRTOS setting with AC6**

Run from the project root:

```powershell
$out = "C:\Users\Joelin\AppData\Local\Temp\opencode\m0-build-repair"
if (-not (Test-Path -LiteralPath "C:\Users\Joelin\AppData\Local\Temp\opencode")) {
    throw "Approved temp parent is missing"
}
if (-not (Test-Path -LiteralPath $out)) {
    New-Item -ItemType Directory -Path $out | Out-Null
}
& "C:\Keil_v5\ARM\ARMCLANG\bin\armclang.exe" `
  --target=arm-arm-none-eabi `
  -mcpu=cortex-m0plus `
  -mfloat-abi=soft `
  -std=c99 `
  -Os `
  -g `
  -fshort-enums `
  -fshort-wchar `
  -D__MSPM0G3507__ `
  -I. `
  -Iinc `
  -IDebug `
  "-Imodules/MCP23017/inc" `
  "-Imodules/ICM42688/inc" `
  "-Imodules/Motion Control/inc" `
  "-Imodules/Sens-Decision/inc" `
  "-I../../controller/documents/sdk/source" `
  "-I../../controller/documents/sdk/source/third_party/CMSIS/Core/Include" `
  "-I../../controller/documents/sdk/kernel/freertos/Source/include" `
  "-I../../controller/documents/sdk/kernel/freertos/Source/portable/GCC/ARM_CM0" `
  -c "src/main.c" `
  -o "$out\main-before.o"
if ($LASTEXITCODE -eq 0) { throw "Expected the pre-fix compile to fail" }
```

Expected: AC6 exits nonzero with:

```text
configENABLE_MPU must be defined in FreeRTOSConfig.h
```

- [ ] **Step 2: Add the minimal non-MPU setting**

Add one definition in the scheduler configuration block of
`inc/FreeRTOSConfig.h`:

```c
/* Scheduler and clock configuration. CPUCLK_FREQ is currently 32 MHz. */
#define configENABLE_MPU                       0
#define configUSE_PREEMPTION                    1
```

Do not change any other macro.

- [ ] **Step 3: Recompile `main.c` without diagnostic workarounds**

Run:

```powershell
$out = "C:\Users\Joelin\AppData\Local\Temp\opencode\m0-build-repair"
& "C:\Keil_v5\ARM\ARMCLANG\bin\armclang.exe" `
  --target=arm-arm-none-eabi `
  -mcpu=cortex-m0plus `
  -mfloat-abi=soft `
  -std=c99 `
  -Os `
  -g `
  -fshort-enums `
  -fshort-wchar `
  -D__MSPM0G3507__ `
  -I. `
  -Iinc `
  -IDebug `
  "-Imodules/MCP23017/inc" `
  "-Imodules/ICM42688/inc" `
  "-Imodules/Motion Control/inc" `
  "-Imodules/Sens-Decision/inc" `
  "-I../../controller/documents/sdk/source" `
  "-I../../controller/documents/sdk/source/third_party/CMSIS/Core/Include" `
  "-I../../controller/documents/sdk/kernel/freertos/Source/include" `
  "-I../../controller/documents/sdk/kernel/freertos/Source/portable/GCC/ARM_CM0" `
  -c "src/main.c" `
  -o "$out\main-after.o"
if ($LASTEXITCODE -ne 0) { throw "main.c still fails after the FreeRTOS repair" }
```

Expected: AC6 exits `0` and creates `main-after.o`. The command must not contain `-DconfigENABLE_MPU=0` or `-include stddef.h`.

- [ ] **Step 4: Compile every selected FreeRTOS C source**

Run:

```powershell
$cc = "C:\Keil_v5\ARM\ARMCLANG\bin\armclang.exe"
$out = "C:\Users\Joelin\AppData\Local\Temp\opencode\m0-build-repair"
$sdk = (Resolve-Path -LiteralPath "..\..\controller\documents\sdk").Path
$common = @(
    "--target=arm-arm-none-eabi",
    "-mcpu=cortex-m0plus",
    "-mfloat-abi=soft",
    "-std=c99",
    "-Os",
    "-g",
    "-fshort-enums",
    "-fshort-wchar",
    "-D__MSPM0G3507__",
    "-I$((Get-Location).Path)",
    "-I$((Get-Location).Path)\inc",
    "-I$((Get-Location).Path)\Debug",
    "-I$sdk\source",
    "-I$sdk\source\third_party\CMSIS\Core\Include",
    "-I$sdk\kernel\freertos\Source\include",
    "-I$sdk\kernel\freertos\Source\portable\GCC\ARM_CM0"
)
$sources = @(
    "$sdk\kernel\freertos\Source\tasks.c",
    "$sdk\kernel\freertos\Source\list.c",
    "$sdk\kernel\freertos\Source\queue.c",
    "$sdk\kernel\freertos\Source\portable\GCC\ARM_CM0\port.c",
    "$sdk\kernel\freertos\Source\portable\MemMang\heap_4.c"
)
$index = 0
foreach ($source in $sources) {
    $index++
    & $cc @common -c $source -o "$out\freertos-$index.o"
    if ($LASTEXITCODE -ne 0) { throw "FreeRTOS compile failed: $source" }
}
```

Expected: all five compilations exit `0`.

- [ ] **Step 5: Review the task diff**

Run:

```powershell
git diff -- inc/FreeRTOSConfig.h
git diff --check -- inc/FreeRTOSConfig.h
```

Expected: exactly one macro is added and `git diff --check` exits `0`.

---

### Task 2: Repair the Planner's Standard `NULL` Dependency

**Files:**
- Modify: `modules/Sens-Decision/src/behavior_planner.c:9-13`

**Interfaces:**
- Consumes: the C99 `<stddef.h>` definition of `NULL`.
- Produces: unchanged planner APIs `behavior_planner_init()` and `behavior_planner_update()` that compile independently under AC6.

- [ ] **Step 1: Reproduce the undeclared `NULL` error with AC6**

Run:

```powershell
$out = "C:\Users\Joelin\AppData\Local\Temp\opencode\m0-build-repair"
& "C:\Keil_v5\ARM\ARMCLANG\bin\armclang.exe" `
  --target=arm-arm-none-eabi `
  -mcpu=cortex-m0plus `
  -mfloat-abi=soft `
  -std=c99 `
  -Os `
  -g `
  -fshort-enums `
  -fshort-wchar `
  -D__MSPM0G3507__ `
  "-Imodules/Sens-Decision/inc" `
  -c "modules/Sens-Decision/src/behavior_planner.c" `
  -o "$out\behavior-planner-before.o"
if ($LASTEXITCODE -eq 0) { throw "Expected the pre-fix compile to fail" }
```

Expected: AC6 exits nonzero with six `use of undeclared identifier 'NULL'` diagnostics at the planner's argument checks.

- [ ] **Step 2: Include the standard definition of `NULL`**

Change the standard-header block to:

```c
#include <math.h>
#include <stddef.h>
```

Do not alter planner logic or move `NULL` into a project header.

- [ ] **Step 3: Recompile the planner without forced includes**

Run:

```powershell
$out = "C:\Users\Joelin\AppData\Local\Temp\opencode\m0-build-repair"
& "C:\Keil_v5\ARM\ARMCLANG\bin\armclang.exe" `
  --target=arm-arm-none-eabi `
  -mcpu=cortex-m0plus `
  -mfloat-abi=soft `
  -std=c99 `
  -Os `
  -g `
  -fshort-enums `
  -fshort-wchar `
  -D__MSPM0G3507__ `
  "-Imodules/Sens-Decision/inc" `
  -c "modules/Sens-Decision/src/behavior_planner.c" `
  -o "$out\behavior-planner-after.o"
if ($LASTEXITCODE -ne 0) { throw "behavior_planner.c still fails after adding stddef.h" }
```

Expected: AC6 exits `0` and creates `behavior-planner-after.o`. The command must not contain `-include stddef.h`.

- [ ] **Step 4: Run the complete host test suite**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File ".\tests\run_tests.ps1"
```

Expected: every test compiles and executes, ending with:

```text
Host tests: PASS
```

- [ ] **Step 5: Review the task diff**

Run:

```powershell
git diff -- modules/Sens-Decision/src/behavior_planner.c
git diff --check -- modules/Sens-Decision/src/behavior_planner.c
```

Expected: exactly one standard include is added and `git diff --check` exits `0`.

---

### Task 3: Correct the EIDE SysConfig Pre-Build Command

**Files:**
- Modify: `.eide/eide.yml:66-76`
- Reference: `.vscode/tasks.json:23-44`

**Interfaces:**
- Consumes: EIDE build-task fields `name`, `command`, and `stopBuildAfterFailed`; built-in variable `${ProjectRoot}`.
- Produces: one complete SysConfig command in `builder.params.options.beforeBuildTasks[0].command`.

- [ ] **Step 1: Record the malformed task shape**

Run:

```powershell
$text = Get-Content -Raw -LiteralPath ".eide\eide.yml"
if ($text -notmatch '(?m)^\s+args:\s*>-') { throw "Expected the unsupported args field" }
if ($text -notmatch '\$\{WorkspaceRoot\}') { throw "Expected the unsupported WorkspaceRoot variable" }
```

Expected: command exits `0`, proving both incompatible constructs are present before the edit.

- [ ] **Step 2: Put the executable and arguments in one supported command**

Replace the existing task with:

```yaml
          beforeBuildTasks:
            - name: Generate SysConfig
              command: >-
                C:/ti/sysconfig_1.26.2/sysconfig_cli.bat
                --product "${ProjectRoot}/../../controller/documents/sdk/.metadata/product.json"
                --device MSPM0G3507
                --package "LQFP-48(PT)"
                --script "${ProjectRoot}/NewProject1.syscfg"
                --output "${ProjectRoot}/Debug"
                --compiler keil
              stopBuildAfterFailed: true
```

Do not change `.vscode/tasks.json`; its array-based task already passes its arguments correctly.

- [ ] **Step 3: Verify the unsupported task constructs are gone**

Run:

```powershell
$text = Get-Content -Raw -LiteralPath ".eide\eide.yml"
if ($text -match '(?m)^\s+args:\s*>-') { throw "Unsupported args field remains" }
if ($text -match '\$\{WorkspaceRoot\}') { throw "Unsupported WorkspaceRoot remains" }
if ($text -notmatch '\$\{ProjectRoot\}') { throw "ProjectRoot was not configured" }
if ($text -notmatch 'stopBuildAfterFailed:\s*true') { throw "Pre-build failure gate was removed" }
```

Expected: command exits `0`.

- [ ] **Step 4: Review the task diff**

Run:

```powershell
git diff -- .eide/eide.yml
git diff --check -- .eide/eide.yml
```

Expected: only the SysConfig pre-build task shape changes and `git diff --check` exits `0`.

---

### Task 4: Install and Validate SysConfig 1.26.2.4477

**Files:**
- External install: `C:/ti/sysconfig_1.26.2/`
- Download outside workspace: `C:/Users/Joelin/AppData/Local/Temp/opencode/sysconfig-1.26.2_4477-setup.exe`
- Reference: `../../controller/documents/sdk/.metadata/product.json:1-24`

**Interfaces:**
- Consumes: TI's signed Windows installer and 500 MB free disk space.
- Produces: `C:/ti/sysconfig_1.26.2/sysconfig_cli.bat` and SysConfig version `1.26.2.4477`.

- [ ] **Step 1: Confirm the exact SDK/tool requirement and current absence**

Run:

```powershell
$product = Get-Content -Raw -LiteralPath "..\..\controller\documents\sdk\.metadata\product.json" | ConvertFrom-Json
if ($product.version -ne "2.10.00.04") { throw "Unexpected SDK version: $($product.version)" }
if ([version]$product.minToolVersion -gt [version]"1.26.2") { throw "SysConfig 1.26.2 is too old" }
Test-Path -LiteralPath "C:\ti\sysconfig_1.26.2\sysconfig_cli.bat"
```

Expected: SDK version check passes and `Test-Path` initially returns `False`.

- [ ] **Step 2: Download the official Windows installer**

Verify the approved temporary parent exists, then download:

```powershell
$temp = "C:\Users\Joelin\AppData\Local\Temp\opencode"
if (-not (Test-Path -LiteralPath $temp)) { throw "Approved temp parent is missing" }
$installer = Join-Path $temp "sysconfig-1.26.2_4477-setup.exe"
Invoke-WebRequest `
  -Uri "https://dr-download.ti.com/software-development/ide-configuration-compiler-or-debugger/MD-nsUM6f7Vvb/1.26.2.4477/sysconfig-1.26.2_4477-setup.exe" `
  -OutFile $installer
```

Expected: download completes and the file size is approximately 173 MB.

- [ ] **Step 3: Verify the installer checksum and signature**

Run:

```powershell
$installer = "C:\Users\Joelin\AppData\Local\Temp\opencode\sysconfig-1.26.2_4477-setup.exe"
$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $installer).Hash.ToLowerInvariant()
if ($hash -ne "b40e31b01987c68c420ea90bb2095885cd97ee9ab23f2447fa37b4a33933fd31") {
    throw "SysConfig installer checksum mismatch: $hash"
}
$signature = Get-AuthenticodeSignature -FilePath $installer
if ($signature.Status -ne "Valid") { throw "Invalid installer signature: $($signature.Status)" }
```

Expected: checksum matches TI's published value and the Authenticode status is `Valid`.

- [ ] **Step 4: Inspect installer-supported command-line options**

Run:

```powershell
& "C:\Users\Joelin\AppData\Local\Temp\opencode\sysconfig-1.26.2_4477-setup.exe" --help
```

Expected: installer help is displayed. Confirm it supports `--prefix` and unattended mode before using those options. If it does not, run the installer interactively and select `C:\ti\sysconfig_1.26.2`; do not guess unsupported flags.

- [ ] **Step 5: Install to the configured path**

If Step 4 confirms the BitRock-style options, run:

```powershell
$parent = "C:\ti"
if (-not (Test-Path -LiteralPath "C:\")) { throw "C drive is unavailable" }
if (-not (Test-Path -LiteralPath $parent)) {
    New-Item -ItemType Directory -Path $parent | Out-Null
}
& "C:\Users\Joelin\AppData\Local\Temp\opencode\sysconfig-1.26.2_4477-setup.exe" `
  --mode unattended `
  --prefix "C:\ti\sysconfig_1.26.2"
if ($LASTEXITCODE -ne 0) { throw "SysConfig installer failed: $LASTEXITCODE" }
```

Expected: installer exits `0`. If elevation is required, stop and request the user to approve the Windows elevation prompt rather than changing the install root.

- [ ] **Step 6: Validate the installed CLI and release**

Run:

```powershell
$cli = "C:\ti\sysconfig_1.26.2\sysconfig_cli.bat"
if (-not (Test-Path -LiteralPath $cli)) { throw "SysConfig CLI was not installed at the configured path" }
& $cli --help
if ($LASTEXITCODE -ne 0) { throw "SysConfig CLI validation failed" }
```

Expected: `sysconfig_cli.bat` exists, starts successfully, and reports SysConfig 1.26.2/4477 in its help or banner. If the banner omits the build number, confirm the installation's release notes identify `1.26.2_4477`.

---

### Task 5: Regenerate and Audit the DriverLib Configuration

**Files:**
- Generated: `Debug/ti_msp_dl_config.c`
- Generated: `Debug/ti_msp_dl_config.h`
- Generated if emitted: `Debug/Event.dot`
- Reference only: `NewProject1.syscfg:14-203`
- Reference only: `WIRING_AND_SYSCONFIG.md:1-117`

**Interfaces:**
- Consumes: `NewProject1.syscfg`, SDK 2.10.00.04 product metadata, SysConfig 1.26.2.4477.
- Produces: generated DriverLib symbols including `ICM42688_TIMER_INST == TIMG12` and unchanged documented hardware mappings.

- [ ] **Step 1: Capture the generated-file baseline without editing it**

Run:

```powershell
$baseline = "C:\Users\Joelin\AppData\Local\Temp\opencode\m0-sysconfig-baseline"
if (-not (Test-Path -LiteralPath "C:\Users\Joelin\AppData\Local\Temp\opencode")) {
    throw "Approved temp parent is missing"
}
if (-not (Test-Path -LiteralPath $baseline)) {
    New-Item -ItemType Directory -Path $baseline | Out-Null
}
Copy-Item -LiteralPath ".\Debug\ti_msp_dl_config.c" -Destination "$baseline\ti_msp_dl_config.c"
Copy-Item -LiteralPath ".\Debug\ti_msp_dl_config.h" -Destination "$baseline\ti_msp_dl_config.h"
if (Test-Path -LiteralPath ".\Debug\Event.dot") {
    Copy-Item -LiteralPath ".\Debug\Event.dot" -Destination "$baseline\Event.dot"
}
```

Expected: baseline copies exist only in the approved temp directory.

- [ ] **Step 2: Generate from the authoritative `.syscfg` file**

Run:

```powershell
& "C:\ti\sysconfig_1.26.2\sysconfig_cli.bat" `
  --product "..\..\controller\documents\sdk\.metadata\product.json" `
  --device MSPM0G3507 `
  --package "LQFP-48(PT)" `
  --script ".\NewProject1.syscfg" `
  --output ".\Debug" `
  --compiler keil
if ($LASTEXITCODE -ne 0) { throw "SysConfig generation failed" }
```

Expected: SysConfig exits `0` with no resource conflict.

- [ ] **Step 3: Verify the dedicated timer and fixed peripheral assignments**

Run:

```powershell
$header = Get-Content -Raw -LiteralPath ".\Debug\ti_msp_dl_config.h"
$required = @(
    '#define ICM42688_TIMER_INST\s+\(TIMG12\)',
    '#define CONTROL_TIMER_INST\s+\(TIMG0\)',
    '#define CONTROL_TIMER_INST_LOAD_VALUE\s+\(63999U\)',
    '#define MOTOR_PWM_A_INST\s+TIMA0',
    '#define MOTOR_PWM_B_INST\s+TIMA1',
    '#define I2C0_INST\s+I2C0',
    '#define SPI1_INST\s+SPI1',
    '#define DMA_SPI1_RX_CHAN_ID\s+\(2\)',
    '#define DMA_SPI1_TX_CHAN_ID\s+\(3\)',
    '#define ICM42688_CS_PIN\s+\(DL_GPIO_PIN_20\)',
    '#define MOTOR_DIR_M1_IN1_PIN\s+\(DL_GPIO_PIN_6\)',
    '#define MOTOR_DIR_M4_IN2_PIN\s+\(DL_GPIO_PIN_24\)',
    '#define ENCODER_ENC1_A_PIN\s+\(DL_GPIO_PIN_12\)',
    '#define ENCODER_ENC4_B_PIN\s+\(DL_GPIO_PIN_18\)'
)
foreach ($pattern in $required) {
    if ($header -notmatch $pattern) { throw "Generated invariant missing: $pattern" }
}
```

Expected: every invariant matches. If a generated macro uses a different stable prefix than shown for encoder pins, inspect the actual generated identifier and compare its port/pin/IOMUX value against `NewProject1.syscfg`; do not weaken or delete the hardware assertion.

- [ ] **Step 4: Audit all documented generated mappings**

Compare `Debug/ti_msp_dl_config.h` against `NewProject1.syscfg` and `WIRING_AND_SYSCONFIG.md` for:

```text
CONTROL_TIMER: TIMG0, 63999 load
ICM42688_TIMER: TIMG12, 1 MHz, maximum period, no interrupt
SPI1: PA16 POCI, PA18 PICO, PA17 SCLK, DMA channels 2/3
ICM42688 CS: PB20
I2C0: PA0 SDA, PA1 SCL, 100 kHz
UART0: PA10/PA11, UART1: PA8/PA9, UART3: PB2/PB3, all 115200
PWM: PA21/TIMA0 CCP0, PA22/TIMA0 CCP1, PA24/TIMA1 CCP1, PA25/TIMA0 CCP3
Motor direction: PB6, PB7, PB8, PA7, PA15, PB9, PB19, PB24
Encoders: PA12, PA13, PA2, PA26, PA27, PA28, PA31, PB18
```

Expected: every mapping is unchanged except the newly generated dedicated timer. If any mismatch exists, stop here and present the diff to the user.

- [ ] **Step 5: Review generated differences**

Run:

```powershell
git diff -- Debug/ti_msp_dl_config.c Debug/ti_msp_dl_config.h Debug/Event.dot
git diff --stat -- Debug/ti_msp_dl_config.c Debug/ti_msp_dl_config.h Debug/Event.dot
git diff --check -- Debug/ti_msp_dl_config.c Debug/ti_msp_dl_config.h Debug/Event.dot
```

Expected: differences are attributable to SysConfig 1.26.2 generation and the `ICM42688_TIMER` addition. `git diff --check` exits `0`. Stop for user review if the diff is unexpectedly broad or changes existing mappings.

- [ ] **Step 6: Re-run host tests after generation**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File ".\tests\run_tests.ps1"
```

Expected: ends with `Host tests: PASS`.

---

### Task 6: Generate EIDE Parameters and Run the Complete AC6 Rebuild

**Files:**
- Generated: `build/Debug/builder.params`
- Generated: `build/Debug/NewProject1.axf`
- Generated: `build/Debug/NewProject1.map`
- Generated: `build/Debug/NewProject1.hex`
- Generated: `build/Debug/NewProject1.s19`
- Generated: `build/Debug/NewProject1.map.view`
- Generated: `build/Debug/unify_builder.log`
- Generated: `build/Debug/compiler.log`

**Interfaces:**
- Consumes: active EIDE target `Debug`, `.eide/eide.yml`, installed AC6 and SysConfig tools, all production sources.
- Produces: a successful full rebuild and linked target image under `build/Debug`.

- [ ] **Step 1: Generate `builder.params` through EIDE**

Open the workspace:

```powershell
code -r ".\NewProject1.code-workspace"
```

In VS Code, verify EIDE's active target is `Debug`, then invoke:

```text
F1 -> EIDE: Generate builder.params
```

Expected file:

```text
build/Debug/builder.params
```

VS Code 1.104.2 has no supported CLI option for invoking arbitrary extension commands, so this one UI command is the supported handoff. Do not construct or edit `builder.params` manually.

- [ ] **Step 2: Validate the generated build snapshot**

Run:

```powershell
$paramsPath = ".\build\Debug\builder.params"
if (-not (Test-Path -LiteralPath $paramsPath)) { throw "EIDE did not generate builder.params" }
$params = Get-Content -Raw -LiteralPath $paramsPath | ConvertFrom-Json
if ($params.name -ne "NewProject1") { throw "Unexpected output name" }
if ($params.target -ne "Debug") { throw "Unexpected EIDE target" }
if ($params.toolchain -ne "AC6") { throw "Unexpected EIDE toolchain" }
if ($params.toolchainLocation -notmatch 'Keil_v5[\\/]ARM[\\/]ARMCLANG') { throw "Unexpected AC6 path" }
$prebuild = [string]$params.options.beforeBuildTasks[0].command
$tokens = @(
    'C:/ti/sysconfig_1.26.2/sysconfig_cli.bat',
    '--product',
    'controller/documents/sdk/.metadata/product.json',
    '--device MSPM0G3507',
    '--package "LQFP-48(PT)"',
    '--script',
    'NewProject1.syscfg',
    '--output',
    '/Debug',
    '--compiler keil'
)
foreach ($token in $tokens) {
    if ($prebuild -notlike "*$token*") { throw "Incomplete pre-build command: $token" }
}
if (-not $params.options.beforeBuildTasks[0].stopBuildAfterFailed) {
    throw "SysConfig pre-build failure gate is disabled"
}
```

Expected: command exits `0`; the pre-build task contains the executable and all arguments, with no unresolved `${ProjectRoot}`.

- [ ] **Step 3: Run EIDE's bundled builder in rebuild mode**

Run:

```powershell
$builder = "C:\Users\Joelin\.vscode\extensions\cl.eide-3.27.2\res\tools\win32\unify_builder\unify_builder.exe"
$paramsPath = (Resolve-Path -LiteralPath ".\build\Debug\builder.params").Path
& $builder --params-file $paramsPath --rebuild --no-color
if ($LASTEXITCODE -ne 0) { throw "EIDE rebuild failed: $LASTEXITCODE" }
```

Expected: pre-build SysConfig generation runs first, all C and assembly files compile, linking succeeds, and the command exits `0`.

- [ ] **Step 4: Verify target artifacts**

Run:

```powershell
$required = @(
    ".\build\Debug\NewProject1.axf",
    ".\build\Debug\NewProject1.map",
    ".\build\Debug\unify_builder.log",
    ".\build\Debug\compiler.log"
)
foreach ($path in $required) {
    if (-not (Test-Path -LiteralPath $path)) { throw "Missing build artifact: $path" }
}
```

Expected: all required files exist. `.hex`, `.s19`, and `.map.view` may also exist; `.bin` is intentionally excluded by EIDE configuration.

- [ ] **Step 5: Verify linked FreeRTOS objects and symbols**

Run:

```powershell
$map = Get-Content -Raw -LiteralPath ".\build\Debug\NewProject1.map"
$required = @('portasm.o', 'PendSV_Handler', 'SVC_Handler', 'vStartFirstTask')
foreach ($token in $required) {
    if ($map -notmatch [regex]::Escape($token)) { throw "Missing linked FreeRTOS token: $token" }
}
```

Expected: all four tokens appear in the map.

- [ ] **Step 6: Verify excluded sources are absent**

Run:

```powershell
$map = Get-Content -Raw -LiteralPath ".\build\Debug\NewProject1.map"
$forbidden = @('example_usage.o', 'OLED', 'test_control_app.o', 'test_motion_control.o')
foreach ($token in $forbidden) {
    if ($map -match [regex]::Escape($token)) { throw "Forbidden object linked: $token" }
}
```

Expected: no forbidden token is present.

- [ ] **Step 7: Record Flash and SRAM usage**

Run:

```powershell
& "C:\Keil_v5\ARM\ARMCLANG\bin\fromelf.exe" `
  --text `
  --info=sizes `
  ".\build\Debug\NewProject1.axf"
if ($LASTEXITCODE -ne 0) { throw "fromelf size inspection failed" }
```

Expected: Total ROM is below 131072 bytes and Total RW size is below 32768 bytes. Record the exact output in the final report.

---

### Task 7: Run Final Acceptance Verification

**Files:**
- Verify: `inc/FreeRTOSConfig.h`
- Verify: `modules/Sens-Decision/src/behavior_planner.c`
- Verify: `.eide/eide.yml`
- Verify: `Debug/ti_msp_dl_config.c`
- Verify: `Debug/ti_msp_dl_config.h`
- Verify: `build/Debug/NewProject1.axf`
- Verify: `build/Debug/NewProject1.map`

**Interfaces:**
- Consumes: all completed repair tasks and their artifacts.
- Produces: fresh evidence for the build-repair acceptance criteria.

- [ ] **Step 1: Run host tests from a clean output directory**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File ".\tests\run_tests.ps1"
```

Expected: all seven current host test programs compile and run, ending with `Host tests: PASS`.

- [ ] **Step 2: Run a second full EIDE rebuild**

Run:

```powershell
$builder = "C:\Users\Joelin\.vscode\extensions\cl.eide-3.27.2\res\tools\win32\unify_builder\unify_builder.exe"
$paramsPath = (Resolve-Path -LiteralPath ".\build\Debug\builder.params").Path
& $builder --params-file $paramsPath --rebuild --no-color
if ($LASTEXITCODE -ne 0) { throw "Final EIDE rebuild failed: $LASTEXITCODE" }
```

Expected: exits `0` and recreates `NewProject1.axf` and `NewProject1.map` with the SysConfig pre-build task included.

- [ ] **Step 3: Re-run image acceptance checks**

Run:

```powershell
$artifactPaths = @(
    ".\build\Debug\NewProject1.axf",
    ".\build\Debug\NewProject1.map",
    ".\build\Debug\unify_builder.log",
    ".\build\Debug\compiler.log"
)
foreach ($path in $artifactPaths) {
    if (-not (Test-Path -LiteralPath $path)) { throw "Missing final artifact: $path" }
}
$map = Get-Content -Raw -LiteralPath ".\build\Debug\NewProject1.map"
foreach ($token in @('portasm.o', 'PendSV_Handler', 'SVC_Handler', 'vStartFirstTask')) {
    if ($map -notmatch [regex]::Escape($token)) { throw "Missing final FreeRTOS token: $token" }
}
foreach ($token in @('example_usage.o', 'OLED', 'test_control_app.o', 'test_motion_control.o')) {
    if ($map -match [regex]::Escape($token)) { throw "Forbidden final object: $token" }
}
& "C:\Keil_v5\ARM\ARMCLANG\bin\fromelf.exe" `
  --text `
  --info=sizes `
  ".\build\Debug\NewProject1.axf"
if ($LASTEXITCODE -ne 0) { throw "Final image-size inspection failed" }
```

Expected: artifacts exist, FreeRTOS symbols are linked, forbidden sources are absent, and memory usage remains within 128 KiB Flash and 32 KiB SRAM.

- [ ] **Step 4: Check only repair-owned repository differences**

Run:

```powershell
git diff --check -- `
  inc/FreeRTOSConfig.h `
  modules/Sens-Decision/src/behavior_planner.c `
  .eide/eide.yml `
  Debug/ti_msp_dl_config.c `
  Debug/ti_msp_dl_config.h `
  Debug/Event.dot `
  docs/superpowers/specs/2026-07-18-firmware-build-repair-design.md `
  docs/superpowers/plans/2026-07-18-firmware-build-repair.md
git diff --stat -- `
  inc/FreeRTOSConfig.h `
  modules/Sens-Decision/src/behavior_planner.c `
  .eide/eide.yml `
  Debug/ti_msp_dl_config.c `
  Debug/ti_msp_dl_config.h `
  Debug/Event.dot
git status --short
```

Expected: `git diff --check` exits `0`; the repair diff contains only the approved source/build/generated changes and the planning documents. Existing unrelated status entries remain untouched.

- [ ] **Step 5: Report exact acceptance evidence**

Report:

```text
SysConfig release and installed CLI path
Host test result and number of test executables
Full EIDE/unify_builder exit status
AXF and map paths
Total ROM bytes and limit
Total RW bytes and limit
Presence of portasm.o, PendSV_Handler, SVC_Handler, vStartFirstTask
Absence of OLED/example/test objects
Generated pin/peripheral audit result
Any residual hardware-only verification gap
```

Do not claim runtime hardware correctness; flashing and on-device testing remain outside this build repair.

## Plan Self-Review

- Spec coverage: all four confirmed blockers map to Tasks 1 through 4; SysConfig generation invariants map to Task 5; complete EIDE/AC6 build, linked symbols, exclusions, and memory limits map to Tasks 6 and 7.
- Placeholder scan: all edits, commands, paths, expected failures, expected successes, stop conditions, official SysConfig download URL, version, and checksum are explicit.
- Type and name consistency: `configENABLE_MPU` is an integer macro set to `0`; the planner uses C99 `<stddef.h>`; EIDE uses `${ProjectRoot}`; the generated target is `Debug`; output names are `NewProject1.axf` and `NewProject1.map`.
- Scope check: no control behavior, pin assignment, scheduler policy, build-system abstraction, flashing, or unrelated cleanup is included.
- External interaction: SysConfig installation may require a Windows elevation prompt, and generating `builder.params` requires one VS Code command because the installed `code` CLI cannot invoke arbitrary extension commands.
