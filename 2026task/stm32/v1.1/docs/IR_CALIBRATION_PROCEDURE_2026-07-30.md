# IR传感器校准操作手册

**版本**: v1.2.1  
**日期**: 2026-07-30  
**适用模式**: `TEST_MODE_IR_CALIBRATION`

---

## 1. 准备工作

### 1.1 硬件需求

| 项目 | 说明 | 状态 |
|------|------|------|
| STM32F407开发板 | 已烧录IR校准固件 | ☐ |
| 8路红外传感器阵列 | 已连接USART2 (PA2/PA3, 115200) | ☐ |
| USB-TTL模块 | 连接UART5 (PC12/PD2, 115200) | ☐ |
| 纯白色表面 | A4纸或白色赛道区域 | ☐ |
| 黑色胶带/黑线 | 宽度20-30mm | ☐ |
| 7.4V锂电池 | 已充电 (>7.0V) | ☐ |

### 1.2 编译确认

打开 `Core/Src/freertos.c`，确认第XX行附近：

```c
#define TEST_MODE_IR_CALIBRATION     /* ← 当前激活 */
// #define TEST_MODE_TRACK_CONTROL   /* ← 必须注释掉 */
```

> **注意**: 如果两个宏同时定义，`TEST_MODE_IR_CALIBRATION` 优先。

---

## 2. 校准流程

```
┌─────────────┐    ┌─────────────┐    ┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│  Step 1     │    │  Step 2     │    │  Step 3     │    │  Step 4     │    │  Step 5     │
│  硬件初始化  │───▶│  白平衡校准  │───▶│  黑线阈值   │───▶│  结果确认   │───▶│  实时验证   │
│  (自动)     │    │  (需操作)    │    │  (需操作)    │    │  (自动)     │    │  (需操作)    │
└─────────────┘    └─────────────┘    └─────────────┘    └─────────────┘    └─────────────┘
     30秒              ~5分钟             ~2分钟              查看              5分钟
```

---

### Step 1: 硬件初始化（自动）

**操作**: 上电，连接串口终端

**预期串口输出**:
```
╔════════════════════════════════════════════════════════════════╗
║       IR Sensor Calibration Mode - STM32 Track Robot v1.2.1   ║
╚════════════════════════════════════════════════════════════════╝

[STEP 1/5] Initializing hardware...
[INFO] Initializing IR sensor (USART2, 115200 baud)...
[INFO] Waiting for IR sensor warm-up (2 seconds)...
[SUCCESS] Hardware initialization complete!
```

**检查项**:
- [ ] 无 `[FATAL]` 或 `[ERROR]` 信息
- [ ] 传感器供电指示灯亮起

---

### Step 2: 白平衡校准

**操作**:

1. 将小车放置在**纯白色表面**上（A4纸或白色赛道）
2. 确保**所有8个传感器下方都是白色**（无黑线）
3. 保持小车**静止不动**
4. 等待倒计时结束

**预期串口输出**:
```
╔════════════════════════════════════════════════════════════════╗
║ [STEP 2/5] White Balance Calibration                          ║
╚════════════════════════════════════════════════════════════════╝

📌 INSTRUCTIONS:
   1. Place the robot on a PURE WHITE surface
   2. Make sure NO black line is under any sensor
   3. Keep the robot STATIONARY
   4. Calibration will start in 5 seconds...

   Starting in 5 seconds...
   Starting in 4 seconds...
   ...

========== IR White Balance Calibration ==========
[INFO] Place robot on WHITE surface (no black line)
[INFO] Stabilizing... (1 second)
[INFO] Sampling 100 times (interval: 10ms)...
[INFO] Calibration successful (100/100 samples)
[INFO] White reference values:
  Channel:        0       1       2       3       4       5       6       7
  Value:      265.3   258.7   271.2   263.5   268.1   259.4   270.8   262.0
[SUCCESS] White balance calibration complete!
===================================================
```

