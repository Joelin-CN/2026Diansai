# Task 1 Report: Project README and Extension Gate

## Status

`DONE_WITH_CONCERNS`

Created `2025e/m0_controller/README.md` as the concise Chinese project entry point and mandatory module-extension gate. No commit was created. No firmware test, SysConfig generation, or firmware rebuild was run because the brief explicitly required documentation-only verification and supplied final firmware evidence.

## Content Added

The README contains:

- Project purpose and the explicit boundary between build verification and target-hardware acceptance.
- Documentation authority table for `README.md`, `NewProject1.syscfg`, `WIRING_AND_SYSCONFIG.md`, `.eide/eide.yml`, and generated `Debug/ti_msp_dl_config.c/.h`.
- MCU, clocks, Flash/SRAM image usage, FreeRTOS, heap, task, toolchain, SDK, and SysConfig summary.
- Concise timer, PWM, motor GPIO, encoder, I2C, SPI, DMA, UART, SWD/reset, and PA23 resource summary.
- Link to `WIRING_AND_SYSCONFIG.md` instead of duplicating the complete wiring/IOMUX tables.
- Reproducible host-test, SysConfig, EIDE/AC6 rebuild, and programming instructions with valid PowerShell fences.
- Verified and unverified status sections.
- Prominent TIMG12/platform-time, unreachable ICM42688/AHRS objects, and UART runtime limitations.
- Required resource declarations, hard constraints, and the complete 13-step new-module acceptance checklist.

## Factual Checks

| Claim area | Evidence checked | Result |
|---|---|---|
| Device/package and mappings | `NewProject1.syscfg`; `Debug/ti_msp_dl_config.h`; `WIRING_AND_SYSCONFIG.md` | MSPM0G3507 LQFP-48 and summarized pin/peripheral assignments agree. |
| Main and auxiliary clocks | `NewProject1.syscfg`; `Debug/ti_msp_dl_config.c/.h` | CPU/BUSCLK 32 MHz; HFXT/SYSPLL disabled; MFCLK enabled; TIMG12 uses MFCLK divide-by-4 for 1 MHz. |
| Timers and PWM | `NewProject1.syscfg`; generated `Debug/ti_msp_dl_config.c/.h`; `src/main.c` | TIMG0 is 2 ms, stopped by generated init, then started by the control task; TIMG12 is maximum-period, stopped, and has no interrupt; PWM is 32 MHz / 1000 = 32 kHz. |
| RTOS/heap/task | `inc/FreeRTOSConfig.h`; `build/Debug/builder.params`; `src/main.c` | non-MPU Cortex-M0+ port; 1 kHz tick; `heap_4.c`; 4 KiB heap; task `control`, priority 4, stack 512 words. |
| Toolchain and SDK | `build/Debug/builder.params`; `.eide/eide.yml`; `controller/documents/sdk/.metadata/product.json` | AC6 at `C:\Keil_v5\ARM\ARMCLANG`; SDK 2.10.00.04; pinned SysConfig command at `C:\ti\sysconfig_1.26.2`. |
| Tests | `tests/run_tests.ps1`; `.superpowers/sdd/firmware-final-review-package.txt` | Seven test executables are defined; final evidence records exit 0 and `Host tests: PASS`. |
| Image sizes and linked symbols | `build/Debug/NewProject1.map` | Total ROM 30,624 bytes; Total RW 10,240 bytes; `portasm.o`, `vStartFirstTask`, `PendSV_Handler`, and `SVC_Handler` are linked. |
| Output artifacts | `build/Debug/NewProject1.axf/.hex/.map/.s19` | All exist; HEX length is 86,181 bytes, approximately 84.16 KiB. |
| Removed code | `build/Debug/NewProject1.map` | Unreachable sections from `ahrs_hal.o` and `icm42688_mspm0.o` are explicitly removed by the linker. |
| Runtime limitations | `src/platform_time.c`; `src/main.c`; map evidence | `PlatformTime_Init()` does not start TIMG12; `PlatformTime_GetUs32()` returns `0U`; no hardware operation is inferred from a successful link. |
| Target exclusions | `.eide/eide.yml`; `build/Debug/builder.params`; `.superpowers/sdd/firmware-final-review-package.txt` | OLED/example/temp exclusions are configured; final map review records OLED, example, host-test, and temp sources absent. |

