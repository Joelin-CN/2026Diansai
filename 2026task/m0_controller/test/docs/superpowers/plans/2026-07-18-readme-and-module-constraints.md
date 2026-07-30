# README and Module Extension Constraints Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Create a project-entry README that accurately summarizes the current MSPM0G3507 firmware and defines mandatory constraints for adding software or hardware modules.

**Architecture:** Keep `README.md` concise enough to serve as the entry point while treating `NewProject1.syscfg` as the hardware authority and `WIRING_AND_SYSCONFIG.md` as the detailed pin reference. Document verified build evidence separately from unverified hardware behavior, then provide one resource declaration and acceptance gate for all future modules.

**Tech Stack:** Markdown, MSPM0G3507, TI SysConfig 1.26.2+4477, MSPM0 SDK 2.10.00.04, EIDE 3.27.2, Arm Compiler 6, FreeRTOS.

## Global Constraints

- Create only `README.md`; do not modify firmware behavior, hardware configuration, generated files, source lists, tests, or build output.
- Treat `NewProject1.syscfg` as the authoritative hardware and peripheral configuration.
- Never direct readers to edit `Debug/ti_msp_dl_config.c` or `Debug/ti_msp_dl_config.h` manually.
- Keep detailed pin and IOMUX tables in `WIRING_AND_SYSCONFIG.md`; README links to that file instead of duplicating the complete tables.
- Treat `.eide/eide.yml` as the authoritative EIDE source/include/exclusion/toolchain and mandatory SysConfig pre-build configuration.
- State only build and link results that were actually verified; do not claim flashing or on-device operation.
- Document current runtime limitations: TIMG12 is not started/read, `PlatformTime_GetUs32()` returns zero, `ahrs_hal.o` and `icm42688_mspm0.o` are linker-removed, and no UART application communication task exists.
- Use ASCII for new prose except where established project names or required user-facing Chinese text justify otherwise; this README will use concise Chinese prose with ASCII identifiers and commands.
- Do not modify `pictures/`.
- Do not commit unless the user explicitly authorizes a commit during execution.

---

### Task 1: Create the Project README and Extension Gate

**Files:**
- Create: `README.md`
- Reference: `docs/superpowers/specs/2026-07-18-readme-and-module-constraints-design.md`
- Reference: `NewProject1.syscfg`
- Reference: `Debug/ti_msp_dl_config.h`
- Reference: `inc/FreeRTOSConfig.h`
- Reference: `build/Debug/builder.params`
- Reference: `build/Debug/NewProject1.map`
- Reference: `WIRING_AND_SYSCONFIG.md`
- Reference: `tests/run_tests.ps1`

**Interfaces:**
- Consumes: the current generated hardware configuration, EIDE build snapshot, final linked-image evidence, and approved README design.
- Produces: `README.md` as the project entry point and mandatory module-extension checklist.

- [ ] **Step 1: Capture the documentation precondition**

Run from the project root:

```powershell
if (Test-Path -LiteralPath ".\README.md") {
    throw "README.md already exists; inspect before creating it"
}
if (-not (Test-Path -LiteralPath ".\WIRING_AND_SYSCONFIG.md")) {
    throw "Detailed wiring reference is missing"
}
if (-not (Test-Path -LiteralPath ".\NewProject1.syscfg")) {
    throw "Authoritative SysConfig input is missing"
}
```

Expected: exit `0`, proving this task creates a new entry document without replacing an existing README.

- [ ] **Step 2: Create the README header, status, and authoritative-file guide**

Create `README.md` with this opening structure and content:

