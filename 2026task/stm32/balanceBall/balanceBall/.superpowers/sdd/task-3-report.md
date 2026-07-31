# Task 3 Report: main.c Integration

**Date:** 2026-07-31
**Status:** DONE_WITH_CONCERNS

---

## What Was Implemented

Three modifications to `Core/Src/main.c`, all inside USER CODE blocks:

### Step 1 — Include and global variable
- Added `#include "debug_cli.h"` inside `/* USER CODE BEGIN Includes */`
- Added `DebugCli g_debug_cli;` inside `/* USER CODE BEGIN PV */`

### Step 2 — CLI init and DMA start (USER CODE BEGIN 2)
- Added `extern DMA_HandleTypeDef hdma_usart3_rx;` declaration (not in dma.h/usart.h)
- Added `debug_cli_init(&g_debug_cli, &huart3);`
- Added `HAL_UARTEx_ReceiveToIdle_DMA(&huart3, g_debug_cli.rx_buf, DEBUG_CLI_RX_BUF);`
- Added `__HAL_DMA_DISABLE_IT(&hdma_usart3_rx, DMA_IT_HT);`

### Step 3 — UART3 branch in HAL_UARTEx_RxEventCallback (USER CODE BEGIN 4)
- Added `else if (huart->Instance == USART3)` branch
- Calls `debug_cli_process_line(...)`, then restarts DMA, disables HT interrupt

---

## Build Results

The CMake + mingw32-make build fails on **pre-existing toolchain misconfiguration** —
`mingw32-make` is using the host x86 assembler instead of the ARM toolchain for `startup_stm32f103xb.s`,
producing errors like `no such instruction: isb 0xF`. This is **not caused by Task 3 changes**.

**Syntax-only verification with the correct cross-compiler:**
```
arm-none-eabi-gcc -c -mcpu=cortex-m3 -mthumb -std=gnu11 [all include paths] -fsyntax-only Core/Src/main.c
```
Result: **0 errors, 0 warnings.**

---

## Files Changed

| File | Change |
|------|--------|
| `Core/Src/main.c` | +16 lines — 3 integration steps as specified |

---

## Self-Review Checklist

- [x] `#include "debug_cli.h"` is inside `/* USER CODE BEGIN Includes */` (line 29)
- [x] `DebugCli g_debug_cli;` is inside `/* USER CODE BEGIN PV */` (line 52)
- [x] `debug_cli_init` and `HAL_UARTEx_ReceiveToIdle_DMA` are inside `/* USER CODE BEGIN 2 */` (lines 115-117)
- [x] USART3 branch in `HAL_UARTEx_RxEventCallback` restarts DMA after processing (lines 207-210)
- [x] `hdma_usart3_rx` extern declaration present in both locations it is used
- [ ] Full CMake build: **cannot verify** — pre-existing toolchain issue in the build env

---

## Concerns

1. **`hdma_usart3_rx` extern placement:** The DMA handle is not declared in `dma.h` or `usart.h`.
   I used `extern DMA_HandleTypeDef hdma_usart3_rx;` locally inside the two USER CODE blocks where
   it is needed. This is consistent with how `stm32f1xx_it.c` already uses it (line 62 of that file).
   No functional issue, but it would be cleaner if `dma.h` exported the extern declaration.

2. **CMake build environment:** The project's `cmake --build build` command uses mingw32-make with
    the host assembler and cannot link an ARM firmware. This was present before Task 3. A proper build
    requires either: configuring CMake with `-DCMAKE_TOOLCHAIN_FILE=...` pointing to the ARM toolchain,
    or building via Keil MDK / STM32CubeIDE. Task-level verification was done with
    `arm-none-eabi-gcc -fsyntax-only` on the actual cross-compiler, which passed clean.

---

## Fix Round — 2026-07-31 (I1 + I2 from final code review)

### I1: vsnprintf buffer overread in `debug_cli_printf` (App/Src/debug_cli.c:30)

**Root cause:** `vsnprintf` returns the number of chars that *would* be written if the buffer were
unlimited. When `len >= sizeof(buf)` the call already truncated at 255 chars but `HAL_UART_Transmit`
was passed the unclamped `len`, causing a buffer overread.

**Fix applied (debug_cli.c:29–32):**
```c
if (len > 0) {
    uint16_t tx_len = (len >= (int)sizeof(buf)) ? (uint16_t)(sizeof(buf) - 1U) : (uint16_t)len;
    HAL_UART_Transmit(cli->uart, (uint8_t *)buf, tx_len, 100U);
}
```

### I2: UART3 DMA reception not restarted after error (Core/Src/main.c:213–225)

**Root cause:** `HAL_UART_ErrorCallback` only handled USART2. A framing/overrun on USART3 stopped
DMA reception permanently until reset.

**Fix applied (main.c:219–225):** Added `else if (USART3)` branch that re-arms DMA with
`HAL_UARTEx_ReceiveToIdle_DMA` and disables the half-transfer interrupt. Also added the required
`extern DMA_HandleTypeDef hdma_usart3_rx;` local declaration (consistent with existing usage in
`stm32f1xx_it.c:62` and `HAL_UARTEx_RxEventCallback`).

### Build

Built with `cmake -DCMAKE_TOOLCHAIN_FILE=cmake/gcc-arm-none-eabi.cmake` + `cmake --build`:
```
[100%] Built target balanceBall
RAM: 2968 B / 20 KB (14.49%)   FLASH: 18784 B / 64 KB (28.66%)
0 errors, 0 warnings in changed files
```

### Commit

`504337b` — fix: clamp vsnprintf output length; restart UART3 DMA on error