## Commands and Exit Results

All project commands ran from `E:\B306\2026\电赛\2025e\m0_controller`.

1. Documentation precondition:

```powershell
if (Test-Path -LiteralPath ".\README.md") { throw "README.md already exists; inspect before creating it" }
if (-not (Test-Path -LiteralPath ".\WIRING_AND_SYSCONFIG.md")) { throw "Detailed wiring reference is missing" }
if (-not (Test-Path -LiteralPath ".\NewProject1.syscfg")) { throw "Authoritative SysConfig input is missing" }
```

Exit `0`; no output. This established that README was new and both required references existed.

2. Initial worktree and branch inspection:

```powershell
git status --short
```

Exit `0`; branch `main`. Output showed the intentional pre-existing firmware/config/generated/test/report changes described by the user. None was reverted or edited.

3. Artifact size inspection:

```powershell
$paths = @('.\build\Debug\NewProject1.axf','.\build\Debug\NewProject1.hex','.\build\Debug\NewProject1.map','.\build\Debug\NewProject1.s19')
foreach ($path in $paths) {
    if (Test-Path -LiteralPath $path) {
        $item = Get-Item -LiteralPath $path
        "{0}`t{1}" -f $path, $item.Length
    } else {
        "MISSING`t$path"
    }
}
```

Exit `0`; AXF `346952`, HEX `86181`, map `249631`, S19 `91888` bytes.

4. Brief-prescribed token/link check, first execution:

```powershell
$readme = Get-Content -Raw -LiteralPath ".\README.md"
# Required-token and referenced-path loops from Step 9 of the brief.
```

Exit `1`; Windows PowerShell 5.1 decoded the UTF-8 Chinese heading using the legacy default encoding, so `新模块验收清单` became mojibake. All ASCII tokens had matched. This was a verification-command encoding problem, not missing README content.

5. UTF-8-correct token/link check:

```powershell
$readme = Get-Content -Raw -Encoding UTF8 -LiteralPath ".\README.md"
$required = @(
    'MSPM0G3507',
    'NewProject1.syscfg',
    'WIRING_AND_SYSCONFIG.md',
    '30,624',
    '10,240',
    'TIMG0',
    'TIMG12',
    'PlatformTime_GetUs32',
    'ahrs_hal.o',
    'icm42688_mspm0.o',
    'Host tests: PASS',
    '--rebuild',
    '新模块验收清单'
)
foreach ($token in $required) {
    if ($readme -notlike "*$token*") { throw "README missing required content: $token" }
}
foreach ($path in @(
    '.\NewProject1.syscfg',
    '.\WIRING_AND_SYSCONFIG.md',
    '.\.eide\eide.yml',
    '.\Debug\ti_msp_dl_config.c',
    '.\Debug\ti_msp_dl_config.h'
)) {
    if (-not (Test-Path -LiteralPath $path)) { throw "README reference is missing: $path" }
}
'README UTF-8 token/link checks: PASS'
```

Exit `0`; output `README UTF-8 token/link checks: PASS`.

6. Markdown fence check:

```powershell
$lines = Get-Content -LiteralPath ".\README.md"
$fences = @($lines | Where-Object { $_ -match '^```' })
if (($fences.Count % 2) -ne 0) { throw "Unbalanced Markdown fences: $($fences.Count)" }
if ($fences.Count -ne 6) { throw "Unexpected Markdown fence count: $($fences.Count)" }
"Markdown fences: PASS ($($fences.Count) fence lines)"
```

Exit `0`; output `Markdown fences: PASS (6 fence lines)`. The three PowerShell blocks are balanced and no nested fence was flattened.

7. Required whitespace check:

```powershell
git diff --check -- README.md
```

Exit `0`; no output.

8. Scoped status:

```powershell
git status --short -- README.md `
  docs/superpowers/specs/2026-07-18-readme-and-module-constraints-design.md `
  docs/superpowers/plans/2026-07-18-readme-and-module-constraints.md
