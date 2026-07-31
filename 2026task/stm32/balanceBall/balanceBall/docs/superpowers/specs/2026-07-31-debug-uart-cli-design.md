# Debug UART CLI Design - 2026-07-31

## Executive Summary

为 STM32 平衡球项目添加基于 UART3 的调试命令行接口（CLI），允许通过串口助手发送文本指令来控制和测试步进电机，无需修改和烧录代码即可快速验证电机通信和动作。

## Background

**Current State:**
- 项目已实现完整的 EMM V5 步进电机驱动栈（协议层 + UART DMA 事务层 + 电机适配层）
- UART2 (PA2/PA3) 用于与 EMM V5 驱动器通信
- UART3 (PB10/PB11) 已配置但未使用
- 现有的测试代码都是单元测试，需要修改代码+编译+烧录的循环

**Problem:**
- 硬件在环测试需要频繁修改 `main.c` 中的测试代码
- 无法在运行时动态调整测试参数（位置、速度、加速度）
- 调试电机通信问题时缺乏实时交互能力
- 无法快速验证"UART2 线路是否正常"、"驱动器地址是否正确"等基础问题

**Goal:**
通过 UART3 提供人类可读的文本命令接口，支持实时控制电机、查询状态、调整参数，加速硬件调试流程。

---

## System Architecture

### High-Level Data Flow

```
┌─────────────────┐
│  PC 串口助手     │
│  (115200 8N1)   │
└────────┬────────┘
         │ ASCII 文本 (换行结尾)
         ↓
┌─────────────────┐
│   UART3 RX      │ PB11 ← DMA + 空闲中断
│   (STM32)       │
└────────┬────────┘
         │
         ↓
┌─────────────────┐
│  debug_cli.c    │  命令解析 + 参数提取
│  Command Parser │
└────────┬────────┘
         │
         ↓
┌─────────────────┐
│ balance_motor   │  现有 API 调用
│ emm_v5_uart     │
└────────┬────────┘
         │ EMM V5 二进制协议
         ↓
┌─────────────────┐
│   UART2 TX/RX   │ PA2/PA3 ↔ 驱动器
└─────────────────┘
         │
         ↓ 响应数据
┌─────────────────┐
│  debug_cli.c    │  格式化响应文本
│  Response       │
└────────┬────────┘
         │
         ↓
┌─────────────────┐
│   UART3 TX      │ PB10 → PC
└─────────────────┘
```

### Component Boundaries

| 组件 | 职责 | 依赖 |
|------|------|------|
| **debug_cli** | UART3 收发、命令解析、响应格式化 | `balance_motor`, `emm_v5_uart`, HAL UART |
| **balance_motor** | 电机控制逻辑（已存在，不修改） | `emm_v5_uart` |
| **emm_v5_uart** | UART2 DMA 事务层（已存在，不修改） | HAL UART, HAL DMA |

---

## Command Set Specification

### Command Format

**通用格式：** `<command> [arg1] [arg2] ...\n`

- 命令和参数用空格分隔
- 命令不区分大小写（内部转为小写匹配）
- 换行符 `\n` 或 `\r\n` 作为命令结束标志
- 最大命令长度：128 字节

### Supported Commands

#### 1. `en` - 使能电机

**格式：** `en`

**功能：** 发送 EMM V5 使能命令（功能码 0xF3，参数 0x01）

**响应：**
- 成功：`OK motor enabled`
- 失败：`ERR: <原因>`

**实现：** 调用 `emm_v5_encode_enable(address, true, false, &frame)` + `emm_v5_uart_send()`

---

#### 2. `dis` - 失能电机

**格式：** `dis`

**功能：** 发送 EMM V5 失能命令（功能码 0xF3，参数 0x00）

**响应：**
- 成功：`OK motor disabled`
- 失败：`ERR: <原因>`

**实现：** 调用 `balance_motor_disable(&g_balance_motor)`

---

#### 3. `pos` - 绝对位置控制

**格式：** `pos <脉冲数> [速度rpm] [加速度]`

