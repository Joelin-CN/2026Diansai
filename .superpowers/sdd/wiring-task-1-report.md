# Wiring Task 1 Implementation Report

## Status

DONE

## Changed Files

- `2025e/m0_controller/WIRING_AND_SYSCONFIG.md`: synchronized authority/tool versions, clock and timer allocation, I2C/OLED status, SPI1 DMA ownership, and verification boundary.
- `.superpowers/sdd/wiring-task-1-report.md`: this implementation report, required by the task brief.

No commit was created. No other file was edited by this task.

## Precondition

Before editing:

- `grep` for `SysConfig 1\.24 / SDK 2\.05` in `WIRING_AND_SYSCONFIG.md`: PASS, one match at original line 3.
- `grep` for `TIMG12` in `WIRING_AND_SYSCONFIG.md`: PASS, no match.
- `git status --short -- "WIRING_AND_SYSCONFIG.md"`: PASS, no output; the implementation file had no pre-existing worktree change.

## Mechanical Checks

Executed from `2025e/m0_controller` after editing:

1. Required-token PowerShell loop using `git grep --untracked -F -q` against `WIRING_AND_SYSCONFIG.md`.
   Result: PASS, all 15 required tokens present: `SysConfig 1.26.2+4477`, `SDK 2.10.00.04`, `NewProject1.syscfg`, `README.md`, `TIMG0`, `63999`, `TIMG12`, `4294967295`, `MFCLK / 4`, `PlatformTime_GetUs32`, `DMA CH2/CH3`, `统一 SPI1 传输层`, `OLED 源码当前未链接`, `当前验证边界`, and `尚未完成目标板烧录`.
2. Existing-mapping-token PowerShell loop using `git grep --untracked -F -q` against `WIRING_AND_SYSCONFIG.md`.
   Result: PASS, all 69 required existing GPIO, reset, and PINCM tokens remain.
3. Stale-version check using `git grep --untracked -F -q -e "SysConfig 1.24 / SDK 2.05" -- "WIRING_AND_SYSCONFIG.md"`.
   Result: PASS, exit code 1 as expected; stale text is absent.
4. `git diff --check -- "WIRING_AND_SYSCONFIG.md"`.
   Result: PASS, exit code 0; no whitespace errors. Git emitted only the repository line-ending warning that LF will be replaced by CRLF when Git next touches the file.
5. `git diff -- "WIRING_AND_SYSCONFIG.md"`.
   Result: PASS; scoped diff contains only the brief-required introduction, clock/timer section, three I2C/OLED wording updates, SPI DMA ownership paragraph, and verification-boundary section. Existing pin, connector, IOMUX, motor, encoder, SWD, reset, correction, and MCP23017 mapping entries are otherwise unchanged.
6. `git status --short`.
   Result: inspected. `WIRING_AND_SYSCONFIG.md` is modified by this task. Numerous unrelated pre-existing modified/untracked/deleted paths remain in the wider worktree and were not touched.
7. `grep` for `CONTROL_TIMER|ICM42688_TIMER|63999|4294967295|MFCLK|MCLK|BUSCLK|DMA_CH2|DMA_CH3|DMA_SPI1` in `NewProject1.syscfg` and `Debug/ti_msp_dl_config.{c,h}`.
   Result: PASS. Confirmed TIMG0 2 ms periodic ZERO interrupt with no autostart, TIMG12 MFCLK / 4 periodic timer with no interrupt/autostart, load values 63999 and 4294967295, and SPI1 DMA channel 2/3 assignments.
8. Source checks for `PlatformTime_GetUs32`, `DL_TimerG_startCounter(CONTROL_TIMER_INST)`, and timer identifiers.
   Result: PASS. `src/platform_time.c` still returns `0U`; `src/main.c` starts CONTROL_TIMER in `ControlTask`; no application start/read of TIMG12 was found.

An initial attempt to run equivalent `rg` token checks failed because `rg` is not installed on PATH. The checks were rerun successfully with `git grep` as documented above.

## Self-Review

Reviewed against `docs/superpowers/specs/2026-07-18-wiring-sysconfig-sync-design.md` and `.superpowers/sdd/wiring-task-1-brief.md`:

- Authority boundary is explicit: `NewProject1.syscfg` is sole hardware authority; generated C/header files are marked non-editable; README remains the entry point for build/runtime/module-extension guidance.
- Fixed SysConfig and SDK versions exactly match the brief.
- Clock and timer facts match current SysConfig and generated output, including disabled HFXT/SYSPLL, 32 MHz CPU/BUSCLK consumers, 4 MHz MFCLK, timer sources, loads, modes, interrupt/autostart state, control-task startup, and timestamp stub limitation.
- I2C wording uses the exact required OLED linkage and 3.3 V/address constraints without changing PA0/PA1 mappings.
- SPI1 DMA ownership paragraph is inserted immediately after the DMA table and preserves independent CS/full ICM42688 transaction requirements.
- Verification section distinguishes generation/mapping/build evidence from unverified target flashing, boot/reset, timing, electrical, peripheral, control-loop, EMI, power-integrity, and WCET behavior, and explicitly avoids hardware-qualification claims.
- Existing physical mappings were preserved and checked against current `NewProject1.syscfg`, generated definitions, and the scoped diff. No remapping was introduced.
- No build or runtime command was run because this is a documentation-only task and the brief requires reporting the existing verified build boundary, not modifying or regenerating artifacts.

## Concerns

- The wider worktree already contains many unrelated changes, including pre-existing modifications to `NewProject1.syscfg` and generated `Debug/ti_msp_dl_config.c/.h`; this task did not modify or revert them and reviewed the current contents as required.
- Hardware flashing, boot/reset, electrical behavior, actual clock/timer accuracy, peripheral/control-loop runtime, EMI, power integrity, and WCET remain unverified by design.
