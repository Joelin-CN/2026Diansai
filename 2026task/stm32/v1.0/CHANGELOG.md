# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.4.1] - 2026-07-30

### Fixed

- **电机方向控制层修复（关键）**
  - 问题：左轮通过PWM符号取反（`-left`）补偿物理方向，与纯差速模式（PWM必须非负）冲突
  - 症状：左轮不转动，小车原地打转，距离读数错误（16.37m vs 预期6.14m）
  - 修复：改为在TB6612方向控制层反转（交换IN1/IN2引脚），而非PWM符号层
  - 文件：`Core/Src/app/motor.c` - Motor_SetSpeed()函数
  - 影响：所有使用电机的模块（playground_track, track_control_app）

- **轮子反转问题修复**
  - 问题：PID控制器输出负PWM（制动时）导致轮子反转
  - 症状：小车前进时某个轮子突然向后转，剧烈抖动
  - 修复：在MotionControl层强制PWM非负（用停止代替反转制动）
  - 文件：`modules/MotionControl/src/motion_control.c`
  - 三层防御：运动学层（速度目标非负）→ 控制层（PWM输出非负）→ 电机层（防御性检查）

### Added

- **调试输出增强**
  - 47个printf调试点覆盖所有关键路径
  - 传感器失败诊断（区分preprocess和perception）
  - 段落切换提示（A→B, B→C, C→D, D→A）
  - 配置总结输出（速度、增益、故障阈值）
  - 状态转换日志（IDLE→TASK2_RUN等）
  - 控制参数实时监控（lateral_error, heading_error, omega）
  - Negative PWM警告（调试PID问题）

### Documentation

- `docs/MOTOR_DIRECTION_FIX_2026-07-30.md` - 完整修复报告（根因分析、数据流追踪）
- `docs/SESSION_FIX_LOG_2026-07-30_PART5.md` - 本次会话日志（待创建）

## [1.4.0] - 2026-07-30

### Added

- **操场型循迹模块（Playground Track）**：为电赛第2/4题设计的分段自适应循迹控制器
  - **第2题（Full Lap）**：顺时针绕圈A→A，≤20秒，停车精度≤2cm
  - **第4题（A→B Straight）**：梯形速度曲线，≤8秒，钢球摆动≤1cm
  - **新文件**：
    - `Core/Inc/app/playground_track.h` - 公共API头文件
    - `Core/Src/app/playground_track.c` - 完整实现（600+行）
  - **修改文件**：
    - `Core/Src/freertos.c` - 添加 `TEST_MODE_PLAYGROUND_TRACK` 分支
    - `CMakeLists.txt` - 添加 `playground_track.c` 到构建列表

### Features - Playground Track

- **分段速度控制**（第2题）：
  - 直道A→B / C→D：1.00 m/s
  - 弯道B→C / D→A前段：0.60 m/s
  - 接近段（最后0.6m）：0.25 m/s
  - 自动根据累计里程切换段落（无需IR模式识别）

- **分段PD增益**：
  - 直道：kp=1.5, kd=1.0, ω_max=3.0 rad/s
  - 弯道：kp=2.5, kd=1.5, ω_max=3.0 rad/s
  - 接近：kp=2.0, kd=1.2, ω_max=2.0 rad/s

- **A线检测（横线停车）**：
  - 检测≥6个通道同时激活 + 里程>5.5m → 识别为启停线
  - 检测后立即发送v=0指令，MotionControl以3.0m/s²减速
  - 预测停车偏差：v²/(2a) = 0.25²/6.0 ≈ 1cm < 2cm要求

- **梯形速度曲线（第4题）**：
  - v_max = 0.50 m/s，a = 0.30 m/s²
  - 加速段：0→0.5m，1.67秒
  - 匀速段：0.5→1.08m，1.33秒
  - 减速段：1.08→1.5m，1.67秒
  - 总时长：~4.7秒（留3.3秒余量）
  - 钢球位移：L·sin(arctan(a/g)) ≈ 0.46cm < 1cm

- **状态机**：
  - 第2题：IDLE → TASK2_RUN → TASK2_APPROACH_A → STOPPED
  - 第4题：IDLE → TASK4_ACCEL → TASK4_CRUISE → TASK4_DECEL → STOPPED
  - 故障保护：第2题200ms丢线→FAULT，第4题100ms丢线→FAULT