**参数：**
- `<脉冲数>`：目标绝对位置（int32_t，范围 -2147483648 ~ 2147483647）
- `[速度rpm]`：可选，默认 100 rpm（uint16_t，范围 0 ~ 3000）
- `[加速度]`：可选，默认 10（uint8_t，范围 0 ~ 255）

**示例：**
- `pos 1000` → 移动到位置 1000，速度 100 rpm，加速度 10
- `pos 500 50` → 移动到位置 500，速度 50 rpm，加速度 10
- `pos -2000 200 20` → 移动到位置 -2000，速度 200 rpm，加速度 20

**响应：**
- 成功：`OK moving to <脉冲数> at <速度>rpm`
- 失败：`ERR: motor locked` / `ERR: invalid argument`

**实现：**
```c
BalanceActuatorCommand cmd = {
    .position = parsed_position,
    .velocity = parsed_speed, // 默认 100
    .dt = 0.01f // 固定 10ms 控制周期
};
balance_motor_submit(&g_balance_motor, &cmd);
```

**注意：** `balance_motor` 使用软件零点机制，首次使用前需先执行 `zero` 命令。

---

#### 4. `vel` - 速度模式

**格式：** `vel <rpm> [加速度]`

**参数：**
- `<rpm>`：目标速度（int16_t，正数=顺时针，负数=逆时针，范围 -3000 ~ 3000）
- `[加速度]`：可选，默认 10

**示例：**
- `vel 200` → 以 200 rpm 顺时针连续转动
- `vel -150` → 以 150 rpm 逆时针连续转动

**响应：**
- 成功：`OK velocity <rpm>rpm`
- 失败：`ERR: <原因>`

**实现：** 调用 `emm_v5_encode_velocity(address, direction, abs(rpm), acc, false, &frame)`

---

#### 5. `stop` - 急停

**格式：** `stop`

**功能：** 立即停止电机运动（不是失能，保持使能状态）

**响应：**
- 成功：`OK stopped`
- 失败：`ERR: <原因>`

**实现：** 调用 `balance_motor_stop(&g_balance_motor)`

---

#### 6. `query` - 查询当前位置

**格式：** `query`

**功能：** 发送 EMM V5 位置查询命令（功能码 0x36），读取驱动器当前绝对位置

**响应：**
- 成功：`OK pos=<脉冲数>` （如 `OK pos=1234`）
- 失败：`ERR: timeout` / `ERR: invalid response`

**实现：** 调用 `emm_v5_encode_position_query(address, &frame)` + 等待响应 + `emm_v5_parse_position()`

---

#### 7. `zero` - 设置软件零点

**格式：** `zero`

**功能：** 读取驱动器当前位置并记录为软件零点（用于相对位置控制）

**响应：**
- 成功：`OK zero set at <脉冲数>` （如 `OK zero set at -542`）
- 失败：`ERR: query failed`

**实现：** 调用 `balance_motor_request_zero(&g_balance_motor)`

**注意：** 这是 `balance_motor` 必需的初始化步骤，必须在使用 `pos` 命令前执行。

---

#### 8. `status` - 查看电机状态

**格式：** `status`

**功能：** 显示 `balance_motor` 内部状态

**响应：**
```
OK status
  zero_valid: true/false
  zero_position: <脉冲数>
  locked: true/false
  consecutive_failures: <次数>
  uart_state: IDLE/ACTIVE/COMPLETE/TIMEOUT/ERROR
```

**实现：** 读取 `g_balance_motor` 和 `g_emm_uart` 的状态字段并格式化输出

---

#### 9. `help` - 显示帮助信息

**格式：** `help`

**响应：**
```
Available commands:
  en                - Enable motor
  dis               - Disable motor
  pos <p> [v] [a]   - Move to position p (pulses), v=speed(rpm), a=accel
  vel <rpm> [a]     - Velocity mode (+ = CW, - = CCW)
  stop              - Emergency stop
  query             - Query current position
  zero              - Set current position as zero
  status            - Show motor status
  help              - Show this message
```

---

## Data Structures

### debug_cli.h

