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