```markdown
# MSPM0G3507 Controller Firmware

基于 MSPM0G3507、FreeRTOS、四电机驱动、四路正交编码器、ICM42688 和 MCP23017 的控制器固件。

当前仓库状态已完成 SysConfig 生成、主机测试、Arm Compiler 6 全量编译和链接验证，但尚未完成目标板烧录与硬件运行验收。

## 文档与配置权威

| 文件 | 职责 |
|---|---|
| `README.md` | 工程入口、配置摘要、构建步骤、已知限制和模块扩展门禁 |
| `NewProject1.syscfg` | 引脚、外设、时钟和 DMA 配置的唯一权威源 |
| `WIRING_AND_SYSCONFIG.md` | 完整接线、引脚和 IOMUX 参考 |
| `.eide/eide.yml` | EIDE 源文件、包含路径、排除项、工具链和预构建任务 |
| `Debug/ti_msp_dl_config.c/.h` | SysConfig 生成文件，禁止手工编辑 |

硬件配置变更必须先修改 `NewProject1.syscfg`，再使用固定版本 SysConfig 重新生成。不得手工修补 `Debug/ti_msp_dl_config.c` 或 `.h`。
```

- [ ] **Step 3: Add the current MCU, memory, RTOS, and task summary**

Append:

```markdown
## 当前平台

| 项目 | 配置 |
|---|---|
| MCU | TI MSPM0G3507，Arm Cortex-M0+，LQFP-48 |
| 主频 | 32 MHz SYSOSC/BUSCLK；HFXT 和 SYSPLL 关闭 |
| 辅助时钟 | 4 MHz MFCLK，为 TIMG12 提供 1 MHz 计数时钟 |
| Flash | 128 KiB；当前镜像 ROM 30,624 bytes |
| SRAM | 32 KiB；当前镜像 RW 10,240 bytes |
| RTOS | FreeRTOS Cortex-M0+ non-MPU port，1 kHz tick |
| Heap | `heap_4.c`，4 KiB 动态堆 |
| 应用任务 | `control`，优先级 4，栈 512 words，500 Hz 唤醒 |
| 编译器 | Arm Compiler 6，`C:\Keil_v5\ARM\ARMCLANG` |
| SDK | 仓库 `../../controller/documents/sdk`，2.10.00.04 |
| SysConfig | 1.26.2+4477，`C:\ti\sysconfig_1.26.2` |
```

- [ ] **Step 4: Add the current peripheral resource summary**

Append a summary table without duplicating every IOMUX detail:

```markdown
## 当前资源分配

| 资源 | 当前用途 |
|---|---|
| TIMG0 | 32 MHz，2 ms 周期，500 Hz 控制中断，运行时由 control 任务启动 |
| TIMG12 | 4 MHz MFCLK / 4，1 MHz，32-bit 最大周期，无中断，无自动启动 |
| TIMA0 | M1/M2/M4 PWM，PA21/PA22/PA25，32 kHz |
| TIMA1 | M3 PWM，PA24，32 kHz |
| 电机方向 GPIO | PB6、PB7、PB8、PA7、PA15、PB9、PB19、PB24，初始低 |
| 编码器 | PA12、PA13、PA2、PA26、PA27、PA28、PA31、PB18，双边沿中断 |
| I2C0 | PA0 SDA、PA1 SCL，100 kHz，MCP23017 等 3.3 V 设备共享 |
| SPI1 | PA16 POCI、PA18 PICO、PA17 SCLK，Mode 0，1 MHz |
| ICM42688 CS | PB20 软件片选，初始高 |
| DMA | SPI1 RX=CH2，TX=CH3 |
| UART0 | PA10/PA11，115200，调试串口 |
| UART1 | PA8/PA9，115200 |
| UART3 | PB2/PB3，115200，蓝牙预留 |
| 调试/复位 | SWDIO=PA19，SWCLK=PA20，NRST 保留 |
| PA23 | 当前未占用；TB6612 STBY 为外部 3.3 V 上拉 |

完整引脚、IOMUX 和接线要求见 [`WIRING_AND_SYSCONFIG.md`](WIRING_AND_SYSCONFIG.md)。
```

- [ ] **Step 5: Add reproducible test, generation, build, and programming commands**

Append:

