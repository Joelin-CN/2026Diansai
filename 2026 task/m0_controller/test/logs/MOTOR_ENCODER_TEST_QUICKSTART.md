# Motor & Encoder Hardware Test - Quick Start Guide

**日期**: 2026-07-24  
**目标**: 快速验证电机驱动和编码器反馈是否正常工作

---

## 硬件准备

### 必需硬件
- ✅ MSPM0G3507开发板
- ✅ 4个电机（带编码器）
- ✅ TB6612驱动板（或类似H桥驱动）
- ✅ 电源（电机供电，建议6-12V）
- ✅ USB转串口（用于调试输出）

### 接线检查
1. **电机驱动（TB6612）**
   - PWM信号连接到MSPM0的PWM输出
   - 方向控制引脚（IN1/IN2）连接到GPIO
   - STBY引脚接3.3V（硬件上拉）

2. **编码器**
   - M1: ENC1_A → GPIOA, ENC1_B → GPIOA
   - M2: ENC2_A → GPIOA, ENC2_B → GPIOA
   - M3: ENC3_A → GPIOA, ENC3_B → GPIOA
   - M4: ENC4_A → GPIOA, ENC4_B → GPIOB

3. **串口调试**
   - UART0_TX (PA10) → USB转串口的RX
   - GND → GND

4. **电源**
   - 电机电源接TB6612的VCC和GND
   - 确保共地

---

## 快速开始（3步）

### 1️⃣ 编译和烧录

```bash
# 确认测试模式已设置为 TEST_MODE_MOTOR_ENCODER
# 在 src/main.c 中检查：
#define ACTIVE_TEST_MODE  TEST_MODE_MOTOR_ENCODER

# 在EIDE中编译
F7

# 烧录到MCU
点击"下载"按钮
```

### 2️⃣ 连接串口

**串口设置**:
- 波特率: 115200
- 数据位: 8
- 校验位: None
- 停止位: 1

**推荐工具**: PuTTY, Tera Term, 或串口助手

### 3️⃣ 运行测试

按下MCU的RESET按钮，观察串口输出和电机行为。

---

## 测试流程

### TEST 1: 编码器被动读取（10秒）
```
=== TEST 1: Encoder Passive Reading ===
Manually rotate each wheel to generate encoder pulses.
Monitoring for 10 seconds...

Motor | Count | Delta/s | Status
------|-------|---------|-------
  M1  |      0 |     0.0 | idle
  M2  |      0 |     0.0 | idle
  M3  |      0 |     0.0 | idle
  M4  |      0 |     0.0 | idle
```

**操作**: 手动转动每个轮子，观察对应的Count是否增加。

**预期结果**:
- ✅ 转动轮子时，对应电机的Count会变化
- ✅ Status显示"ACTIVE"
- ❌ 如果Count始终为0，检查编码器接线

---

### TEST 2: 电机基础控制（12秒）
```
=== TEST 2: Motor Basic Control ===
Testing each motor individually with PWM.
Listen for motor sounds and observe wheel rotation.

[Phase 0] M1 forward @ 20% PWM
[Phase 1] M2 forward @ 20% PWM
[Phase 2] M3 forward @ 20% PWM
[Phase 3] M4 forward @ 20% PWM
[Phase 4] All motors forward @ 50% PWM
[Phase 5] All motors reverse @ 50% PWM
```

**操作**: 无需操作，观察和聆听。

**预期结果**:
- ✅ 每个电机依次转动（每个2秒）
- ✅ 可以听到电机声音
- ✅ 轮子应该转动
- ❌ 如果电机不动，检查电源和驱动板

---

### TEST 3: 电机编码器闭环（6秒）
```
=== TEST 3: Motor + Encoder Closed-Loop ===
Running motors and monitoring encoder feedback.

[Phase 0] All motors forward @ 50% - monitoring encoders
Time(s) |   M1   |   M2   |   M3   |   M4   |
--------|--------|--------|--------|--------|
  0.0  |      0 |      0 |      0 |      0 |
  0.5  |    143 |    138 |    145 |    142 |
  1.0  |    287 |    281 |    289 |    283 |
  1.5  |    431 |    419 |    433 |    425 |
  2.0  |    574 |    562 |    577 |    567 |
  2.5  |    718 |    700 |    721 |    709 |

[Phase 1] All motors reverse @ 50% - monitoring encoders
Time(s) |   M1   |   M2   |   M3   |   M4   |
--------|--------|--------|--------|--------|
  0.0  |    718 |    700 |    721 |    709 |
  0.5  |    575 |    562 |    576 |    567 |
  1.0  |    432 |    419 |    431 |    425 |
  1.5  |    289 |    281 |    286 |    283 |
  2.0  |    146 |    138 |    141 |    141 |
  2.5  |      3 |     -5 |     -4 |     -1 |

=== Motor + Encoder Test Complete ===

Final encoder counts:
  M1: 3 counts
  M2: -5 counts
  M3: -4 counts
  M4: -1 counts

Result interpretation:
  - Counts increased during forward phase: ✓ Correct direction
  - Counts decreased during reverse phase: ✓ Correct direction
  - Counts near zero at end: ✓ Good symmetry
  - Counts stayed zero: ✗ Encoder not connected or motor not moving
```