**常见失败情况**:

| 错误信息 | 原因 | 解决方案 |
|----------|------|----------|
| `Calibration failed: only XX/100` | 传感器通信故障 | 检查USART2接线和供电 |
| 各通道数值差异>50 | 传感器下方有黑线 | 确认纯白表面 |
| 数值全部为0 | 传感器未启动 | 重新上电 |

---

### Step 3: 黑线阈值校准

**操作**:

1. 将小车居中对准黑线
2. 中间传感器（通道3和4）应正对黑线
3. 保持小车静止

**预期串口输出**:
```
╔════════════════════════════════════════════════════════════════╗
║ [STEP 3/5] Black Line Threshold Calibration                   ║
╚════════════════════════════════════════════════════════════════╝

📌 INSTRUCTIONS:
   1. Place the robot CENTERED on the black line
   2. Center sensors (channels 3-4) should be over the line
   3. Keep the robot STATIONARY

========== IR Black Threshold Calibration ==========
[INFO] Place robot CENTERED on BLACK line
[INFO] Current sensor readings:
  Channel:             0       1       2       3       4       5       6       7
  Raw value:         258     255     160      95     102     248     260     263
  White ref:        265.3   258.7   271.2   263.5   268.1   259.4   270.8   262.0
  Black strength:       7       4     111     169     166      11      11       1
[INFO] Maximum black strength: 169.0
[INFO] Threshold set to: 84.5 (50% of max)
[SUCCESS] Black threshold calibration complete!
====================================================
```

**解读**:
- 通道3-4的黑线强度（~160-170）明显高于其他通道 → ✅ 居中正确
- 阈值 = 最大黑线强度 × 50% = 169 × 0.5 ≈ 84.5
- 警告阈值: 如果最大强度 < 20，说明白平衡未校准或黑线对比度太低

---

### Step 4: 校准结果确认（自动）

**预期串口输出**:
```
========== IR Calibration Configuration ==========
White Reference Values:
  Channel:       0       1       2       3       4       5       6       7
  Value:     265.3   258.7   271.2   263.5   268.1   259.4   270.8   262.0

Black Strength Threshold: 84.5

Sensor Weights:
  Channel:       0       1       2       3       4       5       6       7
  Weight:      3.99    2.85    1.71    0.57   -0.57   -1.71   -2.85   -3.99
===================================================
```

**确认**: 记录这些数值，后续调参时会用到。

---

### Step 5: 实时验证

**操作**:

1. 系统进入60秒监控模式
2. 手动移动小车，观察 `lateral_error` 变化

**符号验证（关键！）**:

| 操作 | 预期 lateral_error | 预期 active_channels |
|------|-------------------|---------------------|
| 小车居中在黑线上 | ≈ 0 (±0.02) | 2-4 (中间通道) |
| 手动向右移小车 | **正值** (> 0) | 左侧通道 (0-3) |
| 手动向左移小车 | **负值** (< 0) | 右侧通道 (4-7) |
| 小车完全离开黑线 | 0 | 0 (LINE LOST) |

**预期串口输出**:
```
========== IR Sensor Real-Time Monitor ==========
[INFO] Monitoring for 60000 ms (interval: 500 ms)

[     0 ms] Sample #1:
  Raw:       262   255   158   102   108   250   265   268
  Strength:    3     5   113   162   160     9     6     1
  Active channels: 3/8
  Lateral error: -0.015
  Status: LINE TRACKING

[   500 ms] Sample #2:
  Raw:       120   135   248   260   262   255   258   261
  Strength:  145   124    23     4     6     4     13     1
  Active channels: 2/8
  Lateral error: +1.847
  Status: LINE TRACKING
```

**⚠️ 关键验证**: 
- 如果向右移动时 `lateral_error` 为**负值** → **权重符号错误**！
  - 修复: `config.c` 第78-80行，所有权重取反