````markdown
## 构建与烧录

### 主机测试

```powershell
powershell -ExecutionPolicy Bypass -File ".\tests\run_tests.ps1"
```

验收输出：`Host tests: PASS`。当前共有 7 个主机测试程序。

### SysConfig 生成

```powershell
& "C:\ti\sysconfig_1.26.2\sysconfig_cli.bat" `
  --product "..\..\controller\documents\sdk\.metadata\product.json" `
  --device MSPM0G3507 `
  --package "LQFP-48(PT)" `
  --script ".\NewProject1.syscfg" `
  --output ".\Debug" `
  --compiler keil
```

生成后必须审核所有既有定时器、PWM、编码器、I2C、SPI、DMA、UART、GPIO、SWD 和复位映射。

### EIDE/AC6 全量构建

先在 EIDE 项目树中打开 `NewProject1`，生成当前 `build/Debug/builder.params`，然后运行：

```powershell
$builder = "C:\Users\Joelin\.vscode\extensions\cl.eide-3.27.2\res\tools\win32\unify_builder\unify_builder.exe"
$params = (Resolve-Path -LiteralPath ".\build\Debug\builder.params").Path
& $builder --params-file $params --rebuild --no-color
```

输出文件：

- `build/Debug/NewProject1.axf`
- `build/Debug/NewProject1.hex`
- `build/Debug/NewProject1.map`

当前 HEX 文件约 84.16 KiB；实际烧入 Flash 的有效镜像为 30,624 bytes。

烧录工具选择 MSPM0G3507 后写入 `build/Debug/NewProject1.hex`。烧录和上电运行必须另行记录目标板、探针、供电、复位和功能测试结果。
````

- [ ] **Step 6: Add verified status and prominent runtime limitations**

Append:

```markdown
## 当前验证边界

已验证：

- SysConfig 1.26.2+4477 使用仓库 SDK 2.10.00.04 生成成功。
- 7 个主机测试程序全部通过。
- EIDE/AC6 全量 Rebuild、链接和 HEX/S19 输出成功。
- `portasm.o`、`vStartFirstTask`、`PendSV_Handler`、`SVC_Handler` 已链接。
- OLED、示例、主机测试和临时源文件未进入目标固件。
- ROM 和 RAM 均未超过 MSPM0G3507 限制。

尚未验证：

- 固件烧录、目标板启动和复位行为。
- 实际电机、编码器、I2C、SPI、DMA、UART、GPIO 和控制闭环行为。
- 电气连接、时钟精度和实时最坏执行时间。

## 已知运行限制

- TIMG12 已配置为 1 MHz 时间基准，但 `PlatformTime_Init()` 尚未启动它。
- `PlatformTime_GetUs32()` 当前恒定返回 `0`，因此 32/64 位时间戳还不是真实硬件时间。
- `ahrs_hal.o` 和 `icm42688_mspm0.o` 当前不可达并被链接器删除，不能据此宣称 AHRS 或该适配层已经运行。
- UART0/1/3 已初始化，但当前没有应用层通信任务、协议、缓冲和收发数据流。

“编译和链接通过”不等于“模块已在硬件上运行”。
```

- [ ] **Step 7: Add the module resource declaration and hard constraints**

Append:

