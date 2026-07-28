# 编码器轮询方案 - 解决GPIO中断风暴问题

**日期**: 2026-07-27  
**解决方案**: 使用定时器轮询代替GPIO中断

---

## 问题回顾

GPIO中断方式导致程序完全卡死，原因是：
1. 编码器引脚可能悬空或信号质量差
2. 高频噪声触发中断风暴
3. CPU 100%时间处理中断，主循环无法执行

即使：
- 简化中断处理函数到只有3行代码
- 改为单边沿触发（RISE only）
- 降低中断优先级到最低

问题仍然存在。

---

## 实施的解决方案：轮询模式

### 核心思路
不使用GPIO中断，而是从主循环或定时器周期性调用 `Encoder_Poll()` 来读取编码器状态。

### 代码修改

#### 1. 编码器驱动 (src/encoder.c)

**修改**: 禁用GPIO中断，添加 `Encoder_Poll()` 函数

```c
void Encoder_Init(void)
{
    uint8_t encoder;
    for (encoder = 0U; encoder < (uint8_t)ENCODER_ID_COUNT; encoder++) {
        g_encoderCount[encoder] = 0;
        g_encoderState[encoder] = Encoder_ReadState((Encoder_Id)encoder);
    }
    /* DO NOT enable GPIO interrupts - they cause interrupt storm */
    // NVIC_EnableIRQ(ENCODER_GPIOA_INT_IRQN);
    // NVIC_EnableIRQ(ENCODER_GPIOB_INT_IRQN);
}

void Encoder_Poll(void)
{
    /* Call this function periodically (e.g., 1kHz) to update encoder counts */
    uint8_t encoder;
    for (encoder = 0U; encoder < (uint8_t)ENCODER_ID_COUNT; encoder++) {
        Encoder_Process((Encoder_Id)encoder);
    }
}
```

#### 2. 编码器头文件 (inc/encoder.h)

**修改**: 添加 `Encoder_Poll()` 函数声明

```c
void Encoder_Init(void);
void Encoder_Poll(void);  /* Call this periodically (e.g., 1kHz) to update encoder counts */
int32_t Encoder_GetCount(Encoder_Id encoder);
void Encoder_ResetCount(Encoder_Id encoder);
```

#### 3. 新的测试程序 (tests/test_encoder_polling.c)

**创建**: 使用SysTick以1kHz频率调用 `Encoder_Poll()`

关键特性：
- SysTick配置为1ms中断（1kHz）
- 在主循环中每1ms调用一次 `Encoder_Poll()`
- 状态机测试每个电机正反转各2秒
- 每500ms输出调试信息

```c
void SysTick_Handler(void)
{
    g_systemMs++;
}

static void EncoderPollTask(void)
{
    if (g_systemMs - g_lastPollMs >= 1) {
        g_lastPollMs = g_systemMs;
        Encoder_Poll();  // 1kHz polling rate
    }
}
```

---

## 轮询频率选择

### 1kHz (每1ms轮询一次)

**优点**:
- 对大多数电机编码器足够快（假设1000 CPR编码器，1000 RPM转速 = 约67 Hz基频）
- CPU占用低（4个编码器读取 < 50us）
- 可靠性高，不会丢失太多脉冲

**缺点**:
- 高速场景下可能丢失脉冲
- 不如硬件QEI精确

### 如何调整频率

修改 `EncoderPollTask()` 中的判断条件：

```c
// 500Hz (每2ms)
if (g_systemMs - g_lastPollMs >= 2) { ... }

// 2kHz (每0.5ms)
if (g_systemMs - g_lastPollMs >= 1) { 
    if ((g_systemMs & 1) == 0) { ... }  // 只在偶数毫秒执行
}
```

---

## 如何使用

### 方法A：在主循环中轮询

```c
int main(void)
{
    SYSCFG_DL_init();
    SysTick_Config(32000000 / 1000);  // 1ms tick
    
    Motor_Init();
    Encoder_Init();
    
    while (1) {
        EncoderPollTask();  // 自动限流到1kHz
        // 其他任务...
    }
}
```

### 方法B：在现有定时器中断中轮询

如果已有2ms控制定时器（TIMG0 @ 500Hz），可以直接在其中断中调用：

```c
void TIMG0_IRQHandler(void)
{
    // 清除中断标志
    DL_Timer_clearInterruptStatus(CONTROL_TIMER_INST, DL_TIMER_INTERRUPT_ZERO_EVENT);
    
    // 调用编码器轮询（500Hz也足够）
    Encoder_Poll();
    
    // 控制任务...
}
```

### 方法C：在FreeRTOS任务中轮询

```c
void EncoderTask(void *pvParameters)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(1);  // 1ms = 1kHz
    
    while (1) {
        Encoder_Poll();
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}
```

---

