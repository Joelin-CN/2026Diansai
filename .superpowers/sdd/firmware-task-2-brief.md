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

