# v1.2.0 更新后快速开始指南

**版本**: v1.2.0  
**日期**: 2026-07-30  
**适用对象**: 从v1.0.0/v1.1.0升级到v1.2.0的用户

---

## 概述

v1.2.0 修复了4个关键问题：
1. **传感器配置双轮迁移** - 修复初始化必然失败
2. **初始化失败处理** - 防止传感器故障时运行
3. **红外传感器算法** - 修复黑线检测完全失效
4. **速度配置传递** - 修复应用层配置未生效

**这是一个关键修复版本，升级后必须执行以下步骤！**

---

## 必须执行的步骤

### 第1步: 重新编译固件 ⚠️ 必须！

v1.2.0 修改了17个文件，新增9个文件，必须完全重新编译：

```bash
cd build
rm -rf *          # 清理旧编译输出
cmake ..          # 重新配置
make -j4          # 编译
```

**预期结果**:
- ✅ 编译成功，0个错误
- ✅ 0个警告
- ✅ 固件文件生成

**检查项**: 确认以下新文件被编译：
- `Core/Src/app/ir_calibration.c`
- `modules/Sens-Decision/src/perception_debug.c`
- `Core/Src/app/speed_mode.c`

---

### 第2步: 红外传感器校准 ⚠️ 必须！

v1.2.0 修改了黑线检测算法（从反向阈值到黑线强度反转），**必须重新校准**，否则循迹功能完全失效！

#### 2.1 白平衡校准

**步骤**:
1. 将小车放在**纯白色背景**上（例如白色桌面上，IR传感器阵列下方无黑线）
2. 连接串口（115200 bps），观察输出
3. 在初始化代码中添加校准调用：

```c
// 在 TrackControlApp_Init() 中
#include "ir_calibration.h"

// 白平衡校准
IrCalibration_WhiteBalance();
```

4. 重新编译并烧录
5. 观察串口输出：
```
[IR Calibration] White balance calibration...
[IR Calibration] Channel 0: 270, Channel 1: 268, ... 
[IR Calibration] White reference saved.
```

#### 2.2 黑线阈值校准

**步骤**:
1. 将小车放在**黑线中心**上（IR传感器阵列下方有黑线）
2. 添加校准调用：

```c
// 黑线阈值校准
IrCalibration_BlackThreshold();
```

3. 重新编译并烧录
4. 观察串口输出：
```
[IR Calibration] Black threshold calibration...
[IR Calibration] Black strength: [calculated values]
[IR Calibration] Threshold set to: [value]
```

#### 2.3 验证校准结果

```c
#include "perception_debug.h"

// 在500Hz循环中添加
PerceptionDebug_PrintIrStatus();
```

观察输出，确认：
- ✅ 白色背景：所有传感器 `black_strength` 接近0
- ✅ 黑线上方：对应位置传感器 `black_strength > threshold`
- ✅ `lateral_error` 符号正确（偏右为负，偏左为正）

---

### 第3步: 速度模式确认

v1.2.0 默认为 **DEBUG模式（0.2 m/s）**，确保首次调试安全。

#### 3.1 确认当前模式

上电后观察UART输出：
```
[TrackControlApp] Step 10b: Configuring speed mode...
[SpeedMode] DEBUG: line=0.2, approach=0.18, curve=0.15 m/s
[SpeedMode] Ultra-low speed for sensor verification
[TrackControlApp] Active speed mode: DEBUG
```

如果你看到的是 `SLOW`、`NORMAL` 或 `FAST`，说明有人修改了配置。

#### 3.2 如需更换模式

修改 `Core/Src/app/track_control_app.c` 第226行（约）:

```c
// 首次调试（超低速，验证传感器和循迹）
speed_mode_set(SPEED_MODE_DEBUG);

// 传感器验证通过后（低速调试，PID调优）
// speed_mode_set(SPEED_MODE_SLOW);

// PID调优完成后（正常速度）
// speed_mode_set(SPEED_MODE_NORMAL);

// 竞速模式（高性能）
// speed_mode_set(SPEED_MODE_FAST);
```

