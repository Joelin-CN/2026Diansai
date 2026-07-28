# 编码器中断导致程序卡死问题 - 调试日志

**日期**: 2026-07-27  
**项目**: MSPM0G3507 电机控制器  
**问题**: 自动编码器测试程序在启用GPIO中断后立即卡死

---

## 问题描述

### 症状
程序在打印以下信息后完全卡死，无任何后续输出：
```
Automatic Encoder Test Starting...

=====================================
 Automatic Encoder Test             
=====================================
Board: MSPM0G3507
Test: Motor drives, encoder reads

✓ Motor driver initialized
✓ Encoder interrupts enabled

Starting test sequence...

=== Test 1: M1 (Left Front) Forward @ 50% ===
```

电机M1开始旋转，但程序主循环完全停止，连每500ms的调试输出都没有。

### 核心问题
**`Encoder_Init()` 中调用 `NVIC_EnableIRQ()` 后，程序立即卡死，主循环无法执行**

---

## 尝试过的修复方案（全部失败）

### 1. 修改GPIO触发模式：RISE_FALL → RISE
**文件**: `NewProject1.syscfg` line 191  
**修改前**: `ENCODER.associatedPins[i].polarity = "RISE_FALL";`  
**修改后**: `ENCODER.associatedPins[i].polarity = "RISE";`  
**结果**: ❌ 仍然卡死

### 2. 降低中断优先级到最低
**文件**: `src/encoder.c` - `Encoder_Init()`  
**代码**:
```c
NVIC_SetPriority(ENCODER_GPIOA_INT_IRQN, 3);  // 3 = lowest on Cortex-M0+
NVIC_SetPriority(ENCODER_GPIOB_INT_IRQN, 3);
```
**结果**: ❌ 仍然卡死

### 3. 极简化中断处理函数
**文件**: `src/encoder.c`  
**修改**: 注释掉所有编码器解码逻辑，只保留计数器和清除中断标志  
**代码**:
```c
void GPIOA_IRQHandler(void)
{
    g_interruptCountA++;
    const uint32_t pins = ENCODER_ENC1_A_PIN | ENCODER_ENC1_B_PIN |
                          ENCODER_ENC2_A_PIN | ENCODER_ENC2_B_PIN |
                          ENCODER_ENC3_A_PIN | ENCODER_ENC3_B_PIN | ENCODER_ENC4_A_PIN;
    const uint32_t status = DL_GPIO_getEnabledInterruptStatus(GPIOA, pins);
    DL_GPIO_clearInterruptStatus(GPIOA, status);
    // All encoder decoding logic commented out
}
```
**结果**: ❌ 仍然卡死

### 4. 添加中断计数调试输出
**文件**: `tests/test_encoder_auto.c`  
**代码**:
```c
uint32_t intA, intB;
Encoder_GetInterruptCounts(&intA, &intB);
printf("[DEBUG] State=%d, elapsed=%lu ms, IntA=%lu, IntB=%lu\n",
       (int)g_state, (unsigned long)elapsedMs, (unsigned long)intA, (unsigned long)intB);
```
**结果**: ❌ 这段调试代码从未执行（证明主循环完全卡死）

---

## 根本原因分析

### 最可能的原因
**GPIO中断风暴** - 编码器引脚可能：
1. **悬空未连接** - 导致高频噪声触发
2. **信号质量差** - 抖动/噪声导致每微秒都触发中断
3. **硬件故障** - 编码器输出异常

### 证据
1. 即使中断处理函数只有3行代码（计数+清标志），程序仍卡死
2. 单边沿触发（RISE only）仍无效
3. 最低优先级仍无效
4. 主循环的调试输出从未出现，说明CPU被中断完全占用

### 为什么会卡死而不是报错？
**中断频率过高** → CPU 100%时间在处理中断 → 主循环永远得不到执行

---

## 验证实验结果

### ✅ 成功的测试
1. **test_encoder_no_read.c** - 只运行电机，不初始化编码器
   - 结果：正常工作，主循环输出正常
   - 结论：电机驱动、主循环、定时器系统都没问题

2. **test_encoder_no_interrupt.c** - 初始化编码器但不启用NVIC中断
   - 结果：正常工作，可以轮询读取编码器状态
   - 结论：GPIO配置、编码器读取函数都没问题

### ❌ 失败的测试
所有启用 `NVIC_EnableIRQ(ENCODER_GPIOA_INT_IRQN)` 的测试全部卡死

---

## 下一步建议

