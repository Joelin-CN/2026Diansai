# 电机调试工具开发日志

**日期**: 2026-07-29  
**模块**: Motor & Encoder Debug Tool  
**状态**: ✅ 代码开发完成，待硬件验证  

---

## 📋 任务目标

开发电机和编码器调试工具，用于验证：
1. TB6612电机驱动是否正常输出PWM
2. 左右电机能否正常旋转（正转、反转、停止）
3. 编码器反馈是否随电机旋转正确变化
4. 电机方向和编码器方向是否匹配

---

## 🎯 设计方案

### 测试序列（自动化状态机）

| 阶段 | 测试内容 | 持续时间 | 预期结果 |
|------|---------|---------|----------|
| 1. PWM输出测试 | 左右电机30% PWM | 2秒 | 电机应该转动（低速） |
| 2. 左电机正转 | 左电机50% PWM正转 | 2秒 | 左轮正转，编码器计数增加 |
| 3. 左电机反转 | 左电机50% PWM反转 | 2秒 | 左轮反转，编码器计数减少 |
| 4. 暂停 | 停止1秒 | 1秒 | - |
| 5. 右电机正转 | 右电机50% PWM正转 | 2秒 | 右轮正转，编码器计数增加 |
| 6. 右电机反转 | 右电机50% PWM反转 | 2秒 | 右轮反转，编码器计数减少 |
| 7. 暂停 | 停止1秒 | 1秒 | - |
| 8. 双电机正转 | 两电机50% PWM正转 | 2秒 | 小车前进，两编码器计数增加 |
| 9. 双电机反转 | 两电机50% PWM反转 | 2秒 | 小车后退，两编码器计数减少 |
| 10. 编码器监控 | 停止电机，持续监控 | 无限 | 手动转动轮子，观察编码器变化 |

### 安全特性
- **低PWM占空比**: 使用30%/50%的PWM避免电机过速
- **分段测试**: 先单电机后双电机，逐步验证
- **编码器实时监控**: 每个测试前后打印编码器值
- **手动监控模式**: 最后进入无限循环，每500ms打印编码器值

---

## 📂 新增文件

| 文件 | 功能 | 代码行数 |
|------|------|----------|
| `Core/Inc/app/motor_debug.h` | 电机调试接口 | 24 |
| `Core/Src/app/motor_debug.c` | 电机调试实现（状态机） | ~320 |

**总计**：2个新文件，约340行代码

---

## 🔄 修改文件

| 文件 | 修改内容 |
|------|----------|
| `Core/Src/freertos.c` | 切换到电机调试模式（注释IMU调试） |
| `CMakeLists.txt` | 添加 `motor_debug.c` 源文件 |

---

## 🛠️ 硬件要求

### 电机驱动（TB6612）
```
TB6612 引脚连接:
  PWMA (Left)  - TIM1_CH1 (PA8)   - 左电机PWM
  AIN1 (Left)  - GPIO             - 左电机方向1
  AIN2 (Left)  - GPIO             - 左电机方向2
  PWMB (Right) - TIM1_CH2 (PA9)   - 右电机PWM
  BIN1 (Right) - GPIO             - 右电机方向1
  BIN2 (Right) - GPIO             - 右电机方向2
  STBY         - 拉高（使能）
```

### 编码器（硬件编码器模式）
```
编码器接口:
  Left Encoder  - TIM3 (Encoder Mode)
    A相 - TIM3_CH1 (PA6 or PB4)
    B相 - TIM3_CH2 (PA7 or PB5)
  
  Right Encoder - TIM4 (Encoder Mode)
    A相 - TIM4_CH1 (PB6 or PD12)
    B相 - TIM4_CH2 (PB7 or PD13)
```

### UART调试串口（UART5）
```
UART5 (Debug Console):
  TX  - PC12 (连接USB转串口的RX)
  RX  - PD2  (可选，不用于调试输出)
  波特率: 115200
  数据位: 8
  停止位: 1
  校验位: None
```