```markdown
## 新增模块约束

任何新软件模块或硬件外设在实现前必须声明：

- 引脚、IOMUX、外设实例和通道。
- 时钟源、频率、分频、周期和精度要求。
- IRQ、优先级、最大中断延迟以及 ISR 到任务的数据流。
- DMA 通道、触发源、方向、位宽和唯一所有者。
- Flash、静态 RAM、heap、任务栈和大缓冲区预算。
- 调用周期、最坏执行时间、阻塞行为以及对 500 Hz 控制环的影响。
- 总线地址、模式、速率、片选所有权和 3.3 V 电气要求。
- 初始化顺序、错误传播和失效安全状态。

硬约束：

- 未经评审不得重映射现有引脚、外设、定时器通道、IRQ、DMA、SWD 或 NRST。
- 未提供时序和内存证据，不得修改 500 Hz 控制周期、FreeRTOS 中断优先级、heap、任务优先级或栈。
- 使用 PA23 前必须评审 TB6612 STBY 外部上拉和未来中断预留。
- 新 I2C 设备必须兼容 PA0/PA1、100 kHz、3.3 V，并使用不冲突地址。
- 新 SPI1 设备必须明确片选所有权，不得破坏 ICM42688 事务，也不得复用 DMA CH2/CH3。
- 新 UART 用户必须定义帧格式、协议、缓冲、任务/ISR 数据流，不能只依赖 SysConfig 初始化。
- 硬件资源只在 `NewProject1.syscfg` 中修改；生成文件禁止手工编辑。
- 新源文件和包含路径必须显式加入 `.eide/eide.yml`，并保持示例、测试和临时源排除。
- 源文件能够编译不代表模块已集成。最终 map 必须出现预期对象和符号；若允许链接器删除，必须明确记录原因。
```

- [ ] **Step 8: Add the mandatory module acceptance checklist**

Append:

```markdown
## 新模块验收清单

1. 完成资源申报和冲突检查。
2. 对可主机测试的逻辑先增加或更新测试。
3. 实现最小生产代码和安全失败路径。
4. 需要硬件资源时只修改 `NewProject1.syscfg`。
5. 使用 SysConfig 1.26.2+4477 和 SDK 2.10.00.04 重新生成。
6. 逐项审核所有既有映射，不只检查新模块。
7. 更新 EIDE 源/包含路径并复查排除项。
8. 使用 AC6 独立编译受影响的翻译单元，不使用临时宏或强制包含规避错误。
9. 运行完整主机测试。
10. 重新生成 `builder.params` 并执行完整 AC6 Rebuild。
11. 检查 map 中预期对象/符号和禁止源文件。
12. 记录精确 ROM/RAM 增量并确认未超限。
13. 单独完成烧录和硬件运行测试。

出现引脚或外设映射变化、资源冲突、预期代码被链接器删除、内存超限、IRQ/任务优先级变化或控制周期变化时，必须停止并评审。
```

- [ ] **Step 9: Verify README facts and links mechanically**

Run:

```powershell
$readme = Get-Content -Raw -LiteralPath ".\README.md"
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
    ".\NewProject1.syscfg",
    ".\WIRING_AND_SYSCONFIG.md",
    ".\.eide\eide.yml",
    ".\Debug\ti_msp_dl_config.c",
    ".\Debug\ti_msp_dl_config.h"
)) {
    if (-not (Test-Path -LiteralPath $path)) { throw "README reference is missing: $path" }
}
```

Expected: exit `0`.

- [ ] **Step 10: Review documentation-only scope and Markdown hygiene**

Run:

```powershell
git diff -- README.md
git diff --check -- README.md
git status --short -- README.md `
  docs/superpowers/specs/2026-07-18-readme-and-module-constraints-design.md `
  docs/superpowers/plans/2026-07-18-readme-and-module-constraints.md
```

Expected:

- `README.md` is the only implementation file added.
- The approved design and this plan remain as documentation artifacts.
- `git diff --check` exits `0`.
- No firmware source, SysConfig input, generated file, build configuration,
  test, or `pictures/` file changes because of this task.

## Plan Self-Review

- Spec coverage: README structure, authority boundaries, current configuration,
  build/programming commands, verification boundary, known runtime limitations,
  software/hardware resource constraints, and the complete extension gate all
  map to explicit steps above.
- Placeholder scan: no TBD, TODO, unspecified error handling, or omitted command
  remains in this plan.
- Consistency: all versions, paths, memory values, timer assignments, test count,
  and module names match the approved design and current project evidence.
- Scope: one documentation deliverable; no firmware or generated-file changes.
