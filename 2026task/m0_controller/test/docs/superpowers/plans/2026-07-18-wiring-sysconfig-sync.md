# Wiring and SysConfig Documentation Sync Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Synchronize the detailed wiring guide with the current SysConfig tool pair, clock/timer allocation, DMA ownership, OLED status, and verification boundary without changing existing pin assignments.

**Architecture:** Keep `WIRING_AND_SYSCONFIG.md` focused on detailed physical wiring, IOMUX, clocks, timers, DMA ownership, and electrical constraints. `NewProject1.syscfg` remains the hardware authority, while README retains build instructions and module-extension gates.

**Tech Stack:** Markdown, MSPM0G3507, TI SysConfig 1.26.2+4477, MSPM0 SDK 2.10.00.04, generated DriverLib configuration.

## Global Constraints

- Modify only `WIRING_AND_SYSCONFIG.md` as the implementation file.
- Treat `NewProject1.syscfg` as the sole authoritative hardware and peripheral configuration.
- Do not edit `Debug/ti_msp_dl_config.c` or `Debug/ti_msp_dl_config.h`; they are generated files.
- Keep README as the project entry point for build instructions, runtime limitations, and module-extension gates.
- Preserve every existing pin, connector, IOMUX, motor, encoder, SWD, reset, correction, and MCP23017 table entry unless wording must change to clarify OLED or DMA status.
- Use the fixed current pair SysConfig 1.26.2+4477 and repository SDK 2.10.00.04.
- Do not claim flashing, boot, electrical, timing, or peripheral runtime verification.
- Do not modify firmware behavior, SysConfig input, generated output, build configuration, tests, build artifacts, README, or `pictures/`.
- Do not commit unless the user explicitly authorizes a commit during execution.

---

### Task 1: Synchronize the Detailed Wiring Guide

**Files:**
- Modify: `WIRING_AND_SYSCONFIG.md:1-117`
- Reference: `docs/superpowers/specs/2026-07-18-wiring-sysconfig-sync-design.md`
- Reference: `README.md`
- Reference: `NewProject1.syscfg:14-205`
- Reference: `Debug/ti_msp_dl_config.h:70-329`
- Reference: `Debug/ti_msp_dl_config.c:276-680`
- Reference: `src/main.c:13-104`
- Reference: `src/platform_time.c:34-76`
- Reference: `build/Debug/NewProject1.map`

**Interfaces:**
- Consumes: current hardware authority, generated mappings, runtime source, and approved documentation boundaries.
- Produces: an accurate detailed wiring and SysConfig reference linked by README.

- [ ] **Step 1: Record the stale-document precondition**

Run:

```powershell
$text = Get-Content -Raw -Encoding UTF8 -LiteralPath ".\WIRING_AND_SYSCONFIG.md"
if ($text -notmatch 'SysConfig 1\.24 / SDK 2\.05') {
    throw "Expected stale tool-version statement was not found"
}
if ($text -match 'TIMG12') {
    throw "Expected TIMG12 documentation to be absent before synchronization"
}
```

Expected: exit `0`, proving the task addresses an observable documentation gap.

- [ ] **Step 2: Replace the introduction with current authority and verification status**

Replace the paragraph after the title with:

```markdown
本表记录当前 `NewProject1.syscfg` 的详细物理接线与 IOMUX 分配。硬件配置以 `NewProject1.syscfg` 为唯一权威源；`Debug/ti_msp_dl_config.c` 和 `.h` 由 SysConfig 生成，禁止手工编辑。

当前固定工具链为 SysConfig 1.26.2+4477 和仓库 SDK 2.10.00.04。生成文件已重新生成，并完成定时器、PWM、编码器、I2C、SPI、DMA、UART、GPIO、SWD 和复位映射审核。尚未完成目标板烧录、电气连接和硬件运行验证。

工程构建、当前运行限制和新增模块门禁见 [`README.md`](README.md)。配套总览图：[MSPM0G3507_PINOUT_SUMMARY.png](docs/MSPM0G3507_PINOUT_SUMMARY.png)。
```

- [ ] **Step 3: Add the clock and timer allocation section**

Insert before `## 通信接口`:

```markdown
## 系统时钟与定时器

| 资源 | 时钟与参数 | 当前用途和运行状态 |
|---|---|---|
| CPU / SYSOSC / BUSCLK | 32 MHz；HFXT 和 SYSPLL 关闭 | CPU、PWM、TIMG0、UART、SPI、I2C 主时钟 |
| MFCLK | 固定 4 MHz | 已开启，仅用于 TIMG12 时间基准 |
| TIMG0 / CONTROL_TIMER | BUSCLK 32 MHz，周期 2 ms，load 63999，周期模式，ZERO 中断 | 500 Hz 控制节拍；SysConfig 初始化后保持停止，由 `control` 任务启动 |
| TIMG12 / ICM42688_TIMER | MFCLK / 4，1 MHz，load 4294967295，周期模式，无中断，无自动启动 | 预留微秒时间基准；当前应用尚未启动或读取该定时器 |

当前 `PlatformTime_GetUs32()` 仍返回 `0`，所以 TIMG12 的 1 MHz 配置不能视为已经接入应用时间戳。时钟与定时器只完成生成和链接验证，尚未进行目标板频率与周期测量。
```

- [ ] **Step 4: Clarify I2C/OLED target-firmware status**

Change the two I2C table notes to:

```markdown
| I2C0_SDA | PA0 | H1-1 | PINCM1 / I2C0_SDA | 100 kHz；MCP23017 使用，可在硬件上与 3.3 V OLED 共用；OLED 源码当前未链接 |
| I2C0_SCL | PA1 | H1-2 | PINCM2 / I2C0_SCL | 100 kHz；共享设备必须使用 3.3 V 并避免地址冲突 |
```

