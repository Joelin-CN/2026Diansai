# README and Module Extension Constraints Design

## Goal

Create `README.md` as the project entry point. It must describe the current
MSPM0G3507 firmware configuration accurately, distinguish build verification
from hardware runtime verification, and define mandatory constraints for adding
software or hardware modules.

## Documentation Boundaries

- `README.md` is the entry point and contains the architecture summary, build
  and programming instructions, resource summary, known limitations, and module
  extension gates.
- `NewProject1.syscfg` is the authoritative hardware and peripheral
  configuration.
- `Debug/ti_msp_dl_config.c` and `Debug/ti_msp_dl_config.h` are generated files
  and must never be edited manually.
- `WIRING_AND_SYSCONFIG.md` remains the detailed wiring and IOMUX reference.
  README links to it instead of duplicating every pin table.
- `.eide/eide.yml` is the authoritative EIDE source, include, exclusion,
  toolchain, and mandatory SysConfig pre-build configuration.

## README Structure

1. Project purpose and current scope.
2. Toolchain and reproducible build prerequisites.
3. MCU, clock, memory, RTOS, task, and firmware-size summary.
4. Peripheral resource summary covering timers, PWM, motor GPIO, encoders,
   I2C, SPI, DMA, UART, SWD, reset, and reserved pins.
5. Directory and authoritative-file guide.
6. Host-test, SysConfig generation, EIDE rebuild, and HEX programming commands.
7. Verified status and explicit hardware-verification boundary.
8. Known runtime limitations.
9. Mandatory constraints and acceptance checklist for adding modules.

## Current-State Content

README records these current facts:

- MSPM0G3507 Cortex-M0+, LQFP-48, 32 MHz CPU, 128 KiB Flash, and
  32 KiB SRAM.
- Current linked image: 30,624 bytes ROM and 10,240 bytes RW memory.
- FreeRTOS uses the non-MPU Cortex-M0+ port, 1 kHz tick, 4 KiB heap, and one
  application control task at priority 4 with a 512-word stack.
- TIMG0 provides the 500 Hz control interrupt.
- TIMG12 is configured as a 1 MHz maximum-period timer using fixed 4 MHz MFCLK
  divided by four, but the application does not yet start or read it.
- Four 32 kHz motor PWM outputs use TIMA0/TIMA1; eight direction GPIO outputs
  start cleared.
- Eight encoder GPIO inputs use both-edge interrupts.
- I2C0 is 100 kHz on PA0/PA1.
- SPI1 is 1 MHz mode 0 on PA16/PA18/PA17, with PB20 software chip select and
  DMA channels 2/3.
- UART0, UART1, and UART3 are configured for 115200 baud.
- SWD and reset remain reserved.
- Full pin and IOMUX assignments are linked from `WIRING_AND_SYSCONFIG.md`.

## Verification and Runtime Boundary

README separates verified build facts from unverified runtime behavior.

Verified:

- SysConfig 1.26.2+4477 generation with repository SDK 2.10.00.04.
- Seven host-test executables pass.
- Full EIDE/AC6 rebuild succeeds.
- Required FreeRTOS port object and symbols are linked.
- OLED, examples, host tests, and temporary sources are excluded.
- ROM and RAM remain within the device limits.

Not verified:

- Flash programming and boot on the target board.
- Electrical wiring and peripheral behavior on hardware.
- Motor, encoder, I2C, SPI, DMA, UART, scheduler, and control-loop operation on
  the physical system.

Known runtime limitations must be prominent:

- `PlatformTime_Init()` does not start TIMG12.
- `PlatformTime_GetUs32()` returns zero, so the 32-bit and 64-bit timestamps are
  not real hardware time.
- `ahrs_hal.o` and `icm42688_mspm0.o` are removed by the linker because their
  current implementations are unreachable.
- UART peripherals are initialized, but no application communication task is
  currently defined.

## Module Extension Constraints

Every new software or hardware module must document these resources before
implementation:

- Pins and IOMUX functions.
- Peripheral instances and channels.
- Clock source, frequency, divider, period, and accuracy requirements.
- IRQs, priority, ISR-to-task interaction, and maximum interrupt latency.
- DMA channels, triggers, direction, width, and ownership.
- Flash, static RAM, heap, task stack, and large-buffer estimates.
- Execution period, worst-case execution time, blocking behavior, and control
  loop impact.
- Bus address, mode, bitrate, chip-select ownership, and electrical level.
- Initialization order, failure behavior, and safe-state requirements.

Hard constraints:

- Do not reassign an existing pin, peripheral, timer channel, IRQ, DMA channel,
  SWD pin, or reset function without an explicit review.
- Do not change the 500 Hz control period, FreeRTOS interrupt priorities, heap,
  task priorities, or stack allocations without timing and memory evidence.
- Do not allocate PA23 without reviewing the documented TB6612 STBY and future
  interrupt reservation.
- I2C devices must share PA0/PA1 at 3.3 V and use a non-conflicting address.
- SPI1 additions must define software or hardware chip-select ownership and
  must not conflict with the ICM42688 transaction requirements or DMA 2/3.
- New UART users must define framing, ownership, buffering, and task/ISR data
  flow rather than relying only on SysConfig initialization.
- Hardware changes are made in `NewProject1.syscfg`, then regenerated with the
  pinned SysConfig/SDK pair. Generated DriverLib files are never hand-patched.
- Source and include paths are added explicitly to `.eide/eide.yml`; examples,
  tests, and temporary sources remain excluded from target firmware.
- A module is not considered integrated merely because its source compiles. The
  final map must show the intended object and symbols, or the documentation must
  explicitly state that linker removal is expected.

## Mandatory Extension Gate

For each new module:

1. Complete the resource declaration and conflict review.
2. Add or update host tests before production behavior changes where the logic
   is host-testable.
3. Add the smallest production implementation and safe failure behavior.
4. Update `NewProject1.syscfg` only when hardware resources are required.
5. Regenerate with SysConfig 1.26.2+4477 and SDK 2.10.00.04.
6. Audit all existing timer, PWM, encoder, I2C, SPI, DMA, UART, GPIO, SWD, and
   reset mappings, not only the new module.
7. Add target source/include entries and verify all exclusions.
8. Compile affected translation units with AC6 without diagnostic workarounds.
9. Run the complete host test suite.
10. Generate current EIDE `builder.params` and run a full AC6 rebuild.
11. Inspect the map for intended objects/symbols and forbidden sources.
12. Record the exact ROM and RAM delta and verify device limits.
13. Perform and document hardware programming and runtime tests separately.

Any mapping change, resource conflict, linker removal of intended code, memory
limit failure, or timing/priority change blocks acceptance until reviewed.

## Scope

This change creates documentation only. It does not alter firmware behavior,
hardware configuration, generated files, source lists, tests, or build output.
