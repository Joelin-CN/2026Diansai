# Firmware Task 1 Report: Repair the FreeRTOS Port Configuration

## Status

DONE

No commits were created.

## Implementation

Added the required non-MPU FreeRTOS port setting to the scheduler configuration block in `2025e/m0_controller/inc/FreeRTOSConfig.h`:

```c
/* Scheduler and clock configuration. CPUCLK_FREQ is currently 32 MHz. */
#define configENABLE_MPU                       0
#define configUSE_PREEMPTION                    1
```

This is the only project source change. No other macro was changed. The setting matches both the Cortex-M0+ port contract in `controller/documents/sdk/kernel/freertos/Source/portable/GCC/ARM_CM0/portmacro.h:48-50` and the MSPM0G3507 SDK configuration reference in `controller/documents/sdk/kernel/freertos/builds/LP_MSPM0G3507/release/FreeRTOSConfig.h:76-78`.

## RED: Pre-Fix Reproduction

Run from `E:\B306\2026\电赛\2025e\m0_controller` before editing:

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

Result: AC6 exited nonzero as expected. Complete emitted output:

```text
In file included from src/main.c:9:
In file included from ../../controller/documents/sdk/kernel/freertos/Source/include\FreeRTOS.h:108:
In file included from ../../controller/documents/sdk/kernel/freertos/Source/include/portable.h:53:
../../controller/documents/sdk/kernel/freertos/Source/portable/GCC/ARM_CM0\portmacro.h:49:6: error: configENABLE_MPU must be defined in FreeRTOSConfig.h.  Set configENABLE_MPU to 1 to enable the MPU or 0 to disable the MPU.
   49 |     #error configENABLE_MPU must be defined in FreeRTOSConfig.h.  Set configENABLE_MPU to 1 to enable the MPU or 0 to disable the MPU.
      |      ^
1 error generated.
```

The failure is reproducible and directly identifies the root cause: the selected Cortex-M0+ FreeRTOS port requires `configENABLE_MPU`, but the project configuration did not define it.

## GREEN: main.c Compile

Run from `E:\B306\2026\电赛\2025e\m0_controller` after the one-line configuration repair:

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

Result: exit `0`; complete emitted output was empty. `main-after.o` was created and is non-empty at 12,972 bytes. The command contains neither `-DconfigENABLE_MPU=0` nor `-include stddef.h`.

## GREEN: Selected FreeRTOS Sources

Run from `E:\B306\2026\电赛\2025e\m0_controller`:

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

Result: all five compiler invocations exited `0`; complete emitted output was empty. The diagnostic objects were created and are non-empty:

```text
freertos-1.o: 62876 bytes
freertos-2.o: 5576 bytes
freertos-3.o: 32592 bytes
freertos-4.o: 11216 bytes
freertos-5.o: 14276 bytes
```

## Diff Review

Commands:

```powershell
git diff -- inc/FreeRTOSConfig.h
git diff --check -- inc/FreeRTOSConfig.h
```

`git diff -- inc/FreeRTOSConfig.h` exited `0` with:

```text
warning: in the working copy of '2025e/m0_controller/inc/FreeRTOSConfig.h', LF will be replaced by CRLF the next time Git touches it
diff --git a/2025e/m0_controller/inc/FreeRTOSConfig.h b/2025e/m0_controller/inc/FreeRTOSConfig.h
index 7df147d..27c3d72 100644
--- a/2025e/m0_controller/inc/FreeRTOSConfig.h
+++ b/2025e/m0_controller/inc/FreeRTOSConfig.h
@@ -4,6 +4,7 @@
 #include <stdint.h>
 
 /* Scheduler and clock configuration. CPUCLK_FREQ is currently 32 MHz. */
+#define configENABLE_MPU                       0
 #define configUSE_PREEMPTION                    1
 #define configUSE_PORT_OPTIMISED_TASK_SELECTION 0
 #define configUSE_TIME_SLICING                   0
```

`git diff --check -- inc/FreeRTOSConfig.h` exited `0`. Its complete emitted output was:

```text
warning: in the working copy of '2025e/m0_controller/inc/FreeRTOSConfig.h', LF will be replaced by CRLF the next time Git touches it
```

The line-ending message is a Git working-copy warning, not a `diff --check` whitespace error.

## Files Changed

- `E:\B306\2026\电赛\2025e\m0_controller\inc\FreeRTOSConfig.h`: added exactly one macro.
- `E:\B306\2026\电赛\.superpowers\sdd\firmware-task-1-report.md`: added this required task report outside the project checkout.
- `C:\Users\Joelin\AppData\Local\Temp\opencode\m0-build-repair\main-after.o`: external diagnostic object.
- `C:\Users\Joelin\AppData\Local\Temp\opencode\m0-build-repair\freertos-1.o` through `freertos-5.o`: external diagnostic objects.

No unrelated dirty file, generated `Debug` file, test/build binary, planning document, picture, or SDK submodule state was modified or reverted. The task brief and this required report are not treated as project implementation files.

## Self-Review

- Confirmed RED failed for the expected missing `configENABLE_MPU` contract rather than an unrelated compiler or include-path issue.
- Confirmed the SDK's MSPM0G3507 reference config uses `configENABLE_MPU 0` and the Cortex-M0+ port accepts `0` or `1`.
- Confirmed implementation is the smallest approved change: one definition, in the specified block, with no other macro edits.
- Confirmed GREEN uses no command-line definition or forced include workaround.
- Confirmed `main.c` and all five selected FreeRTOS sources compile successfully.
- Confirmed all six GREEN objects exist outside the workspace and are non-empty.
- Confirmed the file-scoped diff adds exactly one line and `git diff --check` exits `0`.
- Confirmed no commit was created.

## Concerns

No blocking concerns. Git reports that `inc/FreeRTOSConfig.h` will be converted from LF to CRLF if Git later rewrites it; the current scoped diff has no whitespace errors and contains only the required macro addition.
