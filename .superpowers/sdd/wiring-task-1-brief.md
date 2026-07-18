# Task 1 Brief: Synchronize the Detailed Wiring Guide

## Goal

Synchronize `2025e/m0_controller/WIRING_AND_SYSCONFIG.md` with the current SysConfig tool pair, hardware authority, clocks/timers, SPI1 DMA ownership, OLED target-firmware status, and verification boundary, without changing any existing physical mapping.

## Binding Constraints

- Modify only `2025e/m0_controller/WIRING_AND_SYSCONFIG.md` as the implementation file.
- Treat `NewProject1.syscfg` as the sole authoritative hardware and peripheral configuration.
- Do not edit `Debug/ti_msp_dl_config.c` or `Debug/ti_msp_dl_config.h`; they are generated files.
- Keep README as the project entry point for build instructions, runtime limitations, and module-extension gates.
- Preserve every existing pin, connector, IOMUX, motor, encoder, SWD, reset, correction, and MCP23017 table entry unless wording must change to clarify OLED or DMA status.
- Use SysConfig 1.26.2+4477 and repository SDK 2.10.00.04.
- Do not claim flashing, boot, electrical, timing, or peripheral runtime verification.
- Do not modify firmware behavior, SysConfig input, generated output, build configuration, tests, build artifacts, README, or `pictures/`.
- Do not commit.

## Required Changes

1. Replace the stale introductory version statement with current authority and verification status:
   - `NewProject1.syscfg` is the sole hardware authority.
   - `Debug/ti_msp_dl_config.c` and `.h` are generated and must not be edited manually.
   - Fixed tools are SysConfig 1.26.2+4477 and repository SDK 2.10.00.04.
   - Generated mappings were reviewed, but board flashing, electrical connection, and hardware runtime were not verified.
   - Link to `README.md` for build/runtime/module-extension guidance and retain the pinout summary link.
2. Before `## 通信接口`, add `## 系统时钟与定时器` with:
   - CPU / SYSOSC / BUSCLK: 32 MHz; HFXT and SYSPLL disabled; source for CPU, PWM, TIMG0, UART, SPI, and I2C.
   - MFCLK: fixed 4 MHz; enabled only for TIMG12.
   - TIMG0 / CONTROL_TIMER: BUSCLK 32 MHz, 2 ms period, load 63999, periodic mode, ZERO interrupt; 500 Hz control tick; initialized stopped and started by the `control` task.
   - TIMG12 / ICM42688_TIMER: MFCLK / 4, 1 MHz, load 4294967295, periodic mode, no interrupt, no automatic start; reserved microsecond time base currently neither started nor read by the application.
   - State that `PlatformTime_GetUs32()` still returns `0`, so TIMG12 is not integrated as an application timestamp; no target frequency/period measurement has occurred.
3. Update I2C notes:
   - I2C0_SDA note: `100 kHz；MCP23017 使用，可在硬件上与 3.3 V OLED 共用；OLED 源码当前未链接`
   - I2C0_SCL note: `100 kHz；共享设备必须使用 3.3 V 并避免地址冲突`
   - MCP23017 SDA/SCL row: `PA0 / PA1，使用 I2C0；硬件可与 3.3 V OLED 共用，但 OLED 当前未进入目标固件`
4. Immediately after the existing SPI DMA table, add:
   `DMA CH2/CH3 由统一 SPI1 传输层独占管理。同一 SPI1 总线上的新设备可以使用该传输服务，但不得独立重配置、抢占、并发驱动这些通道，也不得建立其他 DMA 所有者。所有设备仍必须使用独立片选，并保持 ICM42688 的完整 CS 低电平事务。`
5. Append `## 当前验证边界`:
   - Verified: generation with the fixed tool pair, generated TIMG0/TIMG12 and all documented peripheral/GPIO mappings, no remapping found during mapping review, EIDE/AC6 full compile/link success.
   - Not verified: HEX flashing, target boot/reset, actual clocks and timer accuracy, peripheral/control-loop hardware behavior, wiring/levels/EMI/power integrity/worst-case execution time.
   - End by stating the document describes current configuration and wiring requirements, not proof of hardware qualification.

## Required Verification

1. Before editing, prove the stale text `SysConfig 1.24 / SDK 2.05` exists and `TIMG12` is absent.
2. After editing, verify these tokens exist: `SysConfig 1.26.2+4477`, `SDK 2.10.00.04`, `NewProject1.syscfg`, `README.md`, `TIMG0`, `63999`, `TIMG12`, `4294967295`, `MFCLK / 4`, `PlatformTime_GetUs32`, `DMA CH2/CH3`, `统一 SPI1 传输层`, `OLED 源码当前未链接`, `当前验证边界`, `尚未完成目标板烧录`. Verify the stale version does not remain.
3. Verify these existing mapping tokens remain: `PB20`, `PA16`, `PA18`, `PA17`, `PA0`, `PA1`, `PA10`, `PA11`, `PA8`, `PA9`, `PB2`, `PB3`, `PA21`, `PA22`, `PA24`, `PA25`, `PB6`, `PB7`, `PB8`, `PA7`, `PA15`, `PB9`, `PB19`, `PB24`, `PA12`, `PA13`, `PA2`, `PA26`, `PA27`, `PA28`, `PA31`, `PB18`, `PA19`, `PA20`, `NRST`, `PA23`, `PINCM1`, `PINCM2`, `PINCM3`, `PINCM6`, `PINCM7`, `PINCM14`, `PINCM15`, `PINCM16`, `PINCM19`, `PINCM20`, `PINCM21`, `PINCM22`, `PINCM23`, `PINCM24`, `PINCM25`, `PINCM26`, `PINCM34`, `PINCM35`, `PINCM37`, `PINCM38`, `PINCM39`, `PINCM40`, `PINCM44`, `PINCM45`, `PINCM46`, `PINCM47`, `PINCM48`, `PINCM52`, `PINCM54`, `PINCM55`, `PINCM59`, `PINCM60`.
4. Run `git diff --check -- WIRING_AND_SYSCONFIG.md` and inspect the scoped diff/status.
5. Self-review against `docs/superpowers/specs/2026-07-18-wiring-sysconfig-sync-design.md` and this brief.

## Report

Write the implementation report to `E:/B306/2026/电赛/.superpowers/sdd/wiring-task-1-report.md`. Include changed files, exact checks run and results, self-review, and concerns. Return only `DONE`, a one-line verification summary, and concerns.