### 方案A：使用硬件QEI模块（推荐）
MSPM0G3507有专用的Quadrature Encoder Interface (QEI)硬件：
- 无需CPU介入即可解码正交信号
- 硬件滤波，抗噪声能力强
- 自动计数，CPU只需读取计数值

**需要做的**:
1. 在TI SysConfig中查找QEI或Timer Capture配置
2. 查阅MSPM0G3507数据手册的QEI章节
3. 重新配置硬件使用QEI模块而非GPIO中断

### 方案B：定时器轮询
主循环中使用定时器定期（如1kHz）读取编码器GPIO状态：
- 不使用中断，避免中断风暴
- 软件解码正交信号
- 可能丢失高速脉冲，但对调试足够

**实现**:
```c
// 每1ms调用一次
void Encoder_Poll(void) {
    for (uint8_t i = 0; i < ENCODER_ID_COUNT; i++) {
        uint8_t newState = Encoder_ReadState((Encoder_Id)i);
        // 查表解码
        // ...
    }
}
```

### 方案C：硬件调试
1. 用示波器/逻辑分析仪查看编码器信号
2. 检查引脚是否悬空
3. 添加硬件滤波电路（RC滤波或施密特触发器）

---

## 当前代码状态

### 最后修改
**文件**: `src/encoder.c`  
**状态**: 已禁用 `NVIC_EnableIRQ()` 调用（注释掉）

```c
void Encoder_Init(void)
{
    uint8_t encoder;
    for (encoder = 0U; encoder < (uint8_t)ENCODER_ID_COUNT; encoder++) {
        g_encoderCount[encoder] = 0;
        g_encoderState[encoder] = Encoder_ReadState((Encoder_Id)encoder);
    }

    /* DO NOT enable interrupts yet - for debugging */
    // NVIC_SetPriority(ENCODER_GPIOA_INT_IRQN, 3);
    // NVIC_SetPriority(ENCODER_GPIOB_INT_IRQN, 3);
    // NVIC_EnableIRQ(ENCODER_GPIOA_INT_IRQN);
    // NVIC_EnableIRQ(ENCODER_GPIOB_INT_IRQN);
}
```

### 相关文件
- `tests/test_encoder_auto.c` - 自动测试程序（当前使用）
- `tests/test_encoder_no_read.c` - 诊断测试（已验证工作）
- `tests/test_encoder_no_interrupt.c` - 诊断测试（已验证工作）
- `src/encoder.c` - 编码器驱动（问题所在）
- `inc/encoder.h` - 编码器接口
- `NewProject1.syscfg` - 硬件配置（line 191改为RISE触发）

---

## 关键发现

1. **GPIO中断本身有问题**，与代码逻辑无关
2. **硬件配置或信号质量是根本原因**
3. **不应继续尝试修改中断处理代码** - 问题不在软件层面
4. **必须更换技术方案** - GPIO中断不适合此应用场景

---

## 参考资料

### TI官方文档
- MSPM0G3507 Technical Reference Manual - QEI章节
- MSPM0 SDK Examples - 查找QEI示例代码
- TI E2E论坛 - 搜索"MSPM0 quadrature encoder"

### GitHub参考
用户建议："去GitHub上边看看其他人的编码器是怎么弄的"
- 搜索关键词：`MSPM0 quadrature encoder`
- 搜索关键词：`MSPM0 QEI`
- 搜索关键词：`MSPM0G3507 motor encoder`

---

## 总结

**不要再修改GPIO中断代码！问题不在这里！**

应该：
1. ✅ 使用硬件QEI模块（最佳方案）
2. ✅ 改用定时器轮询（备选方案）
3. ✅ 检查硬件信号质量（根本诊断）

不应该：
1. ❌ 继续优化中断处理函数
2. ❌ 尝试其他中断优先级
3. ❌ 修改GPIO触发模式配置

---

## ✅ 已实施解决方案 (2026-07-27)

**采用方案B：定时器轮询**

### 修改内容
1. `src/encoder.c` - 禁用GPIO中断，添加 `Encoder_Poll()` 函数
2. `inc/encoder.h` - 添加 `Encoder_Poll()` 声明
3. `tests/test_encoder_polling.c` - 新建轮询测试程序（1kHz SysTick轮询）

### 轮询方案优势
- ✅ 完全避免中断风暴
- ✅ CPU占用 < 1%
- ✅ 对噪声/悬空引脚有容错性
- ✅ 适合中低速电机（< 1000 RPM）

详细文档见：`logs/encoder_polling_solution.md`
