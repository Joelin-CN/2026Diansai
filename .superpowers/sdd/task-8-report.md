# Task 8 Report: STM32 Cross-Compilation Integration

**Date:** 2026-07-31
**Base:** `cc5dc84c503e276f6c9198a0cb76839332565160`
**Commit:** `974eac16fe165f4ab869bc8a031379639d4b2631` (`build: link balance control core`)
**Status:** Implemented and verified

## Change

Added the seven protocol-independent `App/Src/*.c` modules and the private `App/Inc` include directory to the `balanceBall` executable in the root `CMakeLists.txt`.

No changes were made to `main.c`, generated STM32 files, App sources, tests, or the task ledger. The control core is compiled into ARM objects but is intentionally not invoked by firmware yet.

## Baseline

Before the CMake edit:

```text
cmake --preset Debug: exit 0
cmake --build --preset Debug: exit 0
Test-Path build/Debug/CMakeFiles/balanceBall.dir/App/Src/balance_loop.c.obj: False
```

This established that the existing firmware built but did not compile the App control core.

## Host Verification

Fresh completion-gate commands:

```text
cmake --build build/host-tests: exit 0
ctest --test-dir build/host-tests --output-on-failure: exit 0
1/1 Test #1: balance_core ... Passed
100% tests passed, 0 tests failed out of 1
```

## ARM Verification

Fresh clean cross-build:

```text
cmake --build --preset Debug --clean-first: exit 0
[30/30] Linking C executable balanceBall.elf
RAM: 2208 B / 20 KB (10.78%)
FLASH: 9896 B / 64 KB (15.10%)
```

Resulting ELF:

```text
E:\B306\2026\diansai\2026task\stm32\balanceBall\balanceBall\build\Debug\balanceBall.elf
```

ARM object symbol proof:

```text
arm-none-eabi-nm build/Debug/CMakeFiles/balanceBall.dir/App/Src/balance_loop.c.obj
00000000 T balance_loop_init
```

Exactly one `balance_loop_init` symbol was found, and `arm-none-eabi-nm` exited `0`.

## Scope And Safety

The commit contains exactly one file:

```text
2026task/stm32/balanceBall/balanceBall/CMakeLists.txt
```

`App/` and `tests/` were already tracked and were not restaged. No build output, `.superpowers/` content, project photo, deleted PDFs, unrelated `v1.0` changes, generated files, or other untracked project baseline files were committed.

The staged CMake patch passed `git diff --cached --check`. Repository-wide `git diff --check` exits `2` because pre-existing unrelated task briefs `task-1-brief.md` through `task-7-brief.md` contain a new blank line at EOF; those files were not modified for this task.

The brief's `nm | rg` command could not run literally because `rg` is unavailable in this PowerShell environment. Equivalent PowerShell filtering of the complete `arm-none-eabi-nm` output found exactly one symbol and returned success.

## Remaining Worktree State

The repository remains intentionally dirty with pre-existing unrelated changes, including modified `.superpowers/sdd` files, deleted source PDFs/images, modified and generated `v1.0` files, nested repositories, untracked STM32 project-baseline files, and the untracked project photo. None were staged or altered by this task except this required report outside the project directory.

## Limit

This verifies host behavior and ARM cross-compilation only. It does not claim physical closed-loop operation. Camera and stepper wire protocols, hardware adapters, `main.c` integration, bench tests, and user acceptance remain required.

## Reproducible Symbol Verification

Exact PowerShell command executed from `E:\B306\2026\diansai\2026task\stm32\balanceBall\balanceBall`:

```powershell
$nmOutput = @(arm-none-eabi-nm "build/Debug/CMakeFiles/balanceBall.dir/App/Src/balance_loop.c.obj"); $nmExit = $LASTEXITCODE; $symbolLines = @($nmOutput | Where-Object { $_ -match '^\S+\s+\S\s+balance_loop_init$' }); $symbolLines; "NM_EXIT=$nmExit"; "MATCH_COUNT=$($symbolLines.Count)"; if ($nmExit -ne 0 -or $symbolLines.Count -ne 1) { exit 1 }
```

Full relevant output:

```text
00000000 T balance_loop_init
NM_EXIT=0
MATCH_COUNT=1
```

The PowerShell process exited `0`. The native `arm-none-eabi-nm` exit code was captured before running the filter, and the command fails when that native exit code is nonzero or the exact symbol match count differs from one.
