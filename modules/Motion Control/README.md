# 运动控制层模块

## 概述

本模块实现轨迹跟踪执行层，接收上层决策模块（Sens-Decision）的速度指令，通过差速运动学解算 + 前馈/反馈轮速控制，输出PWM到电机。

## 新架构：决策-执行分离

```
┌──────────────────────────────────────────┐
│  Sens-Decision（决策层）                   │
│  传感器 → EKF → 感知 → 行为规划 → 轨迹生成  │
│                                ↓          │
│                    trajectory_point_t      │
│                       {v, omega}           │
└────────────────────┬─────────────────────┘
                     ↓
┌──────────────────────────────────────────┐
│  Motion Control（执行层）                   │
│                                           │
│  速度指令 → 平滑 → 限幅 → 逆运动学          │
│       ↓                                   │
│  轮速控制（前馈+反馈）                       │
│       ↓                                   │
│  PWM → 电机                                │
└──────────────────────────────────────────┘
```

## 文件结构

```
Motion Control/
├── motion_config.h              # 参数配置（调参只需改这个文件）
├── motion_kinematics.h/c        # 差速运动学（不变）
├── motion_feedback.h/c          # PID + 编码器状态估计（不变）
├── motion_feedforward.h/c       # 前馈控制（不变）
├── motion_control.h/c           # 主控制器（重构为轨迹跟踪）
├── example_usage.c              # 使用示例
└── README.md                    # 本文件
```

## 核心特性

### 1. 职责清晰

| 层 | 职责 | 频率 |
|-----|------|------|
| **Sens-Decision** | 传感器数据融合、感知、行为决策、轨迹规划 | 10-50Hz |
| **Motion Control** | 轨迹跟踪（逆运动学+轮速控制） | 500Hz |

Motion Control 不再直接读取循迹传感器或IMU，只接收 `(v, ω)` 速度指令。

### 2. 轮速控制（前馈+反馈）

```
PWM = 前馈（惯性+摩擦+静摩擦）+ PI反馈（速度误差）
```

- **前馈**：基于电机模型的预测补偿，提供70-80%的控制量
- **反馈**：PI闭环修正模型误差和外部扰动

### 3. 指令安全保护

- **一阶低通平滑**：防止指令跳变引起冲击（时间常数可配）
- **加速度限幅**：限制速度变化率，防止打滑
- **角速度限幅**：保护差速机构

### 4. 虚表接口设计

- 解耦硬件依赖，方便测试和移植
- 编码器接口：`getCount()` / `resetCount()`
- 电机接口：`setDifferentialPWM()` / `stop()` / `init()`

## 快速开始

### 1. 包含头文件

```c
#include "motion_control.h"
```

### 2. 实现硬件接口

参考 `example_usage.c` 中的接口实现。

### 3. 初始化和使用

```c
static MotionControl_t g_motionCtrl;

// 初始化（只需编码器和电机接口）
MotionControl_Init(&g_motionCtrl, &hw_encoderInterface, &hw_motorInterface);

// 设置初始速度指令
MotionControl_SetVelocityCommand(&g_motionCtrl, 0.5f, 0.0f);

// 启动控制
MotionControl_Start(&g_motionCtrl);

// ------------------------------------------------------------------
// 高频中断（500Hz）：执行轮速控制
// ------------------------------------------------------------------
void TIMER_IRQHandler(void) {
    MotionControl_Update(&g_motionCtrl);
}

// ------------------------------------------------------------------
// 主循环（10-50Hz）：更新速度指令
// ------------------------------------------------------------------
trajectory_point_t traj = SensDecision_GetTrajectory();
MotionControl_SetVelocityCommand(&g_motionCtrl, traj.v, traj.omega);
```

## 参数调试

所有参数集中在 `motion_config.h` 中，修改后重新编译即可生效。

### 调试顺序

1. **物理参数标定**
   - 轮径、轮距按实际测量填入

2. **前馈参数**（静摩擦 → 摩擦 → 加速度）
   - `FF_K_STATIC`：找到刚好克服静摩擦的最小PWM值
   - `FF_K_FRICTION`：记录不同匀速对应的稳态PWM
   - `FF_K_ACCEL`：测试不同加速度对应的PWM需求

3. **速度PI反馈**
   - 先调 `SPEED_KP` 到快速响应无振荡
   - 再调 `SPEED_KI` 消除稳态误差