4种速度模式对照表：

| 模式 | 直线速度 | 弯道速度 | 使用场景 |
|------|----------|----------|----------|
| DEBUG | 0.2 m/s | 0.15 m/s | 首次调试：验证传感器、lateral_error符号 |
| SLOW | 0.5 m/s | 0.3 m/s | 常规调试：PID参数调优 |
| NORMAL | 1.0 m/s | 0.5 m/s | 正常运行：验证通过后 |
| FAST | 1.5 m/s | 0.8 m/s | 竞速模式：高性能运行 |

---

### 第4步: 传感器初始化验证

上电后观察UART输出，确认所有传感器初始化成功：

```
========== Sensor Initialization Start ==========
[SensDecision] HAL configuration: OK
[SensDecision] Config validation: OK
[SensDecision] Initializing sensor 0 (Encoder_Left)...
[SensDecision] Sensor 0 (Encoder_Left): OK
[SensDecision] Initializing sensor 1 (Encoder_Right)...
[SensDecision] Sensor 1 (Encoder_Right): OK
[SensDecision] Initializing sensor 2 (IMU)...
[SensDecision] Sensor 2 (IMU): OK
[SensDecision] Initializing sensor 3 (IR_Array)...
[SensDecision] Sensor 3 (IR_Array): OK
========== All sensors initialized successfully ==========
```

**如果看到错误**：
```
[ERROR] Sensor X (SensorName) initialization failed: ErrorCode
[FATAL] Sensor initialization failed
[FATAL] Please check:
  1. Encoder connections (TIM3=Left, TIM4=Right)
  2. IR sensor UART (USART2, 115200 baud)
  3. IMU SPI connection (SPI2, ICM42688)
  4. Sensor configuration (Sens-Decision/config.c)
  5. Hardware power supply and connections
```

**排查步骤**：
1. 检查对应传感器的硬件连接
2. 检查供电（万用表测量传感器供电电压）
3. 检查配置参数（`config.c`）
4. 运行诊断工具：
   ```c
   sensors_diagnostic_report();
   ```

---

## 推荐调试流程

### 阶段1: 首次上电验证 (预计15分钟)

```
1. 重新编译固件                  ← 5分钟
2. 烧录固件                      ← 2分钟
3. 观察UART初始化输出             ← 3分钟
4. 确认4个传感器初始化成功         ← 2分钟
5. 确认速度模式为DEBUG            ← 3分钟
```

### 阶段2: 红外传感器校准 (预计20分钟)

```
1. 白平衡校准                    ← 5分钟
2. 阈值校准                      ← 5分钟
3. 验证检测状态                   ← 5分钟
4. 横向偏移黑线验证lateral_error  ← 5分钟
```

### 阶段3: 架空测试 (预计15分钟)

```
1. 架空车轮                      ← 1分钟
2. 启动循迹                      ← 2分钟
3. 观察目标速度（0.2 m/s）       ← 5分钟
4. 观察编码器反馈                ← 5分钟
5. 停止测试                      ← 2分钟
```

### 阶段4: 落地测试 (预计30分钟)

```
1. 准备安全环境（防冲出保护）     ← 5分钟
2. 将小车放在黑线起点             ← 1分钟
3. DEBUG模式循迹测试              ← 10分钟
4. 观察稳定性和加速度              ← 10分钟
5. 切换到SLOW模式验证             ← 4分钟
```

**总计预估时间**: 约80分钟

---

## 常见问题

### Q1: 编译失败，提示找不到新文件

**原因**: CMakeLists.txt未更新

**解决**: 确认 `CMakeLists.txt` 中包含了以下文件和路径：
```cmake
Core/Src/app/ir_calibration.c
Core/Src/app/perception_debug.c
Core/Src/app/speed_mode.c
Core/Inc/app/
```

### Q2: 初始化日志显示"Config validation: [FAIL]"

**原因**: 编码器配置不兼容v1.1.0