```c
#ifndef DEBUG_CLI_H
#define DEBUG_CLI_H

#include <stdint.h>
#include <stdbool.h>
#include "usart.h"

#define DEBUG_CLI_RX_BUFFER_SIZE 128U
#define DEBUG_CLI_TX_BUFFER_SIZE 256U

typedef enum {
    DEBUG_CLI_OK = 0,
    DEBUG_CLI_UNKNOWN_COMMAND,
    DEBUG_CLI_INVALID_ARGUMENT,
    DEBUG_CLI_MOTOR_ERROR,
    DEBUG_CLI_TIMEOUT
} DebugCliResult;

typedef struct {
    UART_HandleTypeDef *uart;
    uint8_t rx_buffer[DEBUG_CLI_RX_BUFFER_SIZE];
    uint16_t rx_index;
    bool echo_enabled; // 可选：回显输入字符
} DebugCli;

void debug_cli_init(DebugCli *cli, UART_HandleTypeDef *uart);
void debug_cli_process_line(DebugCli *cli, const char *line);
void debug_cli_on_rx_char(DebugCli *cli, uint8_t ch);
void debug_cli_printf(DebugCli *cli, const char *format, ...);

#endif
```

### Command Parser Structure

```c
typedef struct {
    const char *name;
    void (*handler)(DebugCli *cli, int argc, char **argv);
    const char *help_text;
} DebugCliCommand;

static const DebugCliCommand commands[] = {
    {"en",     cmd_enable,  "Enable motor"},
    {"dis",    cmd_disable, "Disable motor"},
    {"pos",    cmd_position, "Move to position"},
    {"vel",    cmd_velocity, "Velocity mode"},
    {"stop",   cmd_stop,    "Emergency stop"},
    {"query",  cmd_query,   "Query position"},
    {"zero",   cmd_zero,    "Set zero point"},
    {"status", cmd_status,  "Show status"},
    {"help",   cmd_help,    "Show help"},
    {NULL, NULL, NULL} // 结束标记
};
```

---

## Implementation Details

### UART3 Reception Strategy

**Option A: DMA + Idle Interrupt (Recommended)**

- 使用 `HAL_UARTEx_ReceiveToIdle_DMA(&huart3, rx_buffer, sizeof(rx_buffer))`
- 每收到换行符或空闲超时，触发 `HAL_UARTEx_RxEventCallback`
- 在回调中调用 `debug_cli_process_line()`
- 优点：高效、无需轮询、支持变长命令
- 缺点：需要处理 DMA 循环缓冲（不过我们用换行符切割，无此问题）

**Option B: Interrupt per Byte**

- 使用 `HAL_UART_Receive_IT(&huart3, &rx_char, 1)`
- 每收到一个字节触发中断
- 缓存到内部缓冲区，遇到换行符时解析
- 优点：简单直接
- 缺点：中断频率高（115200 baud → ~11520 字符/秒 → ~11.5kHz 中断）

**选择：Option A（DMA + Idle），匹配项目现有的 UART2 DMA 风格。**

---

### UART3 Transmission Strategy

**阻塞式发送（Recommended for Debug CLI）**

```c
void debug_cli_printf(DebugCli *cli, const char *format, ...) {
    char buffer[DEBUG_CLI_TX_BUFFER_SIZE];
    va_list args;
    va_start(args, format);
    int len = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    HAL_UART_Transmit(cli->uart, (uint8_t*)buffer, len, 100); // 100ms timeout
}
```

**理由：**
- 调试接口不追求极致性能
- 阻塞发送保证响应顺序（避免 DMA 队列管理复杂度）
- 人类交互速度慢（~1 命令/秒），100ms 阻塞可接受

---

### Command Parsing Algorithm

```c
void debug_cli_process_line(DebugCli *cli, const char *line) {
    // 1. Trim leading/trailing whitespace
    // 2. Split by space → argc, argv[]
    // 3. Convert argv[0] to lowercase
    // 4. Linear search in commands[] table
    // 5. Call handler(cli, argc, argv)
    // 6. Handler sends response via debug_cli_printf()
}
```

