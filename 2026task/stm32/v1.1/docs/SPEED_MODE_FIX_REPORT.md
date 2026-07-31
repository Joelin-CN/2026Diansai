# 速度参数配置传递修复报告

**日期**: 2026-07-30  
**问题**: 应用层速度配置未传递到决策层，导致小车以高速运行  
**状态**: ✅ 已修复  

---

## 1. 问题诊断

### 1.1 配置流向分析

```
应用层 (track_control_app.c)
  ↓
  g_track_config.line_speed_mps = 0.5f      ← 设置了，但未使用
  g_track_config.curve_speed_mps = 0.3f     ← 设置了，但未使用
  ↓
  ❌ 没有传递到决策层 ❌
  
决策层 (config.c)
  ↓
  g_sens_decision_config.behavior.line_speed_mps = 1.0f       ← 默认值
  g_sens_decision_config.behavior.curve_speed_mps = 0.5f      ← 默认值
  ↓
行为规划器 (behavior_planner.c)
  ↓
  output->speed_limit_mps = g_sens_decision_config.behavior.line_speed_mps
  ↓
轨迹生成器 (trajectory_generate.c)
  ↓
  实际速度: 1.0/0.5 m/s (高速！)
```

### 1.2 根本原因

**应用层和决策层使用了两个独立的速度配置变量**：

1. **应用层配置** (`track_control_app.c:215-216`)：
   ```c
   g_track_config.line_speed_mps = 0.5f;    // 未被行为规划器使用
   g_track_config.curve_speed_mps = 0.3f;   // 未被行为规划器使用
   ```
   - 这个配置只用于 `TrackPath_CorrectOmega()` 的PID增益计算
   - **不影响速度限制**

2. **决策层默认值** (`config.c:525-528`)：
   ```c
   g_sens_decision_config.behavior.line_speed_mps = 1.0f;       // 实际使用的
   g_sens_decision_config.behavior.curve_speed_mps = 0.5f;      // 实际使用的
   ```
   - 行为规划器从这里读取速度限制
   - **应用层的配置完全没有传递到这里**

3. **行为规划器读取** (`behavior_planner.c:146-152`)：
   ```c
   case BEHAVIOR_STATE_LINE_FOLLOW:
       output->speed_limit_mps = g_sens_decision_config.behavior.line_speed_mps;  // ← 读取决策层默认值
       break;
   case BEHAVIOR_STATE_CURVE:
       output->speed_limit_mps = g_sens_decision_config.behavior.curve_speed_mps; // ← 读取决策层默认值
       break;
   ```

### 1.3 影响

- **预期速度**: 0.5 m/s (直线) / 0.3 m/s (弯道) - 安全调试速度
- **实际速度**: 1.0 m/s (直线) / 0.5 m/s (弯道) - 高速运行
- **后果**: 首次调试时小车速度过快，可能冲出轨道

---

## 2. 修复方案设计

### 2.1 设计思路

采用**方案A + B**（应用层配置覆盖 + 速度模式枚举）：

1. **创建速度模式API**：统一管理速度配置
2. **在应用层初始化时调用**：将速度配置写入决策层
3. **支持多种预设模式**：便于不同调试阶段快速切换

### 2.2 速度模式设计

| 模式 | 直线速度 | 接近弯道 | 弯道速度 | 使用场景 |
|------|----------|----------|----------|----------|
| `DEBUG` | 0.2 m/s | 0.18 m/s | 0.15 m/s | 首次调试：验证传感器、lateral_error符号 |
| `SLOW` | 0.5 m/s | 0.4 m/s | 0.3 m/s | 常规调试：PID参数调优 |
| `NORMAL` | 1.0 m/s | 0.7 m/s | 0.5 m/s | 正常运行：验证通过后 |
| `FAST` | 1.5 m/s | 1.0 m/s | 0.8 m/s | 竞速模式：高性能运行 |

### 2.3 实现架构

```
speed_mode.h/c (新增)
  ↓
  speed_mode_set(SPEED_MODE_DEBUG)
  ↓
  直接写入 g_sens_decision_config.behavior.*
  ↓
track_control_app.c (修改)
  ↓
  TrackControlApp_Init() 调用 speed_mode_set()
  ↓
行为规划器 (无需修改)
  ↓
  继续从 g_sens_decision_config.behavior 读取
```

---

## 3. 修复实施

### 3.1 新增文件

#### ✅ `Core/Inc/app/speed_mode.h`
速度模式接口定义：
- `speed_mode_t` 枚举：4种预设模式
- `speed_mode_set()`: 设置速度模式
- `speed_mode_get()`: 查询当前模式
- `speed_mode_name()`: 获取模式名称

#### ✅ `Core/Src/app/speed_mode.c`
速度模式实现：
- 根据模式设置 `g_sens_decision_config.behavior.*` 的速度参数
- 打印当前速度配置到UART（便于调试）

### 3.2 修改文件

