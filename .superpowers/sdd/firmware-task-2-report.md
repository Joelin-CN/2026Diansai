# Firmware Task 2 Report: Repair the Planner's Standard `NULL` Dependency

## Status

PASS. The planner now includes the C99 standard definition of `NULL` and compiles independently under AC6. No commit was created.

## RED Evidence

Command run from `E:\B306\2026\电赛\2025e\m0_controller`:

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

Result: AC6 exited nonzero and emitted exactly six expected diagnostics:

```text
modules/Sens-Decision/src/behavior_planner.c:32:20: error: use of undeclared identifier 'NULL'
modules/Sens-Decision/src/behavior_planner.c:51:20: error: use of undeclared identifier 'NULL'
modules/Sens-Decision/src/behavior_planner.c:51:37: error: use of undeclared identifier 'NULL'
modules/Sens-Decision/src/behavior_planner.c:51:55: error: use of undeclared identifier 'NULL'
modules/Sens-Decision/src/behavior_planner.c:52:27: error: use of undeclared identifier 'NULL'
modules/Sens-Decision/src/behavior_planner.c:52:56: error: use of undeclared identifier 'NULL'
6 errors generated.
```

Root cause: `behavior_planner.c` uses `NULL` in six argument checks but its standard-header block included only `<math.h>`. The translation unit therefore did not consume the C99 `<stddef.h>` definition of `NULL` and depended on an incidental/transitive definition unavailable in the standalone AC6 compile.

## Minimal Fix

Added exactly one standard include beside `<math.h>` in `modules/Sens-Decision/src/behavior_planner.c`:

```c
#include <math.h>
#include <stddef.h>
```

No planner logic, project headers, APIs, or build flags were changed. In particular, no `-include stddef.h` workaround was used.

## GREEN Evidence

Command run from `E:\B306\2026\电赛\2025e\m0_controller`:

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

Result: AC6 exited `0` with no diagnostics. `C:\Users\Joelin\AppData\Local\Temp\opencode\m0-build-repair\behavior-planner-after.o` exists. The compile command contains no forced include.

## Host Test Suite

Command:

```powershell
powershell -ExecutionPolicy Bypass -File ".\tests\run_tests.ps1"
```

Result: exit `0`; all existing host tests compiled and executed, ending with:

```text
Host tests: PASS
```

The expected unrelated MCP23017 baseline warnings remain and were not fixed:

```text
tests/test_mcp23017.c:107:14: warning: unused variable 'mask' [-Wunused-variable]
tests/test_mcp23017.c:118:14: warning: unused variable 'mask' [-Wunused-variable]
```

## Diff Review

Commands:

```powershell
git diff -- modules/Sens-Decision/src/behavior_planner.c
git diff --check -- modules/Sens-Decision/src/behavior_planner.c
```

Result: the scoped source diff adds exactly `#include <stddef.h>` beside `<math.h>`. `git diff --check` exited `0`. Git emitted an informational warning that the working-copy LF line endings will be replaced by CRLF the next time Git touches the file; no whitespace error was reported.

## Files Changed

- `E:\B306\2026\电赛\2025e\m0_controller\modules\Sens-Decision\src\behavior_planner.c`: added `#include <stddef.h>`.
- `E:\B306\2026\电赛\.superpowers\sdd\firmware-task-2-report.md`: added this required report.
- External diagnostic artifact: `C:\Users\Joelin\AppData\Local\Temp\opencode\m0-build-repair\behavior-planner-after.o`.

No unrelated dirty source/configuration files, Task 1's `inc/FreeRTOSConfig.h`, planning documents, SDK state, `.vscode/settings.json`, `pictures/`, or generated Debug files were edited or reverted. The required host-suite command rebuilds its existing `tests/build` executables; no manual edits or cleanup were performed on those generated test binaries.

## Self-Review

- The implementation is the smallest root-cause fix: one direct standard dependency was added at the point of use.
- Planner behavior and public APIs are unchanged.
- RED demonstrated exactly the six required failures before production code changed.
- GREEN used the same standalone AC6 invocation without a forced include.
- The complete host suite passed after the fix.
- The source-only diff and whitespace check match the task brief.
- No commit was created.

## Concerns

- The two unrelated MCP23017 unused-variable warnings remain as the known host-test baseline.
- Git reports an existing LF-to-CRLF working-copy conversion warning for `behavior_planner.c`; the scoped diff itself contains no unintended line-ending rewrite or whitespace error.