**示例：** `"pos 1000 200\n"` →
- argc = 3
- argv = ["pos", "1000", "200"]
- 调用 `cmd_position(cli, 3, argv)`
- `cmd_position` 内部：
  - `atoi(argv[1])` → 1000
  - `atoi(argv[2])` → 200
  - 调用 `balance_motor_submit(...)`
  - `debug_cli_printf(cli, "OK moving to %d at %drpm\n", 1000, 200)`

---

### Error Handling

**输入错误：**
- 未知命令 → `ERR: unknown command '<cmd>'`
- 参数不足 → `ERR: '<cmd>' requires <n> arguments`
- 参数类型错误 → `ERR: invalid argument '<arg>'`
- 参数超出范围 → `ERR: argument out of range`

**电机错误：**
- 未设置零点 → `ERR: zero not set (run 'zero' first)`
- 电机锁定 → `ERR: motor locked (run 'status' for details)`
- 通信超时 → `ERR: motor timeout`
- 驱动器返回错误 ACK → `ERR: motor NAK: <ack_code>`

**UART 错误：**
- 接收溢出 → 丢弃当前缓冲，发送 `ERR: rx buffer overflow`
- 帧错误/噪声 → 忽略当前字节，继续接收

---

## Integration Points

### main.c Modifications

**在 `USER CODE BEGIN PV` 区：**
```c
DebugCli g_debug_cli;
```

**在 `USER CODE BEGIN 2` 区：**
```c
debug_cli_init(&g_debug_cli, &huart3);
HAL_UARTEx_ReceiveToIdle_DMA(&huart3, g_debug_cli.rx_buffer, DEBUG_CLI_RX_BUFFER_SIZE);
__HAL_DMA_DISABLE_IT(&hdma_usart3_rx, DMA_IT_HT); // 禁用半传输中断（不需要）
```

**在 `USER CODE BEGIN 4` 区（添加回调）：**
```c
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t size) {
    if (huart->Instance == USART2) {
        emm_v5_uart_on_rx_event(&g_emm_uart, size);
    } else if (huart->Instance == USART3) {
        // 处理 debug CLI 接收
        g_debug_cli.rx_buffer[size] = '\0'; // Null-terminate
        debug_cli_process_line(&g_debug_cli, (char*)g_debug_cli.rx_buffer);
        
        // 重新启动 DMA 接收
        HAL_UARTEx_ReceiveToIdle_DMA(&huart3, g_debug_cli.rx_buffer, DEBUG_CLI_RX_BUFFER_SIZE);
        __HAL_DMA_DISABLE_IT(&hdma_usart3_rx, DMA_IT_HT);
    }
}
```

**注意：** 不需要在主循环中轮询 debug CLI，所有处理在 UART 回调中完成。

---

## Testing Strategy

### Unit Tests (Optional, Low Priority)

- `test_debug_cli_parser.c`：测试命令解析器（参数提取、边界检查）
- 使用 mock UART 和 mock balance_motor API

### Integration Tests (Primary)

**测试场景：**

| 测试 | 串口助手输入 | 预期响应 | 验证点 |
|------|-------------|---------|--------|
| 使能电机 | `en` | `OK motor enabled` | UART2 发送 0xF3 使能帧 |
| 设置零点 | `zero` | `OK zero set at <pos>` | `balance_motor.zero_valid = true` |
| 绝对位置移动 | `pos 1000` | `OK moving to 1000 at 100rpm` | 电机转动到目标位置 |
| 速度模式 | `vel 200` | `OK velocity 200rpm` | 电机持续转动 |
| 急停 | `stop` | `OK stopped` | 电机立即停止 |
| 位置查询 | `query` | `OK pos=<num>` | 返回当前编码器位置 |
| 未知命令 | `foo` | `ERR: unknown command 'foo'` | 错误处理 |
| 参数错误 | `pos abc` | `ERR: invalid argument 'abc'` | 参数验证 |

**硬件要求：**
- STM32 + EMM V5 驱动器 + 步进电机 + USB 转串口模块（连接 PB10/PB11）
- 串口助手（如 SSCOM、PuTTY、minicom）

---