- 如果向左移动时 `lateral_error` 为**正值** → **权重符号错误**！

---

## 3. 校准完成后

### 切换到循迹模式

1. 修改 `Core/Src/freertos.c`:
```c
// #define TEST_MODE_IR_CALIBRATION     /* ← 注释掉 */
#define TEST_MODE_TRACK_CONTROL        /* ← 取消注释 */
```

2. 重新编译、烧录、上电

3. 循迹模式会在初始化时自动调用 `sd_config_reset_defaults()`
   - **注意**: 这会用默认值覆盖校准结果！
   - **解决方法**: 将校准后的值硬编码到 `config.c:364` 和 `config.c:399`

### 将校准值固化到配置

打开 `modules/Sens-Decision/src/config.c`，找到：

```c
// 第364行 - 将你的校准值替换 270.0f
for (index = 0U; index < SD_IR_CHANNEL_COUNT; ++index) {
    g_sens_decision_config.perception.white_reference[index] = 270.0f;  // ← 替换
}

// 第399行 - 将校准阈值替换 50.0f
g_sens_decision_config.perception.black_strength_threshold = 50.0f;  // ← 替换
```

---

## 4. 校准数据记录

### 本次校准记录

**日期**: ________  
**光照条件**: ☐ 室内日光灯  ☐ 室外自然光  ☐ 室内LED

**白平衡校准值**:
| 通道 | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
|------|---|---|---|---|---|---|---|---|
| White Ref | ___ | ___ | ___ | ___ | ___ | ___ | ___ | ___ |

**黑线阈值校准**:
| 最大黑线强度 | 阈值 (50%) |
|-------------|-----------|
| ___ | ___ |

**lateral_error 符号验证**:
- [ ] 向右移动 → lateral_error **正值** ✅
- [ ] 向左移动 → lateral_error **负值** ✅

---

## 5. 故障排查

### Q1: 白平衡校准时成功读取数 < 50/100

**症状**: `[ERROR] Calibration failed: only 23/100 successful reads`

**根因**: IR传感器UART通信不稳定

**排查步骤**:
1. 确认 USART2 波特率 = 115200
2. 检查 PA2(TX) PA3(RX) 接线
3. 检查传感器供电（应为5V ± 0.25V）
4. 检查 USART2 的 RXNEIE 中断是否使能
5. 运行 `ir_uart_diagnostic.c` 检查丢包率

### Q2: 所有通道读数都接近0

**根因**: 传感器未进入模拟输出模式

**解决**:
```c
// 在 IrUartSensor_Init() 之后添加
IrUartSensor_RequestAnalogMode();
osDelay(2000);  // 等待模式切换
```

### Q3: lateral_error符号反向

**症状**: 向右移动时 lateral_error < 0（预期 > 0）

**根因**: IR权重符号与传感器物理安装方向不匹配

**临时解决** (代码级):
- `config.c:78-80` 所有权重取反

**永久解决** (物理级):
- 确认传感器阵列安装方向
- 确认 `config.c:32-39` 的物理位置注释与实际一致

### Q4: 黑线强度最大值始终 < 20

**症状**: `[WARNING] Maximum black strength too low (12.3)`

**根因**:
1. 白平衡未校准（white_reference还是默认值270）
2. 传感器距离地面太远（>30mm）
3. 黑线对比度太低（使用灰色胶带而非黑色）

---

## 6. 参考

- 代码实现: `Core/Src/app/ir_calibration.c`
- 头文件: `Core/Inc/app/ir_calibration.h`
- 测试模式入口: `Core/Src/freertos.c` (TEST_MODE_IR_CALIBRATION)
- 配置参数: `modules/Sens-Decision/src/config.c`
- 感知算法: `modules/Sens-Decision/src/perception.c`

---

**文档版本**: 1.0  
**创建日期**: 2026-07-30  
**适用固件**: v1.2.1
