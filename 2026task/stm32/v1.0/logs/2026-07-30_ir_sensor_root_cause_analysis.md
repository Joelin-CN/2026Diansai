# IR传感器根本问题深度分析

**日期**: 2026-07-30  
**状态**: 🔴 发现致命缺陷  

---

## 🔴 根本问题：中断处理与HAL库冲突

### 问题现象

从测试日志 `records-2026-07-30-03-06-26.json` 看到：

```
Frames RX:   61
Errors:      834
Frame rate:  6.10 Hz (期望125 Hz)
Error rate:  1367.21 %
```

**只有6%的帧解析成功！**

### 核心原因分析

#### 1. **致命冲突：双重中断处理**

**文件**: `Core/Src/stm32f4xx_it.c:362-381`

```c
void USART2_IRQHandler(void)
{
  /* USER CODE BEGIN USART2_IRQn 0 */
  // 手动读取字节并调用自定义处理
  if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_RXNE) &&
      __HAL_UART_GET_IT_SOURCE(&huart2, UART_IT_RXNE)) {
    uint8_t received_byte = (uint8_t)(huart2.Instance->DR & 0xFF);
    IrUartSensor_RxByte(received_byte);        // ← 第一次读取
    IrUartDiag_CountRxByte(received_byte);
    IrRawCapture_RxByte(received_byte);
  }
  /* USER CODE END USART2_IRQn 0 */
  HAL_UART_IRQHandler(&huart2);  // ← HAL再次尝试读取！
  /* USER CODE BEGIN USART2_IRQn 1 */
}
```

**问题**：
1. **USER CODE BEGIN段**手动读取了`DR`寄存器（清除RXNE标志）
2. 然后调用`HAL_UART_IRQHandler(&huart2)`，HAL会再次检查RXNE
3. 但RXNE已经被清除，HAL可能会：
   - 触发ORE（overrun error）
   - 禁用RXNEIE中断
   - 进入错误处理流程

#### 2. **ORE标志被设置**

从诊断输出看到：
```
SR:   0x000000F8 [RXNE] [TC] [TXE] [ORE!]
```

**ORE (Overrun Error)** 表示：
- 新数据到达时，旧数据还没被读取
- 这会导致HAL库进入错误恢复模式
- **HAL可能会禁用RXNEIE中断！**

#### 3. **USART2配置了DMA但未使用**

**文件**: `Core/Src/usart.c:327-344`

```c
/* USART2 DMA Init */
/* USART2_RX Init */
hdma_usart2_rx.Instance = DMA1_Stream5;
hdma_usart2_rx.Init.Channel = DMA_CHANNEL_4;
hdma_usart2_rx.Init.Mode = DMA_CIRCULAR;
hdma_usart2_rx.Init.Priority = DMA_PRIORITY_HIGH;
...
__HAL_LINKDMA(uartHandle,hdmarx,hdma_usart2_rx);
```

**问题**：
- DMA被配置但从未启动（没有调用`HAL_UART_Receive_DMA()`）
- 中断模式手动读取字节
- **两种模式冲突可能导致HAL状态机混乱**

#### 4. **中断优先级配置**

**FreeRTOS配置**：
```c
configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY = 5
configPRIO_BITS = 4
// 实际最大安全优先级 = 5 << (8-4) = 5 << 4 = 80 (0x50)
```

**USART2中断优先级**：
```c
HAL_NVIC_SetPriority(USART2_IRQn, 5, 0);  // 优先级5
```

**分析**：
- 优先级5在FreeRTOS临界区边界上
- 在FreeRTOS中可以被`taskENTER_CRITICAL()`阻塞
- 如果主任务或其他任务持有临界区锁，USART2中断会被延迟
- 125 Hz的数据流（每8ms一帧）无法承受长时间的中断延迟

---

## 🔍 问题证据链

### 证据1: 低成功率
```
Frames RX:   61 / 10秒
应该收到:    1250帧 (125 Hz × 10s)
实际成功:    61帧 (4.88%)
```

### 证据2: 大量"Waiting for first valid frame..."
```
[  637 ms] Waiting for first valid frame...
[  849 ms] IR:  248  264  265  259  256  267  260  251 | Frames: 10, Errors: 67
[ 1065 ms] Waiting for first valid frame...
...
```

表明大部分时间`g_frame_ready == false`，即：
- 中断没有触发
- 或者帧在中断中被丢弃

### 证据3: ORE标志
```
SR:   0x000000F8 [ORE!]
```

ORE表示硬件溢出，数据到达速度超过软件处理能力。

### 证据4: RXNEIE状态不稳定
```
初始: CR1=0x0000200C [RXNEIE=OFF!]
强制使能后: CR1=0x0000202C [RXNEIE=ON]
```

但在测试中，RXNEIE可能会再次被HAL禁用（由于ORE错误处理）。

---

## 💡 根本原因总结

### 主要问题

**手动字节读取 + HAL_UART_IRQHandler() 的组合导致：**

