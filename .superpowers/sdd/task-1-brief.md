### Task 1: Establish the Migrated Source Baseline and Host Test Runner

**Files:**
- Create: `modules/ICM42688/inc/ahrs_hal.h`
- Create: `modules/ICM42688/inc/icm42688_hal.h`
- Create: `modules/ICM42688/inc/icm42688_mspm0.h`
- Create: `modules/ICM42688/src/ahrs_hal.c`
- Create: `modules/ICM42688/src/icm42688_hal.c`
- Create: `modules/ICM42688/src/icm42688_mspm0.c`
- Create: `modules/MCP23017/inc/mcp23017.h`
- Create: `modules/MCP23017/src/mcp23017.c`
- Create: `modules/Motion Control/inc/*.h`
- Create: `modules/Motion Control/src/motion_control.c`
- Create: `modules/Motion Control/src/motion_feedback.c`
- Create: `modules/Motion Control/src/motion_feedforward.c`
- Create: `modules/Motion Control/src/motion_kinematics.c`
- Create: `modules/Sens-Decision/inc/*.h`
- Create: `modules/Sens-Decision/src/*.c`
- Create: `tests/run_tests.ps1`

**Interfaces:**
- Consumes: source files under `E:/B306/2026/电赛/modules`.
- Produces: a target-local, self-contained copy of the four included modules and one host-test command.

- [ ] **Step 1: Add only production module sources with `apply_patch`**

Read each source file listed in this task from `E:/B306/2026/电赛/modules`, then add the same content under the target-local `modules/` path with `apply_patch`. Preserve the source text exactly at this baseline step. Do not add `example_usage.c`, `temp/`, OLED, `motor/`, or `servo/`.

Expected: only `inc/` and production `src/` files exist under the four target modules, and a content diff against the source paths is empty before target-specific edits.

- [ ] **Step 2: Create the test runner skeleton**

Create `tests/run_tests.ps1` with strict process handling:

```powershell
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$build = Join-Path $PSScriptRoot "build"

if (Test-Path -LiteralPath $build) {
    Remove-Item -Recurse -Force -LiteralPath $build
}
New-Item -ItemType Directory -Path $build | Out-Null

function Invoke-TestBuild {
    param(
        [string]$Name,
        [string[]]$Arguments
    )

    $exe = Join-Path $build "$Name.exe"
    & gcc @Arguments -o $exe
    if ($LASTEXITCODE -ne 0) { throw "$Name compile failed" }
    & $exe
    if ($LASTEXITCODE -ne 0) { throw "$Name failed" }
}

Write-Host "Host tests: PASS"
```

- [ ] **Step 3: Run portable module baselines**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File "E:\B306\2026\电赛\modules\ICM42688\temp\tests\run_tests.ps1"
powershell -ExecutionPolicy Bypass -File "E:\B306\2026\电赛\modules\Sens-Decision\temp\build_and_test.ps1"
gcc -std=c99 -Wall -Wextra -Werror -pedantic -fsyntax-only -I".\modules\Motion Control\inc" ".\modules\Motion Control\src\motion_kinematics.c" ".\modules\Motion Control\src\motion_feedforward.c" ".\modules\Motion Control\src\motion_feedback.c" ".\modules\Motion Control\src\motion_control.c"
```

Expected: ICM reports `ICM42688 host tests: PASS`, Sens-Decision reports `passed=65, failed=0`, and GCC syntax checking exits 0.

- [ ] **Step 4: Record a review checkpoint**

Review `git diff -- modules tests/run_tests.ps1` and confirm copied sources match the source modules exactly before target-specific changes begin.