```

Exit `0`; output:

```text
?? README.md
?? docs/superpowers/plans/2026-07-18-readme-and-module-constraints.md
?? docs/superpowers/specs/2026-07-18-readme-and-module-constraints-design.md
```

The design and plan were already present and were only read. README is the sole implementation deliverable created by this task.

9. Required scoped diff command:

```powershell
git diff -- README.md
```

Exit `0`; no output because README is untracked. To review the complete untracked-file diff, the following was also run:

```powershell
git diff --no-index -- NUL README.md
```

Exit `1`, which is the expected `git diff --no-index` status when differences exist. It displayed one new `README.md` with 168 lines. The only diagnostic was the repository line-ending warning that LF may be converted to CRLF when Git next writes the file; no whitespace-error diagnostic appeared.

10. Scoped changed-file inspection:

```powershell
$tracked = @(git diff --name-only)
$untracked = @(git ls-files --others --exclude-standard)
$taskChanged = @($tracked + $untracked | Where-Object {
    $_ -eq 'README.md' -or
    $_ -like 'WIRING_AND_SYSCONFIG.md' -or
    $_ -like 'NewProject1.syscfg' -or
    $_ -like 'Debug/*' -or
    $_ -like '.eide/*' -or
    $_ -like 'tests/*' -or
    $_ -like 'build/*' -or
    $_ -like 'pictures/*' -or
    $_ -like 'docs/superpowers/specs/2026-07-18-readme-and-module-constraints-design.md' -or
    $_ -like 'docs/superpowers/plans/2026-07-18-readme-and-module-constraints.md'
})
$taskChanged | ForEach-Object { $_ }
if ($taskChanged -notcontains 'README.md') { throw 'README.md is not present in changed files' }
```

Exit `0`. It listed README plus pre-existing changes/untracked artifacts: `.eide/eide.yml`, `NewProject1.syscfg`, the approved design/plan, and four generated `tests/build/*.exe` files. The initial status proves those non-README paths predated this task; this task did not modify them.

## Files Changed by This Task

- Created implementation file: `E:\B306\2026\电赛\2025e\m0_controller\README.md`.
- Created required external report: `E:\B306\2026\电赛\.superpowers\sdd\readme-task-1-report.md`.
- No other file was edited, reverted, generated, built, staged, or committed by this task.

## Self-Review

- README follows all nine content steps in the exact task brief.
- It remains concise by summarizing peripheral assignments and linking to the detailed wiring/IOMUX document.
- Hardware-source authority and generated-file prohibition are explicit.
- Build success is clearly separated from target-board runtime validation.
- Runtime limitations are prominent and supported by current source/map artifacts.
- All mandatory extension declarations, hard constraints, 13 acceptance steps, and stop conditions are present.
- Commands and identifiers remain ASCII inside code spans and fences.
- Three PowerShell fences are balanced.
- Required tokens and referenced paths pass the UTF-8-aware mechanical check.
- `git diff --check -- README.md` exits `0`.
- No firmware tests or rebuild were rerun, as required.
- No commit was created.

## Concerns

- `WIRING_AND_SYSCONFIG.md` line 3 still states that SysConfig 1.24 / SDK 2.05 was verified. That text is stale relative to current final evidence (SysConfig 1.26.2+4477 / SDK 2.10.00.04). The file was intentionally left untouched by scope. README uses the current authoritative artifacts and approved final evidence.
- The exact Step 9 command omits `-Encoding UTF8`, so its Chinese-token check fails under Windows PowerShell 5.1 even though the UTF-8 README is correct. The same check passes after adding `-Encoding UTF8`; both outcomes are recorded above.
- The exact `git diff -- README.md` command does not display an untracked file. `git diff --no-index -- NUL README.md` was used for full content review without staging it.

## Final Review Fixes

The final README review findings were applied to `README.md` only:

- Replaced the absolute SPI1 DMA reuse prohibition with an ownership rule: SPI1 RX/TX DMA CH2/CH3 have one unified SPI1 transfer-layer owner. Devices sharing SPI1 may use that service but may not independently reconfigure, seize, concurrently drive, or establish another owner for those channels. ICM42688 transaction integrity and chip-select ownership remain mandatory.
- Replaced the machine-user-specific builder path with executable PowerShell based on `Join-Path $env:USERPROFILE`, while retaining `cl.eide-3.27.2`.
- Documented the working EIDE UI flow: open `NewProject1` in the project tree, right-click the `NewProject1` root/`SOLUTION` node, and choose “生成 `builder.params`”/“Generate `builder.params`”. Also documented that the command palette may report no active project if the project is not open.
- Explicitly labeled HEX programming as an unvalidated procedure and required validation of the programmer, probe, and procedure before use. No OpenOCD command was added.

No implementation, configuration, source, generated, test, build, picture, design, or plan file was modified for these fixes. No commit was created.

## Final Review Verification

All commands ran from `E:\B306\2026\电赛\2025e\m0_controller`.

1. UTF-8 token/link check:

```powershell
$readme = Get-Content -Raw -Encoding UTF8 -LiteralPath ".\README.md"
$required = @(
    'MSPM0G3507',
    'NewProject1.syscfg',
    'WIRING_AND_SYSCONFIG.md',
    '30,624',
    '10,240',
    'TIMG0',
    'TIMG12',
    'PlatformTime_GetUs32',
    'ahrs_hal.o',
    'icm42688_mspm0.o',
    'Host tests: PASS',
    '--rebuild',
    '新模块验收清单'
)
foreach ($token in $required) {
    if ($readme -notlike "*$token*") { throw "README missing required content: $token" }
}
foreach ($path in @(
    '.\NewProject1.syscfg',
    '.\WIRING_AND_SYSCONFIG.md',
    '.\.eide\eide.yml',
    '.\Debug\ti_msp_dl_config.c',
    '.\Debug\ti_msp_dl_config.h'
)) {
    if (-not (Test-Path -LiteralPath $path)) { throw "README reference is missing: $path" }
}
'README UTF-8 token/link check: PASS'
```

Exit `0`; output `README UTF-8 token/link check: PASS`.

2. Markdown fence check:

```powershell
$lines = Get-Content -Encoding UTF8 -LiteralPath ".\README.md"
$fences = @($lines | Where-Object { $_ -match '^```' })
if (($fences.Count % 2) -ne 0) { throw "Unbalanced Markdown fences: $($fences.Count)" }
if ($fences.Count -ne 6) { throw "Unexpected Markdown fence count: $($fences.Count)" }
"Markdown fence check: PASS ($($fences.Count) fence lines, 3 blocks)"
```

Exit `0`; output `Markdown fence check: PASS (6 fence lines, 3 blocks)`.

3. Whitespace check:

```powershell
git diff --check -- README.md
if (-not $?) { throw 'git diff --check failed' }
'git diff --check: PASS'
```

Exit `0`; output `git diff --check: PASS`.

4. Review-text presence check:

```text
Grep include: README.md
Pattern: Join-Path \$env:USERPROFILE|右键单击 `NewProject1` 根节点|命令面板可能提示没有活动项目|以下烧录流程尚未经过硬件验证|实际使用前必须先验证烧录工具、探针及其操作流程|SPI1 RX/TX 的 DMA CH2/CH3 由统一 SPI1 传输层独占管理|同一总线上的新设备可以使用该服务|不得独立重配置、抢占、并发驱动这些通道|不得为其建立其他所有者|保持 ICM42688 事务完整
```

Result: four matching README lines at lines 83, 86, 99, and 146 covered every required review phrase.

5. Obsolete/unsupported text absence check:

```text
Grep include: README.md
Pattern: C:\\Users\\Joelin|不得复用 DMA CH2/CH3|OpenOCD
```

Result: no matching README content. The hard-coded user path, absolute DMA prohibition, and unvalidated OpenOCD procedure are absent.

6. Full untracked README review:

```powershell
git diff --no-index -- NUL README.md
```

Exit `1`, the expected `git diff --no-index` status when a new file differs from `NUL`. The complete 168-line README was displayed with all four fixes. Git emitted only the existing LF-to-CRLF working-copy warning.

Two supplementary PowerShell review assertions were attempted and are not acceptance checks:

- A Chinese literal assertion exited `1` because Windows PowerShell 5.1 decoded Chinese text in the inline command as mojibake. UTF-8 file reading and grep both verified the required line.
- A wildcard structural assertion exited `1` because its pattern treated Markdown backticks as PowerShell escape syntax and did not match the EIDE UI line. The direct UTF-8 grep located that exact line at README line 83.

The required UTF-8 token/link check, Markdown fence check, and `git diff --check` all exited `0`.
