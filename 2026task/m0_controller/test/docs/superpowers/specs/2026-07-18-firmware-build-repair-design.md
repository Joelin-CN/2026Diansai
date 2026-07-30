# MSPM0 Firmware Build Repair Design

## 1. Goal

Restore a reproducible EIDE and Arm Compiler 6 firmware build for the
MSPM0G3507 controller project.

The repair is limited to the four confirmed build blockers:

1. TI SysConfig 1.26.2 is not installed at the path required by the project.
2. `inc/FreeRTOSConfig.h` does not define the `configENABLE_MPU` setting
   required by the selected FreeRTOS Cortex-M0+ port.
3. `modules/Sens-Decision/src/behavior_planner.c` uses `NULL` without including
   a standard header that defines it.
4. The EIDE SysConfig pre-build task stores its command-line arguments in an
   unsupported `args` field and uses the unsupported `${WorkspaceRoot}`
   variable, so EIDE would invoke SysConfig without the required arguments.

The repair must not change application behavior, control algorithms, pin
assignments, peripheral selection, or the existing build architecture.

## 2. Selected Approach

Use the existing fixed-version build configuration with the smallest possible
source changes.

- Repository SDK: `controller/documents/sdk`, version `2.10.00.04`
- SysConfig: version `1.26.2`, installed at `C:/ti/sysconfig_1.26.2`
- Compiler: Arm Compiler for Embedded 6.21
- EIDE: the installed 3.27.2 extension
- Target: MSPM0G3507, Cortex-M0+, LQFP-48

The repair will not introduce environment-variable expansion, a new wrapper
build script, a GCC target build, or a fallback that silently skips SysConfig.
The existing mandatory SysConfig pre-build step remains authoritative.

## 3. Production Changes

### 3.1 FreeRTOS Port Configuration

Modify `inc/FreeRTOSConfig.h` to define:

```c
#define configENABLE_MPU 0
```

This is required by
`kernel/freertos/Source/portable/GCC/ARM_CM0/portmacro.h`. The value matches the
repository SDK's MSPM0G3507 reference configuration. The project does not use
FreeRTOS MPU wrappers, and the Cortex-M0+ target is intended to run the
non-MPU port configuration.

No other scheduler, interrupt-priority, heap, hook, or task setting is changed.

### 3.2 Standard `NULL` Definition

Modify `modules/Sens-Decision/src/behavior_planner.c` to include
`<stddef.h>` alongside its existing standard headers.

This supplies the standard definition of `NULL` used by the planner's argument
validation. No planner state transition, configuration value, or output logic
is changed.

### 3.3 EIDE SysConfig Pre-Build Task

Modify `.eide/eide.yml` so the SysConfig executable and all arguments are part
of the task's `command` string. Replace `${WorkspaceRoot}` with EIDE's supported
`${ProjectRoot}` variable.

The resulting task must expand to the same command documented in Section 5.4
and must retain `stopBuildAfterFailed: true`. No other EIDE target, source,
include, compiler, linker, uploader, or memory-layout setting is changed.

### 3.4 SysConfig-Generated Files

Install or locate SysConfig 1.26.2 at the exact path already configured in
`.eide/eide.yml` and `.vscode/tasks.json`:

```text
C:/ti/sysconfig_1.26.2
```

Run SysConfig with the repository SDK product metadata and regenerate the
contents of `Debug/` from `NewProject1.syscfg`. Do not edit
`Debug/ti_msp_dl_config.c` or `Debug/ti_msp_dl_config.h` manually.

Generated differences must be reviewed before the target build proceeds.

## 4. SysConfig Invariants

The regenerated output must contain `ICM42688_TIMER_INST` for the dedicated
1 MHz timebase described by the migration design.

The following existing hardware assignments must remain unchanged:

- `CONTROL_TIMER_INST` remains TIMG0 and provides the 500 Hz control interrupt.
- M1 through M4 PWM timer channels remain on their documented pins.
- All eight encoder GPIO inputs remain on their documented pins.
- I2C0 remains assigned to the MCP23017 and other I2C devices.
- SPI1 and the software chip-select GPIO remain assigned to the ICM42688.
- Existing DMA, UART, SWD, and motor-direction GPIO mappings remain unchanged.

If regeneration removes the dedicated timer, changes a listed peripheral or
pin, reports a resource conflict, or produces an unexpected broad rewrite, the
repair stops for user review before compilation continues.

## 5. Verification Flow

### 5.1 Preserve Failure Evidence