1. **DR寄存器被读两次**
   - 第一次：`uint8_t received_byte = huart2.Instance->DR`
   - 第二次：HAL内部再次检查和读取

2. **HAL状态机混乱**
   - HAL期望自己管理RXNE标志
   - 手动读取破坏了HAL的假设
   - HAL可能进入错误状态

3. **ORE错误累积**
   - 高速数据流（125 Hz）
   - 中断处理时间过长（调用3个函数）
   - 新数据到达时旧数据还没处理完
   - 触发ORE，HAL禁用RXNEIE

4. **DMA配置未使用**
   - CubeMX配置了DMA但代码未启动
   - 可能导致HAL库内部状态不一致

### 次要问题

1. **FreeRTOS中断优先级**
   - 优先级5正好在临界区边界
   - 可能被FreeRTOS阻塞

2. **缓冲区小**
   - `IR_UART_SENSOR_FRAME_MAX = 96`字节
   - 单帧59字节，最多1.6帧
   - 高速数据流容易溢出

---

## 🎯 解决方案

### 方案A: 纯中断模式（推荐）

**删除HAL_UART_IRQHandler()调用，完全手动处理**

```c
void USART2_IRQHandler(void)
{
  /* USER CODE BEGIN USART2_IRQn 0 */
  if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_RXNE)) {
    uint8_t received_byte = (uint8_t)(huart2.Instance->DR & 0xFF);
    IrUartSensor_RxByte(received_byte);
  }
  
  // 清除ORE错误
  if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_ORE)) {
    __HAL_UART_CLEAR_OREFLAG(&huart2);
  }
  /* USER CODE END USART2_IRQn 0 */
  
  // 不调用 HAL_UART_IRQHandler(&huart2);
  
  /* USER CODE BEGIN USART2_IRQn 1 */
  /* USER CODE END USART2_IRQn 1 */
}
```

**优点**：
- 避免双重读取DR
- 避免HAL状态机干扰
- 中断处理更快（只调用一个函数）

**缺点**：
- 需要手动处理错误

### 方案B: DMA + 空闲中断（最优）

**使用CubeMX已配置的DMA**

```c
// 初始化时启动DMA循环接收
uint8_t dma_buffer[256];
HAL_UART_Receive_DMA(&huart2, dma_buffer, sizeof(dma_buffer));

// 启用IDLE中断
__HAL_UART_ENABLE_IT(&huart2, UART_IT_IDLE);

// 中断处理
void USART2_IRQHandler(void)
{
  if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_IDLE)) {
    __HAL_UART_CLEAR_IDLEFLAG(&huart2);
    
    // 计算接收到的字节数
    uint32_t received = sizeof(dma_buffer) - __HAL_DMA_GET_COUNTER(&hdma_usart2_rx);
    
    // 处理接收到的数据
    for (uint32_t i = 0; i < received; i++) {
      IrUartSensor_RxByte(dma_buffer[i]);
    }
  }
  
  HAL_UART_IRQHandler(&huart2);
}
```

**优点**：
- DMA自动搬运数据，CPU负担最小
- 支持高速数据流（125 Hz）
- HAL完全管理，状态机不混乱

**缺点**：
- 代码复杂度稍高

### 方案C: 提高中断优先级

**降低优先级数值（提高优先级）**

```c
HAL_NVIC_SetPriority(USART2_IRQn, 3, 0);  // 从5改为3
```

**配合方案A或B使用**

---

## 🚀 推荐实施步骤

### Step 1: 修复中断处理（方案A）

1. 修改`stm32f4xx_it.c`的`USART2_IRQHandler()`
2. 删除诊断和捕获函数调用（测试后可删除）
3. 添加ORE错误清除

### Step 2: 提高中断优先级

```c
HAL_NVIC_SetPriority(USART2_IRQn, 3, 0);
```

### Step 3: 验证

重新编译测试，应该看到：
```
Frames RX:   1250
Errors:      < 10
Frame rate:  125 Hz
Error rate:  < 1%
```

### Step 4: （可选）升级到DMA

如果方案A+Step2仍有问题，实施方案B。

---

## 📊 预期改善

| 指标 | 当前 | 修复后 |
|------|------|--------|
| 成功率 | 6% | >99% |
| 帧率 | 6 Hz | 125 Hz |
| 错误率 | 1367% | <1% |
| ORE错误 | 有 | 无 |

---

## 🔗 相关文件

| 文件 | 修改内容 |
|------|----------|
| `Core/Src/stm32f4xx_it.c` | 修复USART2_IRQHandler() |
| `Core/Src/usart.c` | 提高NVIC优先级 |
| `modules/IR-tracker/src/ir_uart_sensor.c` | 无需修改（代码正确）|

---

**文档创建时间**: 2026-07-30  
**分析者**: Claude (Opus 4.8)  
**根本问题**: 手动读取DR + HAL_UART_IRQHandler()冲突  
**推荐方案**: 删除HAL调用，纯手动中断处理