## Configuration Parameters

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `DEBUG_CLI_RX_BUFFER_SIZE` | 128 | 接收缓冲区大小（字节） |
| `DEBUG_CLI_TX_BUFFER_SIZE` | 256 | 发送缓冲区大小（字节） |
| `DEBUG_CLI_ECHO_ENABLED` | `false` | 是否回显输入字符 |
| `DEBUG_CLI_MAX_ARGC` | 8 | 最大参数个数 |
| Motor default speed | 100 rpm | `pos` 命令默认速度 |
| Motor default accel | 10 | 默认加速度 |

---

## Future Enhancements (Out of Scope)

- **参数持久化：** 保存常用参数到 Flash，断电后恢复
- **宏命令：** 定义命令序列，如 `macro1: en; zero; pos 1000; stop`
- **自动完成：** 输入 `po<TAB>` 自动补全为 `pos`
- **历史记录：** 上下箭头浏览命令历史（需要 VT100 终端支持）
- **原始 Hex 模式：** 透传 EMM V5 二进制帧（高级调试）
- **PID 参数调节：** `pid <kp> <ki> <kd>` 修改驱动器 PID
- **多电机支持：** `motor 1 pos 1000` / `motor 2 pos -500`

---

## Risk Analysis

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| UART3 DMA 与 UART2 DMA 冲突 | 高 | STM32F4 有独立 DMA 通道，无冲突（已验证硬件配置） |
| 阻塞发送导致主循环卡死 | 中 | 限制单次发送最大 256 字节，超时 100ms，不影响 10ms 控制周期 |
| 命令解析 CPU 占用过高 | 低 | 人类输入速率慢（~1 命令/秒），解析开销可忽略 |
| 输入恶意超长命令（DoS） | 中 | 限制接收缓冲区 128 字节，超长部分截断 + 报错 |
| UART3 线路噪声导致误触发 | 低 | 使用换行符作为命令边界，半条命令不执行 |

---

## Non-Functional Requirements

- **响应延迟：** 命令执行到响应返回 < 200ms（含电机通信往返）
- **可靠性：** 连续 1000 次命令无丢失、无乱码
- **可读性：** 所有响应为人类可读 ASCII 文本，无二进制/转义序列
- **兼容性：** 支持常见串口助手（Windows/Linux/macOS）
- **资源占用：**
  - Flash: < 4 KB（命令解析器 + 字符串常量）
  - RAM: < 512 字节（两个缓冲区 + 状态结构体）
  - CPU: < 1%（空闲时 0%，接收命令时瞬时 5%）

---

## Design Decisions Log

| 决策 | 理由 |
|------|------|
| 选择方案 A（ASCII 文本）而非原始 Hex | 用户调试目标是"验证电机动作"，不需要协议级透明度；文本更直观 |
| UART3 使用 DMA + Idle 中断 | 匹配现有 UART2 风格，减少学习曲线 |
| 发送使用阻塞模式 | 调试接口不追求性能，简化实现，避免 DMA 队列管理 |
| 不在主循环中轮询 | 所有处理在 UART 回调完成，减少主循环耦合 |
| 默认速度 100 rpm | 平衡调试安全性（不太快）和响应速度（不太慢） |
| 不实现命令历史/自动完成 | 增加复杂度，收益有限（调试非高频操作） |

---

## Open Questions

**Q: 是否需要在 `help` 命令中显示当前电机状态？**
- A: 不需要。`status` 命令专门负责显示状态，`help` 仅显示命令列表，职责分离。

**Q: 如果 `pos` 命令执行时电机尚未使能，是否自动使能？**
- A: 不自动使能。强制用户显式执行 `en` → `zero` → `pos` 流程，避免误操作。如果未使能，返回 `ERR: motor not enabled (run 'en' first)`。

**Q: 是否支持相对位置移动（如 `pos +100` = 在当前位置基础上 +100）？**
- A: V1 不支持。EMM V5 协议和 `balance_motor` 都基于绝对位置，相对移动需要先 `query` 再计算，增加复杂度。可作为 Future Enhancement。

---

## Revision History

| 日期 | 版本 | 作者 | 变更说明 |
|------|------|------|----------|
| 2026-07-31 | 1.0 | Claude Code | 初始设计 |