**操作**: 无需操作，观察数据。

**预期结果**:
- ✅ 前进时编码器计数增加
- ✅ 后退时编码器计数减少
- ✅ 最终计数接近0（说明往返对称）
- ❌ 如果计数不变，编码器可能未连接

---

## 结果判断

### ✅ 测试通过
所有三个测试阶段都正常：
- 手动转动轮子时编码器计数变化
- 电机能响应PWM控制
- 电机运行时编码器能正确反馈

**下一步**: 可以进入完整控制应用模式

### ❌ 编码器问题
**症状**: 编码器计数始终为0或不变化

**排查步骤**:
1. 检查编码器供电（通常是5V或3.3V）
2. 用万用表测量编码器输出引脚电压
3. 转动轮子时观察电压是否变化（应在0-3.3V之间跳变）
4. 检查MSPM0引脚配置（是否启用了上拉/下拉）
5. 检查GPIO中断是否正常（在encoder.c中设置断点）

### ❌ 电机问题
**症状**: 电机不转或转速不正常

**排查步骤**:
1. 检查电机供电电压（6-12V，根据电机规格）
2. 用示波器测量PWM输出（应该有方波）
3. 检查TB6612的STBY引脚（应该是高电平）
4. 测量IN1/IN2引脚电压（应该有高低变化）
5. 直接给电机接电源，确认电机本身正常
6. 检查电机驱动板芯片是否过热（可能损坏）

---

## 常见问题

### Q1: 串口无输出
**A**: 
1. 检查UART接线（TX连RX，GND连GND）
2. 确认波特率设置为115200
3. 尝试按RESET按钮
4. 检查USB转串口驱动是否正确安装

### Q2: 编码器计数方向反了
**A**: 
- 这是正常的，取决于编码器安装方向
- 可以在软件中翻转（修改encoder.c中的查找表）

### Q3: 电机转向与预期相反
**A**: 
- 可以在motor.h中设置反转标志：
  ```c
  #define MOTOR_M1_REVERSED (1)  // 翻转M1方向
  ```

### Q4: 四个电机速度不一致
**A**: 
- 这是正常的，电机之间有个体差异
- 在实际控制中需要PID控制器来补偿

### Q5: 编码器计数有抖动
**A**: 
- 可能是接线过长导致干扰
- 尝试添加滤波电容（0.1uF）
- 检查是否有强电磁干扰源

---

## 进阶调试

### 修改测试参数

在 `tests/test_motor_encoder_hardware.c` 中：

```c
/* 修改测试速度 */
#define TEST_MOTOR_LOW_SPEED    200   // 20% PWM
#define TEST_MOTOR_MED_SPEED    500   // 50% PWM
#define TEST_MOTOR_HIGH_SPEED   800   // 80% PWM

/* 修改测试持续时间（毫秒） */
#define PHASE_DURATION 2000  // 每个阶段持续2秒
```

### 单独测试某个电机

可以修改测试代码，只运行特定电机：

```c
// 只测试M1
Motor_SetFour(TEST_MOTOR_MED_SPEED, 0, 0, 0);
```

### 添加详细日志

在关键位置添加printf：

```c
int32_t count = Encoder_GetCount(ENCODER_M1);
printf("[DEBUG] M1 encoder count: %d\n", count);
```

---

## 切换回完整控制模式

测试通过后，切换到完整控制应用：

1. 修改 `src/main.c`:
   ```c
   #define ACTIVE_TEST_MODE  TEST_MODE_CONTROL_APP
   ```

2. 修改 `src/control_app.c`:
   ```c
   #define SOFTWARE_TEST_MODE 0  // 启用硬件模式
   ```

3. 重新编译烧录

---

## 参考资料

- `DEBUG_LOG_UART0_AND_SOFTWARE_TEST_MODE.md` - UART调试设置
- `DEBUG_LOG_ICM42688_HARDWARE_TEST.md` - ICM42688测试流程（类似方法）
- `src/motor.c` - 电机驱动实现
- `src/encoder.c` - 编码器读取实现
- `tests/test_motor_encoder_hardware.c` - 测试程序源码

---

**祝测试顺利！**
