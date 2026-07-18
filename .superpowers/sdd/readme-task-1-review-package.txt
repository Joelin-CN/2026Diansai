# README Task 1 review package

## Base

fb950819624530dce45727bc5dca2773f414e2d7 (working-tree review; commits prohibited)

## Scope

- New implementation file: README.md
- 168 lines
- No other implementation file changed by this task
- Approved design and plan are separate untracked documentation inputs

## Content outline

- Project purpose and hardware-verification boundary
- Authority table for README, syscfg, wiring guide, EIDE config, generated files
- MCU/clock/memory/RTOS/task/tool versions
- Peripheral resource summary
- Host tests, SysConfig, EIDE rebuild, HEX programming instructions
- Verified/unverified state
- PlatformTime, linker-removal, UART limitations
- Software/hardware module resource declarations and hard constraints
- 13-step acceptance checklist and stop conditions

## Mechanical evidence

- UTF-8 required-token check: pass
- Referenced path existence check: pass
- Markdown fences: 6 fence lines / 3 balanced PowerShell blocks
- git diff --check -- README.md: exit 0
- README.md status: untracked
- Firmware tests/rebuild: intentionally not rerun for documentation-only task; controller baseline host suite passed before dispatch

## Known external inconsistency

WIRING_AND_SYSCONFIG.md line 3 contains a stale historical statement about SysConfig 1.24 / SDK 2.05. README explicitly declares current SysConfig 1.26.2+4477, SDK 2.10.00.04, and NewProject1.syscfg as the sole hardware authority. The wiring file was outside this task's approved modification scope.

## Full implementation

Read E:\B306\2026\电赛\2025e\m0_controller\README.md once for the complete untracked file.