#### ✅ `Core/Src/app/track_control_app.c`
1. 添加头文件：`#include "speed_mode.h"`
2. 在 `TrackControlApp_Init()` 中添加速度模式设置：
   ```c
   /* Step 10b: Set speed mode (overrides Sens-Decision default speeds) */
   speed_mode_set(SPEED_MODE_DEBUG);  /* ← CHANGE THIS LINE TO ADJUST SPEED */
   printf("[TrackControlApp] Active speed mode: %s\n", speed_mode_name(speed_mode_get()));
   ```
3. 更新 `g_track_config` 注释，说明这些值不影响行为规划器

#### ✅ `CMakeLists.txt`
添加编译单元：
```cmake
Core/Src/app/speed_mode.c
```

---

## 4. 使用指南

### 4.1 快速切换速度模式

修改 `track_control_app.c` 第226行：

```c
// 首次调试（超低速）
speed_mode_set(SPEED_MODE_DEBUG);

// 传感器验证通过后（低速调试）
speed_mode_set(SPEED_MODE_SLOW);

// PID调优完成后（正常速度）
speed_mode_set(SPEED_MODE_NORMAL);

// 竞速模式（高性能）
speed_mode_set(SPEED_MODE_FAST);
```

### 4.2 调试流程建议

**阶段1：首次上电 (DEBUG模式)**
```
速度：0.2/0.15 m/s
目的：验证传感器数据正确性
检查项：
  ✓ IR传感器8通道ADC值是否正常
  ✓ lateral_error符号是否正确（向右移动→负值，向左移动→正值）
  ✓ 编码器计数是否正常
  ✓ 电机PWM输出是否合理
```

**阶段2：传感器验证通过 (SLOW模式)**
```
速度：0.5/0.3 m/s
目的：PID参数调优
检查项：
  ✓ 循迹是否稳定
  ✓ 弯道是否顺畅
  ✓ 是否有超调/振荡
  ✓ lateral_gain / heading_gain是否合适
```

**阶段3：控制验证通过 (NORMAL模式)**
```
速度：1.0/0.5 m/s
目的：正常运行验证
检查项：
  ✓ 能否完成多圈循迹
  ✓ 速度切换是否平滑
  ✓ 长时间运行稳定性
```

**阶段4：竞赛优化 (FAST模式)**
```
速度：1.5/0.8 m/s
目的：高性能运行
检查项：
  ✓ 是否能维持高速稳定
  ✓ 弯道是否需要更激进的减速
  ✓ 是否需要微调PID参数
```

### 4.3 UART输出示例

成功设置速度模式后，会打印以下信息：
```
[TrackControlApp] Step 10b: Configuring speed mode...
[SpeedMode] DEBUG: line=0.2, approach=0.18, curve=0.15 m/s
[SpeedMode] Ultra-low speed for sensor verification
[TrackControlApp] Active speed mode: DEBUG
```

---

## 5. 测试验证计划

### 5.1 编译验证

```bash
cd build
cmake ..
make
```

预期结果：
- ✅ 编译成功，无警告
- ✅ `speed_mode.c` 正确链接

### 5.2 架空测试

1. **烧录固件**（DEBUG模式）
2. **架空车轮，启动小车**
3. **观察UART输出**：
   ```
   [SpeedMode] DEBUG: line=0.2, approach=0.18, curve=0.15 m/s
   ```
4. **手动转动车轮**，观察目标速度：
   - 直线段：左右轮目标速度应为 ±0.2 m/s
   - 弯道检测后：目标速度应降至 ±0.15 m/s

### 5.3 落地测试

1. **将小车放在黑线起点**
2. **启动循迹**
3. **观察运行速度**：
   - 应以极慢速度前进（~0.2 m/s）
   - 便于观察传感器数据和控制行为
4. **验证速度切换**：
   - 修改为 `SPEED_MODE_SLOW`，重新烧录
   - 观察速度提升至 ~0.5 m/s

### 5.4 速度测量验证

使用编码器数据验证实际速度：

```c
// 在 RunFastCycle() 中添加调试输出
if ((g_cycle_counter % 50U) == 0U) {  // 每秒一次（50Hz）
    printf("[SpeedDebug] Target: %.2f m/s, Actual: %.2f m/s\n",
           g_trajectory.v,
           g_state_evaluator.state.v);
}
```

预期输出（DEBUG模式）：
```
[SpeedDebug] Target: 0.20 m/s, Actual: 0.19 m/s  // 直线
[SpeedDebug] Target: 0.15 m/s, Actual: 0.14 m/s  // 弯道
```

---

## 6. 修复前后对比

### 修复前

| 配置位置 | 配置值 | 是否生效 |
|----------|--------|----------|
| `track_control_app.c` | 0.5 / 0.3 m/s | ❌ 未传递 |
| `config.c` (默认值) | 1.0 / 0.5 m/s | ✅ 实际使用 |

**结果**：小车以高速运行，首次调试危险！

### 修复后

| 配置方式 | 速度值 | 是否生效 |
|----------|--------|----------|
| `speed_mode_set(DEBUG)` | 0.2 / 0.15 m/s | ✅ 覆盖默认值 |
| `speed_mode_set(SLOW)` | 0.5 / 0.3 m/s | ✅ 覆盖默认值 |
| `speed_mode_set(NORMAL)` | 1.0 / 0.5 m/s | ✅ 覆盖默认值 |
| `speed_mode_set(FAST)` | 1.5 / 0.8 m/s | ✅ 覆盖默认值 |

