# 🔄 电机方向调试任务交接文档

## 📌 当前状态

### ✅ 已完成
1. **motor.c 接口层验证** - 已确认工作正常（test_motor_simple.c 测试通过）
2. **引脚配置验证** - 已确认与参考工程完全一致
3. **基础硬件功能** - PWM输出、GPIO控制、UART通信均正常

### ⚠️ 待解决的问题

#### 问题1：电机方向不一致
**现象**：
- 物理布局：1 2（前轮）/ 3 4（后轮）
- 调用 `Motor_SetFour(300, 300, 300, 300)` 时：
  - 前轮（1, 2）**往后转**
  - 后轮（3, 4）**往前转**

**软件编号映射**：
```c
Motor_SetFour(m1, m2, m3, m4)
            (左, 左, 右, 右)
```
- M1, M2 = "左侧"电机
- M3, M4 = "右侧"电机

**需要确认的信息**（向用户询问）：
1. 物理轮子1/2/3/4分别对应软件的M1/M2/M3/M4中的哪个？
2. 正常前进时，期望所有轮子都往哪个方向转？

#### 问题2：串口输出乱码
**现象**：
- test_motor_debug.c 的输出有大量乱码字符
- 偶尔能看到正确的片段如 "CCR=700"、"30%"

**可能原因**：
- `sprintf()` 函数执行时间过长导致UART FIFO溢出
- 时钟配置问题
- 输出频率过高

**已创建的解决方案**：
- `test_motor_identify.c` - 避免使用sprintf，只用简单字符串，逐个电机测试

---

## 🔧 相关代码位置

### 核心文件
- **src/motor.c** - 电机控制实现，包含 `Motor_SetOne()` 方向逻辑
- **inc/motor.h** - 定义了 `MOTOR_M1_REVERSED` 到 `MOTOR_M4_REVERSED`（当前全为0）
- **src/main.c** - 测试模式切换（当前 TEST_MODE_MOTOR_DEBUG = 7）

### 测试文件
- **tests/test_motor_simple.c** ✅ - 最简单测试，已验证成功
- **tests/test_motor_debug.c** ⚠️ - 读取PWM/GPIO值，有串口乱码
- **tests/test_motor_identify.c** 🆕 - 逐个电机识别测试（刚创建）

### 方向反转标志（inc/motor.h:25-28）
```c
#define MOTOR_M1_REVERSED (0)
#define MOTOR_M2_REVERSED (0)
#define MOTOR_M3_REVERSED (0)
#define MOTOR_M4_REVERSED (0)
```

### Motor_SetOne 逻辑（src/motor.c:78-95）
```c
forward = (speed >= 0);
if (reversed) forward = !forward;  // 反转标志生效处

if (duty == 0U) {
    Motor_WritePin(in1Port, in1Pin, false);
    Motor_WritePin(in2Port, in2Pin, false);
} else {
    Motor_WritePin(in1Port, in1Pin, forward);
    Motor_WritePin(in2Port, in2Pin, !forward);
}
```

---

## 🎯 下一步行动计划

### 立即执行

1. **运行电机识别测试**
   ```c
   // 修改 src/main.c
   #define TEST_MODE_MOTOR_IDENTIFY (8)
   #define CURRENT_TEST_MODE TEST_MODE_MOTOR_IDENTIFY
   ```
   
   在 `.eide/eide.yml` 中：
   ```yaml
   excludeList:
     - tests/test_motor_encoder_hardware.c
     - tests/test_motor_simple.c  
     - tests/test_motor_debug.c
     - tests/test_encoder_simple.c
   # 只保留 test_motor_identify.c 被编译
   ```

2. **记录每个阶段的物理轮子运动**
   - Phase 1: M1驱动时，物理哪个轮子转？往哪转？
   - Phase 3: M2驱动时，物理哪个轮子转？往哪转？
   - Phase 5: M3驱动时，物理哪个轮子转？往哪转？
   - Phase 7: M4驱动时，物理哪个轮子转？往哪转？

3. **根据识别结果调整反转标志**
   
   假设期望"所有轮子都往前转"，根据实际测试结果：
   - 如果某个电机实际往后转 → 设置对应的 `MOTOR_MX_REVERSED` 为 `1`
   - 如果某个电机实际往前转 → 保持 `MOTOR_MX_REVERSED` 为 `0`

### 解决串口乱码（可选，优先级较低）

如果 test_motor_identify.c 仍有乱码，尝试：
1. 减少输出频率（增加延迟）
2. 检查波特率设置（当前115200）
3. 使用硬件流控或更大的UART FIFO

---

## 📂 项目路径

- **当前工作目录**：`E:\B306\2026\电赛\2026 task\m0_controller\test`
- **参考工程**：`E:\BaiduNetdiskDownload\PWM30_All_Motors_Validated_20260724\`

---

## 🔑 关键技术细节

### PWM配置
- 频率：32 kHz
- 周期：1000 counts
- 30%占空比 → CCR=700（因为是边沿对齐模式：duty = (PERIOD - CCR) / PERIOD）

### TB6612 真值表
| IN1 | IN2 | 输出 |
|-----|-----|------|
| H   | L   | 正转 |
| L   | H   | 反转 |
| L   | L   | 停止 |

### UART配置
- 波特率：115200
- 引脚：PA10(TX), PA11(RX)

---

## ⚡ 快速命令

### 编译和烧录
```bash
# 在 EIDE 中按 F7 编译，或使用命令行
```

### 查看串口输出
- 工具：串口助手
- 波特率：115200
- 数据位：8
- 停止位：1
- 校验：None

---

## 💡 预期结果

运行 test_motor_identify.c 后，你应该看到类似输出：
```
=== MOTOR IDENTIFICATION TEST ===
Each motor will run for 3 seconds.
Note which physical wheel moves.

Phase 0: ALL STOP (2s)
Phase 1: M1 ONLY at 30% (3s)
Phase 2: STOP (1s)
Phase 3: M2 ONLY at 30% (3s)
...
Phase 8: STOP

=== TEST COMPLETE ===
Record which wheel moved in each phase.
Press RESET to run again.
```

根据这个输出，记录每个软件编号（M1-M4）对应的物理轮子和转向，然后调整反转标志即可解决方向问题。

---

## 📞 联系信息

如果新线程中有疑问，提供以下上下文：
- "电机方向调试任务"
- 当前阶段：需要运行 test_motor_identify.c 识别物理-软件映射关系
- 参考本文档 HANDOFF.md