In the MCP23017 table, replace its SDA/SCL row with:

```markdown
| SDA / SCL | PA0 / PA1，使用 I2C0；硬件可与 3.3 V OLED 共用，但 OLED 当前未进入目标固件 |
```

- [ ] **Step 5: Add SPI1 DMA ownership immediately after its table**

Append below the existing SPI DMA table:

```markdown
DMA CH2/CH3 由统一 SPI1 传输层独占管理。同一 SPI1 总线上的新设备可以使用该传输服务，但不得独立重配置、抢占、并发驱动这些通道，也不得建立其他 DMA 所有者。所有设备仍必须使用独立片选，并保持 ICM42688 的完整 CS 低电平事务。
```

- [ ] **Step 6: Add the document verification boundary**

Append at the end:

```markdown
## 当前验证边界

已验证：

- SysConfig 1.26.2+4477 使用仓库 SDK 2.10.00.04 生成成功。
- 当前生成文件包含 TIMG0、TIMG12 以及本文档列出的全部外设和 GPIO 映射。
- 生成前后的既有物理映射已逐项审核，没有发现引脚或外设重映射。
- EIDE/AC6 全量编译和链接成功。

尚未验证：

- HEX 烧录、目标板启动和复位行为。
- 实际时钟频率、TIMG0 周期、TIMG12 微秒计数精度。
- 电机、编码器、I2C、SPI、DMA、UART、GPIO 和控制闭环硬件行为。
- 接线、电平、EMI、电源完整性和实时最坏执行时间。

本文档描述的是当前配置与接线要求，不是硬件运行合格证明。
```

- [ ] **Step 7: Verify current statements mechanically**

Run:

```powershell
$text = Get-Content -Raw -Encoding UTF8 -LiteralPath ".\WIRING_AND_SYSCONFIG.md"
$required = @(
    'SysConfig 1.26.2+4477',
    'SDK 2.10.00.04',
    'NewProject1.syscfg',
    'README.md',
    'TIMG0',
    '63999',
    'TIMG12',
    '4294967295',
    'MFCLK / 4',
    'PlatformTime_GetUs32',
    'DMA CH2/CH3',
    '统一 SPI1 传输层',
    'OLED 源码当前未链接',
    '当前验证边界',
    '尚未完成目标板烧录'
)
foreach ($token in $required) {
    if ($text -notlike "*$token*") { throw "Wiring guide missing: $token" }
}
if ($text -match 'SysConfig 1\.24 / SDK 2\.05') {
    throw "Stale tool-version statement remains"
}
```

Expected: exit `0`.

- [ ] **Step 8: Verify all existing physical mapping tokens remain present**

Run:

```powershell
$text = Get-Content -Raw -Encoding UTF8 -LiteralPath ".\WIRING_AND_SYSCONFIG.md"
$mappingTokens = @(
    'PB20', 'PA16', 'PA18', 'PA17', 'PA0', 'PA1',
    'PA10', 'PA11', 'PA8', 'PA9', 'PB2', 'PB3',
    'PA21', 'PA22', 'PA24', 'PA25',
    'PB6', 'PB7', 'PB8', 'PA7', 'PA15', 'PB9', 'PB19', 'PB24',
    'PA12', 'PA13', 'PA2', 'PA26', 'PA27', 'PA28', 'PA31', 'PB18',
    'PA19', 'PA20', 'NRST', 'PA23',
    'PINCM1', 'PINCM2', 'PINCM3', 'PINCM6', 'PINCM7',
    'PINCM14', 'PINCM15', 'PINCM16', 'PINCM19', 'PINCM20',
    'PINCM21', 'PINCM22', 'PINCM23', 'PINCM24', 'PINCM25',
    'PINCM26', 'PINCM34', 'PINCM35', 'PINCM37', 'PINCM38',
    'PINCM39', 'PINCM40', 'PINCM44', 'PINCM45', 'PINCM46',
    'PINCM47', 'PINCM48', 'PINCM52', 'PINCM54', 'PINCM55',
    'PINCM59', 'PINCM60'
)
foreach ($token in $mappingTokens) {
    if ($text -notlike "*$token*") { throw "Existing mapping token missing: $token" }
}
```

Expected: exit `0`; no existing physical mapping disappeared from the guide.

- [ ] **Step 9: Review documentation-only scope and whitespace**

Run:

```powershell
git diff -- WIRING_AND_SYSCONFIG.md
git diff --check -- WIRING_AND_SYSCONFIG.md
git status --short -- WIRING_AND_SYSCONFIG.md README.md NewProject1.syscfg `
  Debug/ti_msp_dl_config.c Debug/ti_msp_dl_config.h .eide/eide.yml
```

Expected:

- Only `WIRING_AND_SYSCONFIG.md` changes during this implementation task.
- README and existing firmware repair changes remain untouched.
- `git diff --check` exits `0`.
- No build, test, generation, firmware, or `pictures/` file is modified.

## Plan Self-Review

- Spec coverage: current versions, authority boundaries, timer/clock allocation,
  DMA ownership, OLED status, mapping preservation, and verification boundary
  all map to explicit steps.
- Placeholder scan: no TBD, TODO, omitted text, or unspecified command remains.
- Consistency: frequencies, periods, loads, paths, versions, DMA channels, and
  runtime limitations match current configuration evidence.
- Scope: one existing documentation file changes; no firmware or generated-file
  work is included.
