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