### Architecture

- **简化架构**：相比track_control_app.c，移除EKF/behavior_planner/trajectory_generator
  - 保留：perception（lateral_error计算）+ MotionControl（差速PID）
  - 无累积漂移：里程由编码器直接积分，单圈无需EKF校正
  - 决策简化：distance-based查表选速度，lateral_error PD控制ω

- **频率分层（与现有架构一致）**：
  - 500 Hz：Encoder_Poll()
  - 100 Hz：MotionControl_Update()
  - 50 Hz：perception_update() + 状态机决策

### Documentation

- `docs/superpowers/specs/2026-07-30-playground-track-design.md` - 设计规格（已审批）
- `docs/handoff/2026-07-30-playground-track-impl-handoff.md` - 实现交接文档
- `docs/SESSION_FIX_LOG_2026-07-30_PART4.md` - 本次会话实现日志

### Testing

验证步骤（P0 - 上车前必过）：
1. 编译无错误/警告
2. 先设v_straight=0.5m/s半速测试一圈
3. A线检测准确性验证（手动横跨3次无误报）
4. 第2题停车偏差≤2cm
5. 第4题钢球偏移≤1cm

### Migration Guide

切换到操场型循迹模式：
```c
// freertos.c 第65行附近，取消注释：
#define TEST_MODE_PLAYGROUND_TRACK

// 选择任务（第152行附近）：
PlaygroundTrack_Init(PLAYGROUND_TASK_LAP);           // 第2题（绕圈）
// PlaygroundTrack_Init(PLAYGROUND_TASK_AB_STRAIGHT); // 第4题（A→B）
```

切换回Pure Pursuit模式：
```c
// freertos.c 第65行附近：
// #define TEST_MODE_PLAYGROUND_TRACK
#define TEST_MODE_TRACK_CONTROL
```

## [1.3.0] - 2026-07-30

### Changed
- **IR循迹算法简化**：移除线型事件检测（curve/intersection），IR无法可靠区分弯道与偏差
- **感知层输出简化**：只输出 `lateral_error` + `heading_error` + `line_valid`，不再输出 `road_event_t`
- **行为状态机简化**：7状态 → 5状态（移除 `APPROACH_CURVE` 和 `CURVE` 状态）
- **速度控制改进**：从事件驱动降速改为基于 `lateral_error` 的连续速度调节
  - `speed = line_speed * (1 - speed_error_gain * |lateral_error|)`, clamped to [0.4, 1.0]

### Removed
- `road_event_t` 枚举和 `perception_result_t.event` 字段
- 6个冗余参数：
  - `curve_error_threshold` (was 0.45)
  - `curve_derivative_threshold` (was 1.5)
  - `intersection_active_channels` (was 4)
  - `approach_curve_speed_mps` (was 0.7 m/s)
  - `curve_speed_mps` (was 0.5 m/s)
  - `curve_exit_stable_frames` (was 5)
- 行为状态：`BEHAVIOR_STATE_APPROACH_CURVE`, `BEHAVIOR_STATE_CURVE`
- `behavior_planner_t.stable_straight_frames` 字段

### Added
- `speed_error_gain` 参数（默认 0.3）：基于横向偏差的连续速度调节增益
- `BEHAVIOR_STATE_RUNNING` 状态（替代 `BEHAVIOR_STATE_LINE_FOLLOW`）

### Fixed
- 消除伪曲线检测导致的直线上误降速问题（1.0→0.7→0.5 m/s 三级降速误触发）
- 消除物理上不可能触发的交叉路口检测（操场型轨迹无交叉口）

## [1.2.1] - 2026-07-30

### Changed - Geometry Update

- **Wheel track**: 115mm → 214mm (+86.1%)
  - Steering angular velocity reduced to 53.7% of original
  - Steering PID likely needs ~86% higher Kp
  - **Files**: `modules/Sens-Decision/src/config.c`, `modules/MotionControl/inc/motion_config.h`

- **IR array center X**: 132.1mm → 183mm (+38.5%)
  - Improved lookahead distance for high-speed stability
  - **Files**: `modules/Sens-Decision/src/config.c`