**结果**：应用层配置正确传递，支持多种速度模式！

---

## 7. 技术细节

### 7.1 为什么直接修改全局配置？

```c
void speed_mode_set(speed_mode_t mode) {
    // 直接写入决策层全局配置
    g_sens_decision_config.behavior.line_speed_mps = 0.2f;
    g_sens_decision_config.behavior.curve_speed_mps = 0.15f;
}
```

**原因**：
1. `g_sens_decision_config` 是全局单例配置
2. 行为规划器直接从这里读取速度限制
3. 无需修改现有架构，最小侵入性修复

### 7.2 为什么不修改行为规划器？

**方案对比**：

| 方案 | 优点 | 缺点 |
|------|------|------|
| **修改行为规划器接口** | 架构更清晰 | 需要修改多处代码，影响范围大 |
| **覆盖全局配置（当前）** | 最小修改，即刻生效 | 依赖全局变量 |

**选择当前方案的原因**：
- 修复范围小（3个新文件 + 2处修改）
- 不破坏现有架构
- 便于回退

### 7.3 速度模式是否支持运行时切换？

**当前实现**：初始化时设置，运行期间不变

**如需运行时切换**（可选扩展）：
```c
// 在 track_control_app.c 添加命令处理
void TrackControlApp_HandleCommand(char cmd) {
    switch (cmd) {
        case '1': speed_mode_set(SPEED_MODE_DEBUG); break;
        case '2': speed_mode_set(SPEED_MODE_SLOW); break;
        case '3': speed_mode_set(SPEED_MODE_NORMAL); break;
        case '4': speed_mode_set(SPEED_MODE_FAST); break;
    }
}
```

通过UART发送 `1`/`2`/`3`/`4` 即可切换速度。

---

## 8. 潜在问题与注意事项

### 8.1 ⚠️ `g_track_config` 的速度参数不再使用

修复后，`track_control_app.c` 中的这两行配置已**不再影响速度限制**：
```c
g_track_config.line_speed_mps = 0.5f;    // 仅作为参考值
g_track_config.curve_speed_mps = 0.3f;   // 仅作为参考值
```

**建议**：
- 保留这些字段（未来可能用于其他用途）
- 或者删除，避免混淆

### 8.2 ⚠️ 速度模式与PID增益的关系

不同速度下，PID参数可能需要调整：

| 速度模式 | 建议lateral_gain | 建议heading_gain |
|----------|------------------|------------------|
| DEBUG (0.2 m/s) | 1.0 ~ 1.5 | 0.8 ~ 1.0 |
| SLOW (0.5 m/s) | 1.5 ~ 2.0 | 1.0 ~ 1.5 |
| NORMAL (1.0 m/s) | 2.0 ~ 2.5 | 1.5 ~ 2.0 |
| FAST (1.5 m/s) | 2.5 ~ 3.0 | 2.0 ~ 2.5 |

**原因**：高速下需要更强的控制增益以抑制偏差。

### 8.3 ⚠️ approach_curve_speed 的作用

`approach_curve_speed_mps` 是**接近弯道时的减速目标**：

```
直线 (LINE_FOLLOW) → 检测到弯道 → 接近弯道 (APPROACH_CURVE) → 进入弯道 (CURVE)
  1.0 m/s              开始减速           0.7 m/s              0.5 m/s
```

设置原则：
- `line_speed > approach_speed > curve_speed`
- 梯度减速，避免急刹

---

## 9. 后续优化建议

### 9.1 速度配置持久化（可选）

将速度模式保存到Flash，重启后自动恢复：
```c
// 保存到Flash
save_speed_mode_to_flash(SPEED_MODE_SLOW);

// 启动时读取
speed_mode_t saved_mode = load_speed_mode_from_flash();
speed_mode_set(saved_mode);
```

### 9.2 自适应速度调整（高级）

根据循迹质量动态调整速度：
```c
if (lateral_error_large) {
    // 偏差大时减速
    speed_scale_factor = 0.8f;
}
```

### 9.3 速度模式热切换（便利性）

通过UART命令或按键实时切换速度，无需重新烧录。

---

## 10. 总结

### ✅ 修复完成项

- [x] 诊断配置传递路径
- [x] 设计速度模式枚举
- [x] 实现速度模式API
- [x] 修改应用层初始化
- [x] 更新CMakeLists.txt
- [x] 编写使用文档

### 📋 待测试项

- [ ] 编译验证
- [ ] 架空测试（验证速度配置生效）
- [ ] 落地测试（DEBUG模式循迹）
- [ ] 速度切换测试（SLOW/NORMAL/FAST）

### 🎯 核心改进

1. **问题修复**：应用层速度配置现在正确传递到决策层
2. **便捷切换**：一行代码即可切换速度模式
3. **安全调试**：首次运行默认超低速（0.2 m/s）
4. **灵活扩展**：支持自定义速度配置

---

**修复完成时间**: 2026-07-30  
**修复作者**: Claude (Kiro)  
**版本**: v1.0.0