4. **指令平滑**
   - `CMD_SMOOTH_TAU`：越大越平滑但响应慢，越小越跟指但冲击大
   - `MAX_ACCELERATION`：根据赛道抓地力设定

### 关键参数

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `WHEEL_BASE` | 0.150 | 轮距 (m) |
| `WHEEL_RADIUS` | 0.033 | 轮半径 (m) |
| `ENCODER_PPR` | 334 | 编码器每转脉冲数 |
| `CONTROL_FREQ_HZ` | 500 | 控制频率 (Hz) |
| `SPEED_KP` | 200.0 | 轮速比例增益 |
| `SPEED_KI` | 50.0 | 轮速积分增益 |
| `FF_K_ACCEL` | 50.0 | 加速度前馈系数 |
| `FF_K_FRICTION` | 300.0 | 摩擦前馈系数 |
| `FF_K_STATIC` | 80.0 | 静摩擦补偿 |
| `MAX_SPEED` | 1.00 | 最大速度 (m/s) |
| `MAX_ACCELERATION` | 2.0 | 最大加速度 (m/s²) |
| `MAX_OMEGA` | 6.0 | 最大角速度 (rad/s) |
| `CMD_SMOOTH_TAU` | 0.05 | 指令平滑时间常数 (s) |

## 性能指标

| 指标 | 目标值 |
|------|--------|
| 控制频率 | 500Hz |
| CPU占用率 | <2% (MSPM0G3507 @ 32MHz) |
| 代码量 | ~500行（精简后） |
| Flash占用 | ~5-6KB |
| SRAM占用 | ~2KB |

## 状态机

```
INIT → IDLE → RUNNING → EMERGENCY
```

- `INIT`: 初始化状态（仅内部使用）
- `IDLE`: 空闲，电机停止，等待指令
- `RUNNING`: 正常运行，跟踪速度指令
- `EMERGENCY`: 紧急停止，需手动恢复

## 常见问题

### Q1: 高速时车轮打滑
**A**: 增加 `CMD_SMOOTH_TAU` 降低指令变化率，或降低 `MAX_ACCELERATION`

### Q2: 速度跟踪有稳态误差
**A**: 增大 `SPEED_KI`

### Q3: 低速时抖动
**A**: 调整 `FF_K_STATIC` 确保能克服静摩擦，适当降低 `SPEED_KP`

### Q4: 响应慢
**A**: 减小 `CMD_SMOOTH_TAU`，增大 `SPEED_KP`，检查前馈参数

### Q5: 指令跳变引起冲击
**A**: 增大 `CMD_SMOOTH_TAU`，确保加速度限幅生效

## 从旧版（三环控制）迁移

### 删除的功能

| 功能 | 原因 |
|------|------|
| 横向位置PID（外环） | 由 Sens-Decision 感知层替代 |
| 角速度PI（中环） | 由 Sens-Decision 轨迹生成替代 |
| 循迹传感器接口 | 不再直接读取 |
| IMU接口 | 不再直接读取 |
| 丢线处理 | 由 Sens-Decision 行为规划处理 |
| LineSensorInterface_t | 删除 |
| ImuInterface_t | 删除 |
| PerceptionInterface_t | 删除（简化为 EncoderInterface_t + MotorInterface_t） |
| ActuatorInterface_t | 删除（电机接口直接内嵌） |

### 新增的功能

| 功能 | 说明 |
|------|------|
| VelocityCommand_t | 速度指令结构 (v, ω) |
| 指令平滑 | 一阶低通滤波 |
| 加速度限幅 | 保护不打滑 |
| 角速度限幅 | 保护差速机构 |
| MotionControl_SetVelocityCommand() | 接收上层指令 |
| MotionControl_GetTargetWheelSpeed() | 查询目标轮速 |

### 复用的模块

| 模块 | 状态 |
|------|------|
| motion_feedback.h/c (PID + 状态估计器) | 完全复用 |
| motion_feedforward.h/c (前馈控制) | 完全复用 |
| motion_kinematics.h/c (差速运动学) | 完全复用 |

## 参考文档

- [运动控制层设计文档](../../docs/superpowers/specs/2026-07-14-motion-control-design.md)

## 作者与维护

- **开发日期**: 2026-07-14
- **重构日期**: 2026-07-17
- **芯片平台**: TI MSPM0G3507
- **应用场景**: 电赛循迹小车（与 Sens-Decision 配合使用）