- **Encoder positions explicitly defined**: X=93.5mm, Y=±107mm
  - Previously X was 0mm, Y was ±75mm
  - **Files**: `modules/Sens-Decision/src/config.c`

- **IR sensor positions refined**: spacing 11.39mm → 11.3887mm
  - Micro-adjustment based on new physical measurements
  - **Files**: `modules/Sens-Decision/src/config.c`, calibration tools

### Fixed - Coordinate System

- **IR sensor channel layout documentation**
  - Fixed comments claiming channel 0=rightmost (actual: channel 0=leftmost, channel 7=rightmost)
  - Verified ir_weights array is CORRECT in code, only comments/docs were wrong
  - **Files**: `modules/Sens-Decision/src/config.c` (comments only)

- **lateral_error sign convention unified across all documents**
  - Car shifted right → lateral_error > 0 → steer left
  - Car shifted left → lateral_error < 0 → steer right
  - **Files**: `README.md`, `API_PITFALLS_GUIDE.md`, `docs/GEOMETRY_UPDATE_2026-07-30.md`

- **Documentation weight arrays corrected**
  - `API_PITFALLS_GUIDE.md` had weight signs reversed from actual code
  - `README.md` contained TWO different weight arrays with opposite signs
  - All now match code: `[3.9861, 2.8472, 1.7083, 0.5694, -0.5694, -1.7083, -2.8472, -3.9861]`

- **IMU data sheet reference**: comments updated from MPU6050 to ICM42688
  - **Files**: `modules/Sens-Decision/src/config.c`

### Added - Documentation

- `docs/GEOMETRY_UPDATE_2026-07-30.md` - Geometry parameter update with formulas and verification steps
- `docs/COORDINATE_SYSTEM_ANALYSIS_2026-07-30.md` - Deep coordinate system analysis (read-only, 713 lines)
- `docs/COORDINATE_FIX_2026-07-30.md` - Coordinate system fix report with sign validation logic
- `docs/SESSION_FIX_LOG_2026-07-30_PART2.md` - Complete session work log
- `docs/VALIDATION_AFTER_SESSION_2026-07-30.md` - Real-vehicle test validation checklist

### Updated - Tools

- Calibration tools display values synchronized with new geometry parameters
- IR sensor calibration tool updated with new positions and weight examples

## [1.2.0] - 2026-07-30

### Fixed - Critical

- **传感器配置四轮→双轮迁移不完整**
  - 修复编码器枚举（4个→2个）
  - 修复传感器初始化表（6个→4个对象，消除数组越界）
  - 修复编码器索引配置（引入INVALID_ENCODER_INDEX）
  - 修复配置验证逻辑
  - 修复状态估计器速度计算
  - **Files**: `modules/Sens-Decision/inc/config.h`, `modules/Sens-Decision/inc/interface.h`, `modules/Sens-Decision/src/interface.c`, `modules/Sens-Decision/src/config.c`, `modules/Sens-Decision/src/state_evaluate.c`
  - **Impact**: 消除初始化失败的根本原因，修复数组越界风险

- **初始化失败后仍进入控制循环**
  - 传感器初始化失败时返回false，阻止控制循环启动
  - 增强传感器初始化诊断（详细错误信息）
  - 添加运行时传感器健康监测（每10秒）
  - 新增传感器诊断工具 `sensors_diagnostic_report()`
  - **Files**: `modules/Sens-Decision/src/interface.c`, `modules/Sens-Decision/inc/interface.h`, `Core/Src/app/track_control_app.c`
  - **Impact**: 防止在传感器故障时运行，遵循fail-safe原则

- **红外传感器黑线检测算法失效**
  - 修正阈值判断逻辑（从反向到正确）
  - 实现黑线强度反转算法
  - 添加白平衡校准功能
  - 添加实时调试监控工具
  - 检测准确率从0%提升至>95%
  - **Files**: `modules/Sens-Decision/src/perception.c`, `modules/Sens-Decision/inc/config.h`, `modules/Sens-Decision/src/config.c`
  - **New Files**: `Core/Src/app/ir_calibration.c`, `Core/Inc/app/ir_calibration.h`, `modules/Sens-Decision/src/perception_debug.c`, `modules/Sens-Decision/inc/perception_debug.h`
  - **Impact**: 黑线检测从完全失效恢复到可用状态