---

## 📺 预期输出示例

### 初始化阶段
```
========================================
    Motor & Encoder Debug Tool v1.0
========================================

[1/4] Initializing platform timer...
[2/4] Initializing encoders...
[3/4] Initializing motors (TB6612)...
[4/4] Ensuring motors are stopped...

========================================
  Initialization Complete!
========================================

Test Sequence:
  1. PWM output test (30%)
  2. Left motor forward/backward
  3. Right motor forward/backward
  4. Both motors forward/backward
  5. Encoder verification

Each test runs for 2 seconds.
Press RESET to stop at any time.

[ENCODER] Initial - Left: 0  Right: 0

Starting in 2 seconds...
```

### 测试运行阶段
```
========================================
[MOTOR] Left Motor FORWARD
  Left PWM:  50%
  Right PWM: 0%
========================================
[ENCODER] Before - Left: 0  Right: 0

... (2秒后) ...

[ENCODER] After - Left: 1245  Right: 0
  --> Left encoder change: 1245 counts

========================================
[MOTOR] Left Motor BACKWARD
  Left PWM:  -50%
  Right PWM: 0%
========================================
[ENCODER] Before - Left: 1245  Right: 0

... (2秒后) ...

[ENCODER] After - Left: 12  Right: 0
  --> Left encoder change: -1233 counts
```

### 手动监控阶段
```
========================================
  Final Encoder Check
========================================
[ENCODER] Final - Left: 2456  Right: -1234

Manually rotate each wheel and observe:
  - Left wheel forward  -> Left count increases
  - Left wheel backward -> Left count decreases
  - Right wheel forward -> Right count increases
  - Right wheel backward -> Right count decreases

[ENCODER] Current - Left: 2456  Right: -1234
[ENCODER] Current - Left: 2460  Right: -1234  (左轮被手动转动)
[ENCODER] Current - Left: 2460  Right: -1230  (右轮被手动转动)
...
```

---

## ✅ 验证清单

### 阶段1：基础PWM输出
- [ ] 左右电机都能转动（30% PWM）
- [ ] 电机转速较低（可安全观察）
- [ ] 编码器有变化（说明电机在转）

### 阶段2：左电机测试
- [ ] 左电机正转：左轮正向旋转，编码器计数**增加**
- [ ] 左电机反转：左轮反向旋转，编码器计数**减少**
- [ ] 右电机始终静止

### 阶段3：右电机测试
- [ ] 右电机正转：右轮正向旋转，编码器计数**增加**
- [ ] 右电机反转：右轮反向旋转，编码器计数**减少**
- [ ] 左电机始终静止

### 阶段4：双电机测试
- [ ] 双电机正转：小车直线前进，两编码器计数都增加
- [ ] 双电机反转：小车直线后退，两编码器计数都减少
- [ ] 左右电机转速大致相同（编码器变化量相近）

### 阶段5：手动验证
- [ ] 手动正转左轮：左编码器计数增加
- [ ] 手动反转左轮：左编码器计数减少
- [ ] 手动正转右轮：右编码器计数增加
- [ ] 手动反转右轮：右编码器计数减少

---

## 🔧 常见问题排查

### 问题1: 电机不转
**可能原因**：
1. TB6612 STBY引脚未拉高（电机驱动未使能）
2. 电源供电不足（电机需要独立电源，不能用STM32的3.3V）
3. 电机线未连接或接触不良
4. PWM信号未输出（TIM1未启动）

**排查步骤**：
1. 用万用表测量TB6612的STBY引脚（应该为高电平）
2. 测量电机供电电压（应该在6-12V）
3. 用示波器测量PWMA/PWMB引脚（应该有20kHz PWM波形）
4. 检查AIN1/AIN2/BIN1/BIN2的电平（应该随电机方向变化）