Before applying the production changes, compile the affected translation units
with the configured AC6 target parameters and confirm the known failures:

- `src/main.c` fails through FreeRTOS because `configENABLE_MPU` is undefined.
- `behavior_planner.c` fails because `NULL` is undefined.

These commands establish that each minimal change addresses a reproduced
failure rather than an assumed one.

### 5.2 Verify the Source Repairs

After each source change, rerun the corresponding AC6 compile:

- Compile `src/main.c` and the selected FreeRTOS kernel sources after adding
  `configENABLE_MPU`.
- Compile `modules/Sens-Decision/src/behavior_planner.c` after adding
  `<stddef.h>`.

No diagnostic-only command-line definitions may be used for this verification.

### 5.3 Run Host Tests

Run the project test entry point from a clean test output directory:

```powershell
powershell -ExecutionPolicy Bypass -File ".\tests\run_tests.ps1"
```

All existing host test executables must compile and run successfully. The test
runner must finish with `Host tests: PASS`.

### 5.4 Generate SysConfig Output

Run the configured SysConfig command using:

```powershell
& "C:\ti\sysconfig_1.26.2\sysconfig_cli.bat" `
  --product "..\..\controller\documents\sdk\.metadata\product.json" `
  --device MSPM0G3507 `
  --package "LQFP-48(PT)" `
  --script ".\NewProject1.syscfg" `
  --output ".\Debug" `
  --compiler keil
```

Generation must exit successfully. Review the generated files against the
invariants in Section 4 before proceeding.

### 5.5 Run the Complete Target Rebuild

Run an EIDE Rebuild using AC6, including the mandatory pre-build SysConfig task.
A collection of independently compiled objects is not sufficient target-build
evidence.

The rebuild must produce the configured AXF or ELF artifact under
`build/Debug/` and a link map suitable for object and symbol inspection.

### 5.6 Inspect the Linked Image

Confirm the target image contains:

- `portasm.o`
- `PendSV_Handler`
- `SVC_Handler`
- `vStartFirstTask`
- All intended application and module objects

Confirm the image excludes:

- OLED sources
- `example_usage.c`
- Host tests and temporary test sources

Record final Flash and SRAM usage. The image must fit within 128 KiB Flash and
32 KiB SRAM.

### 5.7 Final Repository Checks

Run:

```powershell
git diff --check
git status --short
```

Review only repair-owned files. Existing unrelated worktree modifications must
not be reverted, overwritten, or included in the repair.

## 6. Failure Handling

- If SysConfig installation or execution fails, report the exact environment
  error and do not fabricate or manually patch generated files.
- If EIDE's generated `builder.params` does not contain the complete expanded
  SysConfig command, stop before the full target rebuild.
- If generated hardware mappings violate Section 4, stop and request user
  review before continuing.
- If the two source repairs expose another compiler or linker error, reproduce
  and trace that error independently before proposing another change.
- If the full target build succeeds but memory limits are exceeded or required
  FreeRTOS symbols are absent, the repair is not accepted.
- Hardware flashing and on-device functional tests are outside this build
  repair. No claim about runtime hardware behavior follows from compilation.

## 7. Acceptance Criteria

The firmware build repair is complete only when all of the following are true:

- SysConfig 1.26.2 is available at the configured path.
- EIDE's generated `builder.params` contains the executable and every required
  SysConfig argument in its pre-build command.
- SysConfig generation succeeds using repository SDK 2.10.00.04.
- Generated files contain `ICM42688_TIMER_INST` and preserve all documented
  hardware assignments.
- The two known AC6 compiler failures no longer reproduce without command-line
  workarounds.
- All host tests pass from a clean test build.
- A complete EIDE/AC6 Rebuild exits successfully.
- The configured target firmware artifact and link map exist.
- Required FreeRTOS port objects and symbols are linked.
- Excluded sources are absent from the linked image.
- Flash and SRAM usage remain within target limits.
- `git diff --check` reports no repair-introduced whitespace errors.
- No unrelated worktree change is modified.

## 8. Out of Scope

- Control, perception, trajectory, or planner behavior changes
- Pin remapping or peripheral redesign
- FreeRTOS scheduling or memory tuning beyond the required MPU setting
- Build-path abstraction or support for arbitrary SysConfig installation roots
- A new CI pipeline or build system
- GCC-based target firmware support
- Flashing, hardware-in-the-loop tests, or vehicle tuning
- Cleanup of unrelated warnings, generated binaries, or existing worktree
  changes
