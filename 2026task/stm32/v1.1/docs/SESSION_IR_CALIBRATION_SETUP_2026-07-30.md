# 会话总结：IR传感器校准测试模式实现

**日期**: 2026-07-30  
**会话目标**: 为循迹小车添加独立的IR传感器校准测试模式  
**状态**: ✅ 完成

---

## 执行摘要

为STM32循迹小车系统（v1.2.1）成功实现了**独立的IR传感器校准测试模式**，用户可以通过编译开关在"IR校准模式"和"完整循迹模式"之间切换。

### 核心成果
1. ✅ 修改 `Core/Src/freertos.c`，添加编译开关和校准流程
2. ✅ 创建详细操作手册（15页）
3. ✅ 创建快速参考卡（1页）
4. ✅ 代码验证通过（无编译依赖问题）

---

## 代码修改清单

### 修改的文件

| 文件 | 修改内容 | 行数变化 |
|------|---------|---------|
| `Core/Src/freertos.c` | 添加TEST_MODE开关和校准流程 | +209行 |

### 新增的文件

| 文件 | 类型 | 用途 |
|------|------|------|
| `docs/IR_CALIBRATION_PROCEDURE_2026-07-30.md` | 文档 | 详细操作手册（15页） |
| `docs/QUICK_START_IR_CALIBRATION.md` | 文档 | 快速参考卡（1页） |
| `docs/SESSION_IR_CALIBRATION_SETUP_2026-07-30.md` | 文档 | 本会话总结 |

---

## 功能设计

### 测试模式开关机制

**位置**: `Core/Src/freertos.c` 第65-66行

```c
#define TEST_MODE_IR_CALIBRATION     /* ← 当前激活：IR校准模式 */
// #define TEST_MODE_TRACK_CONTROL   /* ← 注释掉：完整循迹模式 */
```

**切换方法**:
1. 注释掉 `TEST_MODE_IR_CALIBRATION`
2. 取消注释 `TEST_MODE_TRACK_CONTROL`
3. 重新编译、烧录

### IR校准流程设计