### 问题2: 电机转但编码器不变
**可能原因**：
1. 编码器线未连接或接触不良
2. 编码器供电不足（检查3.3V或5V供电）
3. TIM3/TIM4未配置为编码器模式
4. 编码器A/B相接反

**排查步骤**：
1. 用示波器测量编码器A/B相输出（手动转动轮子应该有方波）
2. 检查TIM3/TIM4的CNT寄存器（通过调试器查看）
3. 交换A/B相引脚连接，看是否有变化

### 问题3: 编码器方向错误
**现象**：电机正转时编码器计数减少

**原因**：编码器A/B相接反，或电机接线反

**解决方案**：
- 方案1：交换编码器A/B相引脚
- 方案2：在代码中反转编码器方向（修改`config.c`中的`encoder_directions`数组）
- 方案3：交换电机线（不推荐，会导致前进后退混乱）

### 问题4: 左右轮转速差异大
**现象**：双电机测试时，左右编码器变化量差异>20%

**可能原因**：
1. 电机老化程度不同
2. 轮子摩擦力不同
3. 电机供电电压不均（检查TB6612的VCC和GND连接）

**解决方案**：
- 这是正常现象，后续通过PID控制补偿
- 记录左右轮速度比例，用于motion_kinematics参数标定

### 问题5: 串口无输出
**排查步骤**：
1. 检查UART5是否初始化（CubeMX中使能）
2. 检查printf重定向（`uart_debug.c`中的`fputc`函数）
3. 检查串口工具配置（115200, 8N1）
4. 检查TX引脚（PC12）是否正确连接到USB转串口的RX

---

## 📊 编码器计数参考值

基于典型参数估算（实际值需要根据你的硬件测量）：

**假设**：
- 电机转速：100 RPM @ 50% PWM（约1.67转/秒）
- 编码器PPR（每转脉冲数）：334
- 测试时间：2秒

**预期编码器变化量**：
```
编码器计数 = 转速 × PPR × 时间
           = 1.67 转/秒 × 334 脉冲/转 × 2 秒
           ≈ 1100 counts
```

**实际测试时的合理范围**：800 - 1400 counts（±30%）

如果实际值偏差很大：
- 远小于预期（<500）：电机供电不足，或PWM占空比太低
- 远大于预期（>2000）：电机转速过快，或编码器PPR配置错误

---

## 🚀 使用步骤

### 1. 编译和烧录
```bash
# 在项目根目录下
mkdir -p build
cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug
cmake --build .

# 烧录（使用你的烧录工具，例如OpenOCD或ST-Link Utility）
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg -c "program v1.0_freeRTOS.elf verify reset exit"
```

### 2. 连接硬件
1. 将STM32板通过ST-Link连接到电脑（用于烧录）
2. 将UART5的TX引脚（PC12）连接到USB转串口的RX
3. 确保电机驱动（TB6612）的STBY引脚拉高
4. 连接电机电源（6-12V，独立供电）
5. 将小车架起来（轮子悬空，避免小车跑走）

### 3. 打开串口监控
- 波特率：115200
- 数据位：8
- 停止位：1
- 校验位：None

推荐工具：
- Windows: Tera Term, PuTTY
- macOS/Linux: minicom, screen
- 跨平台: CoolTerm, RealTerm

### 4. 复位开发板
按下RESET按钮，观察串口输出。

### 5. 观察测试结果
- 自动测试序列会依次运行（每个2秒）
- 观察电机是否按预期旋转
- 记录编码器变化量
- 最后进入手动监控模式

### 6. 记录测试数据
建议记录以下数据（用于后续参数标定）：

| 测试项 | 左编码器变化 | 右编码器变化 | 备注 |
|--------|------------|------------|------|
| 左电机正转 | +____ | 0 | |
| 左电机反转 | -____ | 0 | |
| 右电机正转 | 0 | +____ | |
| 右电机反转 | 0 | -____ | |
| 双电机正转 | +____ | +____ | 差异: ____% |
| 双电机反转 | -____ | -____ | 差异: ____% |

