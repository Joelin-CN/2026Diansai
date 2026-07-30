# 修复日志：UART5 调试串口 printf 在 GCC 工具链下无输出

- **模块**：调试串口输出 / uart_debug
- **文件**：`Core/Src/app/uart_debug.c`、`Core/Src/main.c`、`Core/Src/freertos.c`
- **芯片**：STM32F407VGT6
- **日期**：2026-07-29
- **状态**：✅ 已修复

---

## 问题描述

UART5 调试串口接线和波特率设置均正确（115200, 8N1，PC12 TX / PD2 RX），但上位机串口助手始终收不到任何数据。

---

## 根本原因

`uart_debug.c` 只实现了 `fputc()` 作为 printf 重定向出口：

```c
int fputc(int ch, FILE *f) {
    HAL_UART_Transmit(&huart5, &c, 1, HAL_MAX_DELAY);
    return ch;
}
```

`fputc()` 是 **Keil MDK + MicroLIB** 的 printf 挂钩路径。

本工程使用 **VSCode + ARM GCC + newlib-nano** 工具链。GCC/newlib 的 printf 内部调用的是 `_write()` 系统调用，**不经过 `fputc()`**。未实现 `_write()` 时，newlib 使用默认的 stub，直接返回 -1 并丢弃所有输出，不报任何错误。

| 工具链 | printf 调用路径 |
|--------|----------------|
| Keil MDK (MicroLIB) | `printf` → `fputc()` |
| GCC / newlib | `printf` → `fwrite` → `_write()` |

两条路径互不重叠，原代码只覆盖了 Keil 路径，GCC 路径完全未实现。

---

## 修复内容

### `Core/Src/app/uart_debug.c`

新增 `_write()` 系统调用实现，覆盖 GCC/newlib 路径；保留 `fputc()` 以兼容 Keil：

```c
/* ---- Keil MicroLIB 重定向 ---- */
int fputc(int ch, FILE *f) {
    (void)f;
    uint8_t c = (uint8_t)ch;
    HAL_UART_Transmit(&huart5, &c, 1, HAL_MAX_DELAY);
    return ch;
}

/* ---- GCC / newlib 重定向（VSCode、STM32CubeIDE） ---- */
int _write(int fd, char *ptr, int len) {
    (void)fd;
    HAL_UART_Transmit(&huart5, (uint8_t *)ptr, (uint16_t)len, HAL_MAX_DELAY);
    return len;
}
```

`_write()` 接受整块字符串，比 `fputc()` 逐字节发送效率更高。

### `Core/Src/main.c`（USER CODE BEGIN 2）

在外设 init 完成后、RTOS 启动前加启动横幅，用于验证 UART5 物理链路在 RTOS 介入之前就已正常：

```c
UartDebug_Init();
printf("[UART5] Debug UART initialized @115200, PC12(TX) PD2(RX)\r\n");
printf("[SYSTEM] Entering FreeRTOS scheduler...\r\n");
```

### `Core/Src/freertos.c`（StartDefaultTask）

在任务入口和循环中加诊断输出，便于区分 UART 故障和任务调度/BLE 初始化故障：

```c
printf("[TASK] defaultTask started\r\n");
if (ATK_BLE02_Start() != HAL_OK) {
    printf("[ERROR] ATK_BLE02_Start failed\r\n");
    Error_Handler();
}
printf("[BLE] ATK_BLE02_Start OK\r\n");

uint32_t heartbeat = 0;
for(;;) {
    osDelay(1);
    heartbeat++;
    if (heartbeat % 1000 == 0)
        printf("[HB] %lu s alive\r\n", heartbeat / 1000);
}
```

---

## 变更说明

| 文件 | 变更 | 说明 |
|------|------|------|
| `uart_debug.c` | 新增 `_write()` | GCC/newlib printf 重定向，核心修复 |
| `uart_debug.c` | 保留 `fputc()` | 兼容 Keil MicroLIB，双工具链均可用 |
| `main.c` | 新增启动横幅 | RTOS 前验证 UART5 物理链路 |
| `freertos.c` | 新增任务日志 + 1s 心跳 | 辅助区分 UART / 任务 / BLE 故障层 |

---

## 注意事项

若编译报 `multiple definition of '_write'`，说明工程中已有 `syscalls.c` 含默认 stub，将其中的 `_write` 函数体替换为上方的 `HAL_UART_Transmit` 实现即可，删除 `uart_debug.c` 里的版本以避免重复定义。

---

## 环境信息

- IDE：VSCode + EIDE 扩展
- 工具链：ARM GCC (newlib-nano)
- 烧录：ST-Link (SWD)
- 上位机：串口助手，115200 baud

---

**修复人员**：Claude Code  
**修复日期**：2026-07-29  
**影响范围**：调试输出模块，不影响任何业务逻辑  
**风险评估**：低