**解决**: 确认以下文件中编码器枚举已更新：
- `config.h`: `ENCODER_LEFT`, `ENCODER_RIGHT` (+ `INVALID_ENCODER_INDEX`)
- `interface.c`: 传感器初始化表只包含4个对象
- `config.c`: 编码器配置只填充2个

### Q3: 红外传感器一直没有检测到黑线

**原因**: 未执行校准

**解决**:
1. 执行白平衡校准（第2.1节）
2. 执行阈值校准（第2.2节）
3. 验证校准结果（第2.3节）

### Q4: 小车速度很慢，感觉在执行慢动作

**说明**: 这是正常现象！v1.2.0默认DEBUG模式（0.2 m/s），确保首次调试安全。

**解决**: 传感器验证通过后，切换到SLOW或NORMAL模式（第3.2节）

### Q5: 初始化成功1秒后小车停止

**原因**: 可能是传感器在运行时发生故障

**排查**:
1. 观察UART是否有健康监测警告：
   ```
   [WARN] Left encoder not responding
   ```
2. 运行诊断工具：
   ```c
   sensors_diagnostic_report();
   ```
3. 检查连接线是否松动

### Q6: 之前v1.1.0的PID参数还适用吗？

**不完全适用**。v1.2.0的速度范围发生了变化：
- v1.1.0: 固定速度 1.0/0.5 m/s
- v1.2.0: 可变速度 0.2-1.5 m/s

**建议**: 从低增益开始，逐步增加：
```
DEBUG模式 (0.2 m/s): lateral_gain = 1.0-1.5, heading_gain = 0.8-1.0
SLOW模式  (0.5 m/s): lateral_gain = 1.5-2.0, heading_gain = 1.0-1.5
NORMAL模式(1.0 m/s): lateral_gain = 2.0-2.5, heading_gain = 1.5-2.0
FAST模式  (1.5 m/s): lateral_gain = 2.5-3.0, heading_gain = 2.0-2.5
```

---

## 安全注意事项

### ⚠️ 首次调试必须使用DEBUG模式

- 速度：0.2 m/s（约每秒20cm）
- 准备紧急停止：
  - 串口发送停止命令
  - 或直接断开电源
  - 或用手挡住小车

### ⚠️ 必须在安全区域测试

- 桌面或地面，有足够空间
- 边缘有围挡，防止冲出掉落
- 建议先用胶带标出测试区域

### ⚠️ 逐步提升速度

- 不要在未验证DEBUG模式的情况下直接使用FAST模式
- 每个模式验证至少完成3圈稳定循迹
- 提升速度前确认PID参数已调优

---

## 升级前备份

### 备份当前固件

```bash
# 备份当前可用的固件
cp build/*.bin backup/v1.1.0_backup.bin
cp build/*.hex backup/v1.1.0_backup.hex
```

### 备份当前配置

```bash
# 备份配置文件
cp modules/Sens-Decision/src/config.c backup/config_v1.1.0.c
cp modules/Sens-Decision/inc/config.h backup/config_v1.1.0.h
```

### 回退方法

如果v1.2.0出现问题，回退到v1.1.0：
```bash
git checkout v1.1.0
cd build && cmake .. && make -j4
# 烧录 backup/v1.1.0_backup.bin
```

---

## 下一步学习资料

更新完成后的参考文档：

| 文档 | 用途 |
|------|------|
| `docs/V1.2.0_FIX_SUMMARY.md` | 完整修复总结（4个问题详解） |
| `docs/SPEED_MODE_FIX_REPORT.md` | 速度模式详细说明 |
| `docs/IR_SENSOR_FIX_2026-07-30.md` | 红外传感器修复报告 |
| `docs/IR_SENSOR_QUICK_FIX_GUIDE.md` | 红外传感器快速修复指南 |
| `docs/INITIALIZATION_FIX_SUMMARY.md` | 初始化修复总结 |
| `API_PITFALLS_GUIDE.md` | 模块调试避坑指南（含最新章节） |
| `CHANGELOG.md` | 版本变更历史 |

---

**指南创建时间**: 2026-07-30  
**版本**: v1.2.0  
**适用固件**: v1.2.0