---

## 📝 后续工作建议

### 1. 参数标定（根据测试结果）

#### 编码器PPR验证
```c
// 在config.h中
#define ENCODER_PPR 334  // 需要根据实际测试验证
```

验证方法：
1. 手动精确转动轮子1圈
2. 记录编码器变化量
3. 如果与334不符，更新此值

#### 编码器方向
```c
// 在config.c中
static const int8_t encoder_directions[SD_ENCODER_COUNT] = {1, -1};
```

如果测试发现方向反了，修改为 `{-1, 1}` 或 `{-1, -1}`。

#### 轮距和轮径
```c
// 在motion_config.h中
#define WHEEL_BASE   0.150f  // 米（用直尺测量左右轮中心线间距）
#define WHEEL_RADIUS 0.033f  // 米（轮子在已知长度地面滚动测量）
```

### 2. 速度PID控制调参

当基础电机功能验证通过后，下一步是调整速度PID参数：

```c
// 在motion_config.h中
#define SPEED_KP 0.5f   // 比例系数
#define SPEED_KI 0.1f   // 积分系数
#define SPEED_KD 0.0f   // 微分系数（通常为0）
```

调参步骤：
1. 先用小的KP（0.1），观察响应
2. 逐步增加KP直到出现震荡
3. 减小KP到震荡消失的80%
4. 添加小的KI（0.01-0.1）消除稳态误差

### 3. 集成到主控制循环

当电机调试验证通过后：
1. 在 `freertos.c` 中恢复正常控制模式
2. 移除电机调试代码
3. 恢复 `ControlApp_Init()` 和 `ControlApp_RunFastCycle()` 调用
4. 电机将通过 `motor_adapter.c` 被Motion Control模块控制

### 4. 完整系统测试

最终验证迁移指南中的5个阶段：
- [x] 阶段1：电机+编码器基础回路测试 ← **当前阶段**
- [ ] 阶段2：Motion Control闭环测试
- [ ] 阶段3：IMU验证
- [ ] 阶段4：IR循迹验证
- [ ] 阶段5：完整控制联调

---

## 🔗 相关文档

- `logs/ti-to-stm32-migration-2026-07-29.md` - TI到STM32迁移日志
- `logs/2026-07-29_imu_debug_complete.md` - IMU调试完成日志
- `docs/MIGRATION_GUIDE.md` - 迁移指南
- `Core/Inc/app/motor.h` - 电机驱动接口
- `Core/Inc/app/encoder.h` - 编码器接口
- `modules/MotionControl/inc/motion_config.h` - 运动控制参数

---

## ⚠️ 安全注意事项

1. **电机测试时将小车架起**：避免小车突然移动掉落或碰撞
2. **使用低PWM占空比**：首次测试使用30-50% PWM，确认正常后再提高
3. **准备急停**：随时准备按下RESET按钮停止电机
4. **独立电源供电**：电机必须使用独立6-12V电源，不要用STM32的3.3V
5. **检查接线**：确认电机和编码器接线正确，避免短路

---

## ✅ 验收标准

本次电机调试工具开发已达到以下标准：

1. ✅ **代码完整性**: 电机调试工具代码已完成
2. ✅ **测试序列**: 10个测试阶段覆盖所有基础功能
3. ✅ **安全设计**: 低PWM + 分段测试 + 实时监控
4. ✅ **文档完整**: 使用步骤、排查指南、参数标定方法已记录
5. ⏳ **硬件验证**: 待在实际硬件上运行验证

**下一步**: 编译、烧录、在实际硬件上运行电机测试

---

**日志撰写时间**: 2026-07-29 23:45  
**开发执行者**: Claude (Opus 4.8)  
**开发耗时**: 约20分钟  
**代码状态**: ✅ 已完成，待编译  
**风险评估**: 低（独立调试工具，不影响主业务逻辑）