## 性能分析

### CPU占用估算

- 单次 `Encoder_Poll()` 调用：
  - 4个编码器 × (2个GPIO读取 + 查表 + 加法) ≈ 40-50 CPU周期
  - @ 32MHz: ≈ 1.5μs
  
- 1kHz轮询频率：
  - 每秒CPU占用：1.5μs × 1000 = 1.5ms
  - CPU占用率：1.5ms / 1000ms = **0.15%**

### 脉冲丢失风险

假设编码器参数：
- 1000线编码器（1000 PPR）
- 4倍频后 = 4000 CPR (Counts Per Revolution)
- 最高转速 3000 RPM

计算：
- 3000 RPM = 50 RPS (Revolutions Per Second)
- 50 RPS × 4000 CPR = 200,000 counts/second = 200 kHz
- 单相频率：200 kHz / 4 = 50 kHz

**1kHz轮询**：每次轮询间隔1ms，期间可能产生 50 个边沿。如果编码器在两次轮询之间旋转超过1/4圈，会丢失计数。

**解决方案**：
1. 对于低速应用（< 1000 RPM），1kHz完全足够
2. 对于高速应用，需要：
   - 提高轮询频率到5-10kHz，或
   - 使用硬件QEI模块（未来方案）

---

## 与GPIO中断方案对比

| 特性 | GPIO中断方案 | 轮询方案 |
|------|-------------|---------|
| **精度** | 理论上最高（每个边沿都捕获） | 取决于轮询频率，1kHz对低速足够 |
| **CPU占用** | 中断频繁时接近100%（卡死） | 固定0.15%（1kHz时） |
| **抗噪声能力** | 极差，噪声导致中断风暴 | 好，自然滤波 |
| **实时性** | 高（边沿立即响应） | 低（最大延迟1ms） |
| **稳定性** | ❌ 不稳定，悬空引脚导致卡死 | ✅ 稳定可靠 |
| **适用场景** | 需要高速高精度且信号质量好 | 一般电机控制应用 |

---

## 下一步改进方向

### 短期：优化轮询方案

1. **自适应轮询频率**：根据当前转速动态调整轮询频率
2. **硬件滤波**：添加RC滤波电路改善信号质量
3. **统计分析**：记录编码器速度变化，判断是否丢失脉冲

### 长期：硬件QEI方案

使用MSPM0G3507的硬件定时器捕获功能（Timer Capture Mode）：

1. **研究MSPM0 SDK示例**：
   - 搜索关键词：`MSPM0 timer capture quadrature`
   - 查看TI E2E论坛相关讨论
   - GitHub搜索MSPM0编码器项目

2. **配置Timer Capture**：
   - 使用TIMG1/TIMG2等未使用的定时器
   - 配置为Quadrature Decoder模式
   - A/B相信号连接到Timer Capture引脚
   - 硬件自动解码，只需读取计数值

3. **优势**：
   - 零CPU占用（硬件解码）
   - 无脉冲丢失（硬件计数器）
   - 内建硬件滤波

---

## 文件修改清单

| 文件 | 修改内容 |
|------|---------|
| `src/encoder.c` | 注释掉NVIC_EnableIRQ()，添加Encoder_Poll()函数 |
| `inc/encoder.h` | 添加Encoder_Poll()函数声明 |
| `tests/test_encoder_polling.c` | ✅ 新建轮询测试程序 |

---

## 测试计划

### 测试1：基本功能验证
- 编译并运行 `test_encoder_polling.c`
- 验证程序不再卡死
- 观察每个电机的编码器计数

### 测试2：精度验证
- 电机以已知转速运行（如50% PWM）
- 记录2秒内的编码器计数
- 与理论值比较，评估丢失率

### 测试3：不同速度测试
- 测试PWM 20%, 50%, 80%的计数准确性
- 找出开始丢失明显脉冲的临界转速

### 测试4：长时间稳定性
- 连续运行30分钟
- 验证无卡死、无异常

---

## 总结

**轮询方案是当前最可行的解决方案**，因为：

1. ✅ 完全避免GPIO中断风暴问题
2. ✅ 实现简单，代码改动小
3. ✅ CPU占用极低（< 1%）
4. ✅ 对悬空/噪声引脚有天然容错性
5. ✅ 适合中低速电机控制（< 1000 RPM）

**不适用场景**：
- ❌ 需要极高精度的位置控制
- ❌ 高速电机（> 3000 RPM）
- ❌ 需要捕获每个边沿的应用

**未来升级路径**：
当需要更高性能时，研究并实施硬件QEI/Timer Capture方案。

---

## 参考资料

- 调试日志：`logs/encoder_interrupt_hang_debug.md`
- MSPM0G3507数据手册：Timer章节
- TI E2E论坛：搜索"MSPM0 quadrature encoder"