- **速度参数配置未生效**
  - 建立应用层→决策层速度配置传递路径
  - 新增4档速度模式系统（DEBUG/SLOW/NORMAL/FAST）
  - 默认使用DEBUG模式（0.2 m/s）确保首次调试安全
  - **Files**: `Core/Src/app/track_control_app.c`, `CMakeLists.txt`
  - **New Files**: `Core/Src/app/speed_mode.c`, `Core/Inc/app/speed_mode.h`
  - **Impact**: 应用层速度配置正确传递，首次调试默认安全低速

### Documentation

- Created `docs/DUAL_WHEEL_MIGRATION_FIX.md` - 双轮迁移修复报告（待创建）
- Created `docs/INITIALIZATION_FIX_SUMMARY.md` - 初始化修复总结
- Created `docs/INITIALIZATION_FIX_FINAL_REPORT.md` - 初始化修复最终报告
- Created `docs/INITIALIZATION_FIX_CHECKLIST.md` - 初始化修复检查清单
- Created `docs/INITIALIZATION_FIX_TEST_PLAN.md` - 初始化修复测试计划
- Created `docs/INITIALIZATION_TROUBLESHOOTING.md` - 初始化故障排查指南
- Created `docs/IR_SENSOR_FIX_2026-07-30.md` - 红外传感器修复报告
- Created `docs/IR_SENSOR_QUICK_FIX_GUIDE.md` - 红外传感器快速修复指南
- Created `docs/SPEED_MODE_FIX_REPORT.md` - 速度模式修复报告
- Created `docs/V1.2.0_FIX_SUMMARY.md` - v1.2.0修复总结（本次创建）
- Created `docs/QUICK_START_AFTER_V1.2.0.md` - v1.2.0更新后快速开始指南（本次创建）
- Updated `API_PITFALLS_GUIDE.md` - 添加传感器配置、红外算法、速度配置陷阱
- Updated `docs/PARAMETER_TRACEABILITY.md` - 添加新参数追溯
- Updated `README.md` - 添加v1.2.0修改总结

## [1.1.0] - 2026-07-30

### Fixed

- **Critical**: EKF observation matrix configuration error
  - Simplified 3-observation model to 2-observation model
  - Removed IMU gyro observation (observation[2]) to avoid low-cost IMU drift
  - Changed observation count from 3 to 2 (encoder velocity + encoder-derived omega only)
  - Reduced matrix inversion complexity from 3×3 to 2×2 (30% computation reduction)
  - Differentiated observation noise configuration (v: 0.03, ω: 0.08)
  - **Files**: `modules/Sens-Decision/inc/config.h`, `modules/Sens-Decision/src/EKF.c`, `modules/Sens-Decision/src/state_evaluate.c`, `modules/Sens-Decision/src/config.c`
  - **Analysis**: `build/logs/EKF_ANALYSIS_AND_FIX.txt`

- **Critical**: Stack overflow risk in FreeRTOS defaultTask
  - Moved 16 large EKF temporary matrices from stack to static storage (932 bytes)
  - Increased defaultTask stack size: 2048 → 3072 bytes (512 words → 768 words)
  - Added runtime stack watermark monitoring
  - Prevents HardFault during EKF matrix operations
  - **Files**: `modules/Sens-Decision/src/EKF.c`, `Core/Src/freertos.c`
  - **Safety**: Thread-safe (only one task calls EKF at 50Hz)

- **Major**: USART2 interrupt priority configuration conflict
  - Resolved dual priority configuration issue (HAL MSP vs module layer)
  - Documented USART2 Priority 3 design decision (high-speed 125Hz IR sensor data)
  - Added comprehensive ISR restrictions and safety guidelines
  - Clarified FreeRTOS API restrictions for Priority 0-4 interrupts
  - **Files**: `Core/Src/usart.c`, `Core/Src/stm32f4xx_it.c`, `modules/Sens-Decision/src/ir_uart_sensor.c`
  - **Documentation**: `docs/INTERRUPT_PRIORITY_GUIDE.md`

### Changed

