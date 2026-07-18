### Task 7: Restore a Reproducible EIDE/SysConfig Project Definition

**Files:**
- Modify: `NewProject1.syscfg`
- Create: `.eide/eide.yml`
- Create: `.eide/files.options.yml`
- Create: `.vscode/tasks.json`
- Create: `.vscode/launch.json`
- Create: `.vscode/settings.json`
- Modify: `.gitignore`

**Interfaces:**
- Consumes: one matched MSPM0 SDK/SysConfig installation selected by the user at build time.
- Produces: `ICM42688_TIMER_INST`, all intended source/include lists, FreeRTOS `portasm.c`, and EIDE build/upload tasks.

- [ ] **Step 1: Restore metadata from the matching template, then make paths project-relative**

Use `controller/mspm0_2025e_template_v1/.eide` and `.vscode` as the structural baseline. Remove ignore rules that prevent project metadata from being tracked. Keep installation roots in `.vscode/settings.json` rather than duplicating absolute paths across files.

- [ ] **Step 2: Add production sources and includes explicitly**

Add these module source directories to EIDE and exclude `example_usage.c`, OLED, and all `temp/` paths. Add all four module `inc/` paths plus `inc/` and `Debug/`.

Add FreeRTOS:

```text
tasks.c
list.c
queue.c
portable/GCC/ARM_CM0/port.c
portable/GCC/ARM_CM0/portasm.c
portable/MemMang/heap_4.c
```

The map must contain `PendSV_Handler`, `SVC_Handler`, and `vStartFirstTask` from `portasm.o`.

- [ ] **Step 3: Add a dedicated SysConfig timer**

In `NewProject1.syscfg`, add a timer instance named `ICM42688_TIMER`, assigned to a free 32-bit-capable timer such as TIMG12, configured from 32 MHz BUSCLK with a divide/prescale combination yielding exactly 1 MHz, maximum period, automatic start disabled, and no interrupt. `PlatformTime_Init()` owns start timing.

Do not edit `Debug/ti_msp_dl_config.h` or `.c` manually.

- [ ] **Step 4: Select one SDK/SysConfig version pair**

Prefer the repository SDK `controller/documents/sdk` version `2.10.00.04` with SysConfig `1.26.2`, or install and consistently use the older `2.05.01.00`/`1.24.1` pair. Do not generate with one pair and compile against the other.

- [ ] **Step 5: Generate SysConfig and inspect output when tools are available**

With the repository SDK selected, install SysConfig 1.26.2 at `C:\ti\sysconfig_1.26.2` and run:

```powershell
& "C:\ti\sysconfig_1.26.2\sysconfig_cli.bat" --product "E:\B306\2026\电赛\controller\documents\sdk\.metadata\product.json" --device MSPM0G3507 --package "LQFP-48(PT)" --script ".\NewProject1.syscfg" --output ".\Debug" --compiler keil
```

Expected: generated header contains `ICM42688_TIMER_INST`; existing motor, encoder, I2C0, SPI1, DMA, UART, and `CONTROL_TIMER_INST` mappings remain unchanged.

- [ ] **Step 6: Build or record the environmental blocker accurately**

Run EIDE Rebuild. Expected success artifact is `build/Debug/NewProject1.axf`. If ARMCLANG/SysConfig remains unavailable, record that exact blocker and continue only with host-tested work; do not label the firmware build as passing.