```
┌─────────────────────────────────────────────────────────────┐
│ IR校准模式 (TEST_MODE_IR_CALIBRATION)                        │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│ Step 1: 硬件初始化 (自动, 30秒)                              │
│   ├─ Motor_Init() + Motor_Stop()                           │
│   ├─ Encoder_Init()                                         │
│   ├─ IrUartSensor_Init()                                    │
│   ├─ IrUartSensor_RequestAnalogMode()                       │
│   └─ sd_config_reset_defaults()                             │
│                                                              │
│ Step 2: 白平衡校准 (需操作, 5分钟)                           │
│   ├─ 用户操作: 放置小车在纯白表面                            │
│   ├─ 倒计时: 5秒                                            │
│   ├─ IrCalibration_WhiteBalance()                           │
│   │   ├─ 采样100次 (间隔10ms)                               │
│   │   ├─ 计算8通道平均值                                    │
│   │   └─ 更新 g_sens_decision_config.perception.white_ref  │
│   └─ 等待用户查看结果: 5秒                                   │
│                                                              │
│ Step 3: 黑线阈值校准 (需操作, 2分钟)                         │
│   ├─ 用户操作: 放置小车居中对准黑线                          │
│   ├─ 倒计时: 5秒                                            │
│   ├─ IrCalibration_BlackThreshold()                         │
│   │   ├─ 读取当前传感器值                                   │
│   │   ├─ 计算黑线强度 (white_ref - raw)                     │
│   │   ├─ 找到最大黑线强度                                   │
│   │   └─ 设置阈值 = max_strength × 50%                      │
│   └─ 等待用户查看结果: 5秒                                   │
│                                                              │
│ Step 4: 校准结果显示 (自动, 查看)                            │
│   ├─ IrCalibration_PrintConfig()                            │
│   │   ├─ 显示白色参考值 (8通道)                             │
│   │   ├─ 显示黑线阈值                                       │
│   │   └─ 显示传感器权重                                     │
│   └─ 等待用户查看: 5秒                                       │
│                                                              │
│ Step 5: 实时验证 (需操作, 5-60分钟)                          │
│   ├─ 用户操作: 手动左右移动小车                              │
│   ├─ IrCalibration_Monitor(60000, 500)                      │
│   │   ├─ 每500ms输出一次传感器状态                           │
│   │   ├─ 显示: Raw值, 黑线强度, lateral_error                │
│   │   └─ 运行60秒                                           │
│   ├─ 用户验证lateral_error符号:                              │
│   │   ├─ 右移 → lateral_error > 0 ✅                        │
│   │   └─ 左移 → lateral_error < 0 ✅                        │
│   └─ 完成提示 + 下一步指引                                   │
│                                                              │
│ Loop: 持续监控模式 (可选)                                    │
│   └─ for(;;) { IrCalibration_Monitor(30000, 500) }          │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

---

## 关键设计决策

### 决策1: 为什么使用编译开关而非运行时切换？

**理由**:
1. **简单可靠**: 编译时确定，无运行时分支判断开销
2. **代码隔离**: 校准模式不会意外启动电机
3. **内存节省**: 未使用的模式代码可以被优化掉
4. **符合现有架构**: 项目已有多个测试工具使用类似模式

**替代方案（未采用）**:
- 串口命令交互式菜单 → 复杂度高，需要额外的命令解析器
- GPIO按键选择模式 → 需要额外硬件，不适合调试阶段

### 决策2: 为什么校准流程不支持跳过某个步骤？

**理由**:
1. **强制顺序**: 黑线阈值校准**必须**在白平衡之后
2. **防止错误**: 跳过白平衡会导致阈值校准失败
3. **用户体验**: 线性流程更清晰，减少操作失误

**灵活性保留**:
- 如果只需重新校准阈值，可以重启系统直接跳过前面倒计时（手动重置）
- 或修改 `freertos.c` 注释掉不需要的步骤

### 决策3: 为什么监控模式运行60秒后进入无限循环？

**理由**:
1. **充分验证**: 60秒足够手动测试多次左右移动
2. **持续监控**: 无限循环允许长时间观察传感器稳定性
3. **退出简单**: 用户可随时按Reset键退出

---

## 验证清单

### 编译验证
- [x] `Core/Src/freertos.c` 语法正确
- [x] 所有头文件已包含（ir_calibration.h, config.h等）
- [x] 无未定义的函数引用
- [x] TEST_MODE开关逻辑正确（互斥）

### 功能验证（待实车测试）
- [ ] 硬件初始化成功（无FATAL错误）
- [ ] 白平衡校准成功率 > 50%
- [ ] 黑线阈值校准完成（最大强度 > 20）
- [ ] lateral_error符号正确（右移→正值，左移→负值）
- [ ] 持续监控模式正常运行

### 文档验证
- [x] 操作手册完整（覆盖所有步骤）
- [x] 快速参考卡精简实用（1页纸）
- [x] 故障排查覆盖常见问题
- [x] 预期输出示例清晰

---

## 使用指南（快速版）

### 场景1: 首次IR校准

```bash
# 1. 修改代码
# 编辑 Core/Src/freertos.c:65
#define TEST_MODE_IR_CALIBRATION

# 2. 编译烧录
# 使用 Keil / CMake 构建

# 3. 连接串口
# 115200,8,N,1

# 4. 按照串口提示操作
# Step 2: 放白色表面
# Step 3: 放黑线上
# Step 5: 手动移动验证

# 5. 记录校准值
# 从 Step 4 的输出中记录 white_reference 和 threshold

# 6. 切换到循迹模式
# 修改 freertos.c:65-66
// #define TEST_MODE_IR_CALIBRATION
#define TEST_MODE_TRACK_CONTROL