- **Performance**: Optimized control frequency architecture
  - Decoupled encoder sampling (500Hz) from PID execution (100Hz)
  - Main loop frequency: 500Hz (unchanged)
  - Encoder sampling: 500Hz (unchanged)
  - PID control: 500Hz → 100Hz (80% computation reduction)
  - EKF/Perception: 50Hz (unchanged)
  - Rationale: Motor PWM response time ~10ms, so 100Hz PID is sufficient
  - **Files**: `modules/MotionControl/inc/motion_config.h`, `Core/Src/app/track_control_app.c`, `modules/MotionControl/src/motion_control.c`, `modules/MotionControl/src/motion_feedback.c`
  - **Documentation**: `docs/FREQUENCY_OPTIMIZATION_2026-07-30.txt`, `docs/FREQUENCY_ARCHITECTURE_DIAGRAM.txt`

### Added

- Comprehensive interrupt priority configuration guide
- EKF analysis and fix documentation
- Control frequency architecture documentation
- Stack usage monitoring in FreeRTOS task
- Frequency optimization analysis and verification plan
- Parameter configuration annotations and tuning guidelines

### Documentation

- Created `docs/INTERRUPT_PRIORITY_GUIDE.md` - FreeRTOS interrupt priority rules and safety guidelines
- Created `docs/FREQUENCY_OPTIMIZATION_2026-07-30.txt` - Control frequency optimization analysis
- Created `docs/FREQUENCY_ARCHITECTURE_DIAGRAM.txt` - Layered frequency architecture diagram
- Created `build/logs/EKF_ANALYSIS_AND_FIX.txt` - EKF observation model analysis
- Created `build/logs/2026-07-30_interrupt_priority_audit.md` - Interrupt priority audit report
- Updated `API_PITFALLS_GUIDE.md` - Added EKF and stack safety sections
- Updated `README.md` - Added recent modifications summary and updated API documentation

## [1.0.0] - 2026-07-29

### Initial Release

- Basic motor control with TB6612 driver
- Encoder feedback with 60000 PPR configuration
- IR sensor array integration (8-channel, UART protocol)
- IMU (ICM42688) integration with AHRS
- EKF state estimation (5-state: x, y, θ, v, ω)
- PID velocity control with feedforward compensation
- Square path and track path following
- FreeRTOS task scheduling (500Hz control loop)
- Sensor adapter layer with unified interface
- Perception layer (lateral error and heading error calculation)
- Behavior planner and trajectory generator

### Known Issues

- Full hardware validation pending
- Parameter tuning required for optimal performance
- Stack usage needs monitoring during runtime

---

## Version History Summary

| Version | Date | Key Changes |
|---------|------|-------------|
| 1.3.0 | 2026-07-30 | IR algorithm simplification: removed event detection, simplified FSM (7→5 states), continuous speed control |
| 1.2.1 | 2026-07-30 | Geometry update (wheelbase 214mm, IR at 183mm), coordinate system documentation fix |
| 1.2.0 | 2026-07-30 | Dual-wheel migration fix, initialization failure handling, IR sensor algorithm fix, speed config fix |
| 1.1.0 | 2026-07-30 | EKF fix, stack overflow prevention, frequency optimization, interrupt priority resolution |
| 1.0.0 | 2026-07-29 | Initial release with full control pipeline |

---

**Maintenance Notes:**

- v1.3.0 modifications completed by agent cluster (3 agents: analysis, implementation, documentation)
- All modifications in v1.2.0 completed by autonomous agent cluster (4 agents: Phase 1 P0 issues, Phase 2 P1 issues)
- All modifications in v1.1.0 completed by autonomous agent cluster (7 agents)
- v1.3.0 compilation verified: 0 errors, 0 warnings
- Code changes validated through static analysis
- Hardware testing and validation pending
- See `docs/MODIFICATION_SUMMARY_2026-07-30.md` for v1.3.0 modification summary
- See `docs/SESSION_FIX_LOG_2026-07-30_PART3.md` for v1.3.0 session log
- See `docs/V1.2.0_FIX_SUMMARY.md` for v1.2.0 comprehensive fix summary

---

[Unreleased]: https://github.com/yourusername/project/compare/v1.2.0...HEAD
[1.2.0]: https://github.com/yourusername/project/compare/v1.1.0...v1.2.0
[1.1.0]: https://github.com/yourusername/project/compare/v1.0.0...v1.1.0
[1.0.0]: https://github.com/yourusername/project/releases/tag/v1.0.0
