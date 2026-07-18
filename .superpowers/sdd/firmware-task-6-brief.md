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