# 7. 重新编译烧录
```

### 场景2: 光照变化后重新校准

```bash
# 直接运行Step 1-5即可
# 无需修改代码（TEST_MODE_IR_CALIBRATION已激活）
```

---

## 后续工作建议

### 优先级P0（立即）
1. **实车测试校准流程**
   - 验证白平衡校准成功率
   - 验证lateral_error符号正确性
   - 记录实际校准值

2. **固化校准参数**
   - 将校准值写入 `config.c:364` (white_reference)
   - 将阈值写入 `config.c:399` (black_strength_threshold)

### 优先级P1（校准后）
3. **切换到循迹模式**
   - 修改 `freertos.c` 开关
   - 开始PID调优

4. **更新CHANGELOG**
   - 记录本次修改到 `CHANGELOG.md`
   - 标记为 v1.2.2 (Minor: 新增校准测试模式)

### 优先级P2（可选）
5. **增强校准工具**
   - 添加串口命令交互（跳过倒计时、重复某步骤）
   - 校准结果自动保存到Flash（无需手动固化）

6. **自动化测试**
   - 编写Python脚本自动解析串口输出
   - 生成校准报告（CSV格式）

---

## 风险与限制

### 已知限制

1. **校准值仅在RAM中**
   - 重启后丢失，需要重新校准
   - **解决方案**: 固化到 `config.c` 或使用Flash存储

2. **无法跳过步骤**
   - 必须按 Step 1→2→3→4→5 顺序执行
   - **解决方案**: 手动注释掉不需要的步骤代码

3. **倒计时无法中断**
   - 5秒倒计时期间无法提前开始
   - **解决方案**: 修改 `freertos.c` 中的倒计时秒数

### 潜在风险

| 风险 | 概率 | 影响 | 缓解措施 |
|------|------|------|---------|
| 白平衡校准失败（成功率<50%） | Medium | High | 详细故障排查文档已提供 |
| lateral_error符号反向 | Low | High | Step 5验证环节可检测，文档有修复方法 |
| 用户忘记切换回循迹模式 | High | Low | 完成提示明确指出下一步操作 |

---

## 附录

### A. 相关文件索引

**代码文件**:
- `Core/Src/freertos.c` - 主任务入口，测试模式开关
- `Core/Src/app/ir_calibration.c` - IR校准工具实现
- `Core/Inc/app/ir_calibration.h` - IR校准工具头文件
- `modules/Sens-Decision/src/config.c` - 传感器配置参数

**文档文件**:
- `docs/IR_CALIBRATION_PROCEDURE_2026-07-30.md` - 详细操作手册
- `docs/QUICK_START_IR_CALIBRATION.md` - 快速参考卡
- `docs/SESSION_IR_CALIBRATION_SETUP_2026-07-30.md` - 本文档
- `API_PITFALLS_GUIDE.md` - API调试避坑指南
- `CHANGELOG.md` - 版本变更历史

### B. 编译依赖

**必需的头文件**:
```c
#include "ir_calibration.h"    // IR校准工具
#include "ir_uart_sensor.h"    // IR传感器驱动
#include "motor.h"              // 电机驱动
#include "encoder.h"            // 编码器驱动
#include "config.h"             // 传感器配置
#include "platform_time.h"      // 时间戳
```

**必需的源文件** (确保已添加到工程):
- `Core/Src/app/ir_calibration.c`
- `modules/IR-tracker/src/ir_uart_sensor.c`

### C. 预期串口输出（完整示例）

```
╔════════════════════════════════════════════════════════════════╗
║       IR Sensor Calibration Mode - STM32 Track Robot v1.2.1   ║
╚════════════════════════════════════════════════════════════════╝

[STEP 1/5] Initializing hardware...
[INFO] Initializing IR sensor (USART2, 115200 baud)...
[INFO] Waiting for IR sensor warm-up (2 seconds)...
[SUCCESS] Hardware initialization complete!

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
   Starting in 3 seconds...
   Starting in 2 seconds...
   Starting in 1 seconds...

========== IR White Balance Calibration ==========
[INFO] Place robot on WHITE surface (no black line)
[INFO] Stabilizing... (1 second)
[INFO] Sampling 100 times (interval: 10ms)...
[INFO] Calibration successful (98/100 samples)
[INFO] White reference values:
  Channel:        0       1       2       3       4       5       6       7
  Value:      263.2   259.8   268.5   265.1   270.3   261.7   267.9   264.5
[SUCCESS] White balance calibration complete!
===================================================

⏸️  Press any key via UART to continue (or wait 5 seconds)...

[... 其余步骤类似 ...]
```

---

## 总结

本次会话成功为STM32循迹小车v1.2.1添加了**独立的IR传感器校准测试模式**，包括：

✅ **代码实现**: 编译开关 + 5步自动化校准流程  
✅ **用户体验**: 清晰的串口提示 + 倒计时引导  
✅ **文档完善**: 详细手册 + 快速参考卡 + 故障排查  
✅ **质量保证**: 符号验证环节 + 持续监控模式  

**下一步**: 实车测试校准流程 → 记录校准值 → 固化参数 → 切换到循迹模式

---

**文档版本**: 1.0  
**创建日期**: 2026-07-30  
**会话耗时**: ~1小时  
**主要贡献者**: 主持人Claude (Opus 4.8)
