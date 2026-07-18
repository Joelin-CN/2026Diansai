# Wiring and SysConfig Documentation Sync Design

## Goal

Update `WIRING_AND_SYSCONFIG.md` so its detailed hardware reference matches the
current MSPM0G3507 SysConfig, tool versions, clock/timer allocation, DMA
ownership, and verification boundary.

## Documentation Boundaries

- `NewProject1.syscfg` remains the sole authoritative hardware and peripheral
  configuration.
- `WIRING_AND_SYSCONFIG.md` documents detailed physical wiring, IOMUX, clocks,
  timer resources, and electrical constraints.
- `README.md` remains the project entry point for build instructions, runtime
  limitations, and module-extension gates.
- `Debug/ti_msp_dl_config.c` and `Debug/ti_msp_dl_config.h` remain generated and
  must not be edited manually.

## Changes

Update only `WIRING_AND_SYSCONFIG.md`:

1. Replace the stale SysConfig 1.24 / SDK 2.05 statement with the current fixed
   pair: SysConfig 1.26.2+4477 and repository SDK 2.10.00.04.
2. State that the generated C/header were regenerated and all documented
   mappings were audited, while flashing and hardware operation remain
   unverified.
3. Add a clock and timer section:
   - SYSOSC/BUSCLK and CPU at 32 MHz; HFXT and SYSPLL disabled.
   - MFCLK fixed at 4 MHz and enabled for TIMG12.
   - TIMG0 uses 32 MHz BUSCLK, 2 ms, load 63999, periodic ZERO interrupt, and is
     started by the control task for 500 Hz operation.
   - TIMG12 uses MFCLK / 4 for 1 MHz, load 4294967295, periodic mode, no
     interrupt, and no autostart.
   - The current application does not start or read TIMG12, and
     `PlatformTime_GetUs32()` returns zero.
4. Clarify SPI1 DMA ownership: DMA CH2/CH3 belong to one unified SPI1 transfer
   layer. Devices on the same bus may use that service but must not independently
   reconfigure, seize, or concurrently drive the channels.
5. Clarify I2C/OLED status: I2C0 wiring can physically support OLED and MCP23017
   on the shared 3.3 V bus, but OLED source code is currently excluded from the
   target firmware.
6. Preserve all existing pin, connector, IOMUX, motor, encoder, SWD, reset,
   original-correction, and MCP23017 tables unchanged unless a wording change is
   required for the OLED/DMA clarification.

## Verification

- Compare every documented physical mapping against `NewProject1.syscfg` and
  `Debug/ti_msp_dl_config.h`.
- Verify the new timer values against generated C/header and current source.
- Mechanically assert the new version, timer, DMA ownership, OLED exclusion,
  and hardware-boundary statements.
- Run `git diff --check -- WIRING_AND_SYSCONFIG.md`.
- Confirm no firmware, SysConfig input, generated output, README, build config,
  tests, or pictures are modified by this documentation sync.

## Scope

Documentation-only change. No firmware behavior, pin assignment, peripheral
selection, clock configuration, generated output, build result, or hardware
claim changes.
