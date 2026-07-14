# 运动控制层模块

## 概述

本模块实现了基于三环串级控制的循迹小车运动控制系统，支持：
- **外环**：横向位置PID控制（基于12路循迹传感器）
- **中环**：角速度PI控制（基于ICM42688陀螺仪）
- **内环**：轮速控制（前馈+反馈，基于编码器）

## 文件结构

```
Motion Control/
├── motion_config.h              # 参数配置
├── motion_kinematics.h/c        # 差速运动学
├── motion_feedback.h/c          # 反馈控制+状态估计
├── motion_feedforward.h/c       # 前馈控制
├── motion_control.h/c           # 主控制器
├── example_usage.c              # 使用示例
└── README.md                    # 本文件
```

## 核心特性

### 1. 三环控制架构
```
循迹传感器 → 横向位置环(100Hz) → 角速度环(500Hz) → 轮速环(500Hz) → 电机
                    ↑                    ↑                  ↑
                    └─────────────────────┴──────────────────┘
                           状态估计器（编码器+IMU）
```

### 2. 前馈+反馈控制
- **前馈**：基于电机模型补偿惯性、摩擦（准电流环效果70-80%）
- **反馈**：PID闭环修正模型误差和外部扰动

### 3. 虚表接口设计
- 解耦硬件依赖，方便测试和移植
- 支持运行时切换接口实现

## 快速开始

### 1. 包含头文件

```c
#include "motion_control.h"
```

### 2. 实现硬件接口

参考 `example_usage.c` 中的接口实现：
- 循迹传感器接口
- 编码器接口
- IMU接口
- 电机接口

### 3. 初始化和使用

```c
// 创建控制器实例
static MotionControl_t g_motionCtrl;

// 初始化
MotionControl_Init(&g_motionCtrl, &hw_perceptionInterface, &hw_actuatorInterface);

// 配置参数
MotionControl_SetBaseSpeed(&g_motionCtrl, 0.5f);  // 0.5 m/s

// 启动控制
MotionControl_Start(&g_motionCtrl);

// 在500Hz定时器中断中调用
void TIMER_IRQHandler(void) {
    MotionControl_Update(&g_motionCtrl);
}
```

## 参数调试

### 调试顺序

1. **内环（轮速）**
   - 断开外环，单独测试速度跟踪
   - 先调Kp，再调Ki
   - 目标：速度跟踪误差<5%

2. **中环（角速度）**
   - 固定内环，断开外环
   - 测试原地旋转响应
   - 目标：快速无超调

3. **外环（横向位置）**
   - 完整系统测试
   - 先调Kp，再调Kd
   - 目标：平滑循迹无摆动

### 关键参数

参数在 `motion_config.h` 中定义：

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `LATERAL_KP` | 0.30 | 横向位置比例增益 |
| `LATERAL_KD` | 0.15 | 横向位置微分增益 |
| `OMEGA_KP` | 0.80 | 角速度比例增益 |
| `OMEGA_KI` | 0.10 | 角速度积分增益 |
| `SPEED_KP` | 200.0 | 轮速比例增益 |
| `SPEED_KI` | 50.0 | 轮速积分增益 |
| `FF_K_ACCEL` | 50.0 | 加速度前馈系数 |
| `FF_K_FRICTION` | 300.0 | 摩擦前馈系数 |
| `FF_K_STATIC` | 80.0 | 静摩擦补偿 |

## 性能指标

| 指标 | 目标值 |
|------|--------|
| 循迹精度 | ±1cm |
| 速度稳定性 | ±5% |
| 控制频率 | 500Hz |
| CPU占用率 | <6% (MSPM0G3507 @ 32MHz) |
| 代码量 | ~1000行 |
| Flash占用 | ~8-10KB |
| SRAM占用 | ~3KB |

## 状态机

```
INIT → IDLE → RUNNING ⇄ LINE_LOST
                ↓
           EMERGENCY
```

- `INIT`: 初始化状态
- `IDLE`: 空闲，电机停止
- `RUNNING`: 正常运行
- `LINE_LOST`: 丢线搜索（0.5秒超时后停止）
- `EMERGENCY`: 紧急停止

## 常见问题

### Q1: 小车左右摆动严重
**A**: 降低 `LATERAL_KP`，增加 `LATERAL_KD`

### Q2: 转弯响应慢
**A**: 增加 `OMEGA_KP`，检查IMU是否正常工作

### Q3: 速度不稳定
**A**: 检查编码器连接，调整 `SPEED_KP` 和 `SPEED_KI`

### Q4: 高速时冲出轨道
**A**: 降低 `BASE_SPEED`，或增加 `LATERAL_KD`

### Q5: 低速时抖动
**A**: 调整 `FF_K_STATIC`，确保能克服静摩擦

## 物理参数标定

### 轮径标定
```c
// 推车直线10米，记录编码器脉冲数
float wheel_radius = 10.0f / (total_pulses / ENCODER_PPR) / (2 * 3.14159f);
```

### 轮距标定
```c
// 原地旋转360度，记录左右轮脉冲差
float wheel_base = (left_pulses - right_pulses) * wheel_radius / (2 * 3.14159f);
```

## 扩展功能

### 1. 自适应速度规划
根据路径曲率动态调整速度：
```c
// 在外环控制中添加
float curvature = estimate_curvature(lateral_error);
ctrl->base_speed = adaptive_speed(curvature);
```

### 2. 电池电压补偿
```c
// 在前馈中添加
float voltage_comp = 12.0f / Battery_GetVoltage();
pwm_ff *= voltage_comp;
```

### 3. 卡尔曼滤波
融合编码器和IMU进行更精确的状态估计。

## 调试工具

### 性能监控
```c
uint32_t loop_count, max_exec_time;
MotionControl_GetPerformance(&ctrl, &loop_count, &max_exec_time);
printf("Loop: %lu, Max time: %lu us\n", loop_count, max_exec_time);
```

### 实时数据
```c
int16_t error = MotionControl_GetLateralError(&ctrl);
float v_left, v_right;
MotionControl_GetWheelSpeed(&ctrl, &v_left, &v_right);
printf("Error: %d, V_L: %.2f, V_R: %.2f\n", error, v_left, v_right);
```

## 参考文档

- [运动控制层设计文档](../../docs/superpowers/specs/2026-07-14-motion-control-design.md)
- [小车底盘控制系统完整设计框架](./小车底盘控制系统完整设计框架.md)
- [运动控制层架构详解](./运动控制层架构详解.md)

## 作者与维护

- **开发日期**: 2026-07-14
- **芯片平台**: TI MSPM0G3507
- **应用场景**: 电赛循迹小车

## 许可证

本模块为电赛项目内部代码，仅供学习和比赛使用。
