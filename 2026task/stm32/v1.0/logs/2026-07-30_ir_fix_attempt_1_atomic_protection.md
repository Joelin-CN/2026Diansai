# 修复尝试 #1 - 原子操作保护

**日期**: 2026-07-30  
**方案**: 在帧数据复制时使用原子操作  
**状态**: ✅ 已实施，待测试  

---

## 📝 修改内容

### 文件：`modules/IR-tracker/src/ir_uart_sensor.c`

**函数**: `IrUartSensor_Process()`

**修改前的问题**：
```c
ir_uart_sensor_status_t IrUartSensor_Process(void)
{
    if (!g_frame_ready) {
        return IR_UART_SENSOR_STATUS_NO_FRAME;
    }

    g_frame_ready = false;
    memcpy(g_frame, (const void *)g_rx_frame, g_rx_length);  // ← 竞态！
    //                                         ^^^^^^^^^^^^
    //                               中断可能在这里修改 g_rx_length!
    
    parsed_ok = IrUartSensor_ParseAnalogFrame(g_frame, g_rx_length);
    ...
}
```

**竞态场景**：
```
时间线:
T0: 中断完成帧1接收 → g_frame_ready=true, g_rx_length=67
T1: 中断接收到帧2的'$' → g_rx_length=0 (重置!)
T2: 主循环检查 g_frame_ready → true
T3: 主循环 memcpy(g_frame, g_rx_frame, 0) → 复制0字节!
T4: 解析失败 → BAD_FRAME
```

**修改后的保护**：
```c
ir_uart_sensor_status_t IrUartSensor_Process(void)
{
    uint8_t local_length;

    // CRITICAL SECTION: 关中断保护
    __disable_irq();

    if (!g_frame_ready) {
        __enable_irq();
        return IR_UART_SENSOR_STATUS_NO_FRAME;
    }

    // 原子操作：同时读取长度和复制数据
    local_length = g_rx_length;
    memcpy(g_frame, (const void *)g_rx_frame, local_length);
    g_frame_ready = false;

    __enable_irq();
    // END CRITICAL SECTION

    // 在临界区外解析（不影响中断响应）
    parsed_ok = IrUartSensor_ParseAnalogFrame(g_frame, local_length);
    ...
}
```

---

## 🎯 预期效果

### 修复前
```
Duration:    10008 ms
Frames RX:   63
Errors:      834
Frame rate:  6.29 Hz
Error rate:  1323.81 %
```

### 修复后（预期）
```
Duration:    10000 ms
Frames RX:   1250
Errors:      < 10
Frame rate:  125.00 Hz
Error rate:  < 1.00 %
```

---

## ⏱️ 性能影响

### 临界区持续时间估算

```c
__disable_irq();
  local_length = g_rx_length;           // ~2 cycles
  memcpy(..., local_length);            // ~67 bytes × 1 cycle = 67 cycles
  g_frame_ready = false;                // ~2 cycles
__enable_irq();
```

**总时间**: ~71 cycles @ 168 MHz = **0.42 μs**

**对125 Hz数据流的影响**：
- 字节间隔: 87 μs @ 115200 baud
- 临界区: 0.42 μs
- 影响: **0.48%** (可忽略)

---

## ✅ 为什么这个方案应该有效

### 1. 消除竞态窗口

**之前**：
```
检查 g_frame_ready → 复制数据 → 清除标志
   ↑                    ↑
   中断可能插入         中断可能修改 g_rx_length
```

**现在**：
```
[关中断]
检查 g_frame_ready → 复制数据 → 清除标志
[开中断]
   ↑
   中断无法插入
```

### 2. 保证数据一致性

- `local_length` 和 `g_rx_frame` 的内容是同一时刻的快照
- 中断无法在复制过程中修改数据

### 3. 最小化中断延迟

- 临界区仅包含必要操作（<1 μs）
- 帧解析在临界区外（~10-20 μs）
- 对125 Hz数据流影响可忽略

---

## 🧪 测试验证

### 测试步骤

1. **编译固件**
   ```bash
   cd /e/B306/2026/diansai/2026task/stm32/v1.0
   cmake --build build
   ```

2. **烧录并运行**

3. **查看串口输出**，期待看到：
   ```
   [ 200 ms] IR: 1521 1762 1753 1696 1581 1792 1692 1464 | Frames: 25, Errors: 0
   [ 400 ms] IR: 1521 1762 1753 1696 1581 1792 1692 1464 | Frames: 50, Errors: 0
   ...
   [10000 ms] IR: 1521 1762 1753 1696 1581 1792 1692 1464 | Frames: 1250, Errors: 0
   
   ✅ PASS: IR sensor working normally
   Frame rate: 125.00 Hz
   ```

### 成功标志

- ✅ Frames RX ≈ 1250 (125 Hz × 10s)
- ✅ Error rate < 1%
- ✅ 连续的数据流，无"Waiting for first valid frame..."
- ✅ 8通道数据稳定

### 失败标志

- ❌ 仍然大量"Waiting for first valid frame..."
- ❌ Error rate > 10%
- ❌ Frame rate < 50 Hz

如果失败 → 尝试**方案B：双缓冲**

---

## 📊 方案对比

| 方案 | 复杂度 | 性能影响 | 可靠性 |
|------|--------|----------|--------|
| **A. 原子保护** | 低 | 0.5% | 高 |
| B. 双缓冲 | 中 | 0% | 最高 |
| C. 增加缓冲 | 低 | 0% | 中 |
| D. 提高频率 | 低 | 取决于主循环 | 中 |

---

## 🔗 相关文档

- `logs/2026-07-30_ir_sensor_race_condition_analysis.md` - 竞态条件分析
- `logs/2026-07-30_ir_sensor_root_cause_analysis.md` - 根本原因分析

---

**实施时间**: 2026-07-30  
**实施者**: Claude (Opus 4.8)  
**下一步**: 编译测试，如果成功则完成；如果失败则尝试方案B
