# 串级PID控制架构设计 - 2026-07-31

## 1. Executive Summary

本文档提出用**三层串级PID架构**替换现有的前馈+反馈组合控制架构，核心变更包括：

- **删除前馈控制器**：移除所有基于模型的前馈补偿（惯性、摩擦、静摩擦）
- **引入陀螺仪反馈**：使用ICM42688的Z轴角速度直接闭环，替代基于IR横向误差时间导数的航向估计
- **三层串级PID**：
  - **外环（50Hz）**：lateral_error_IR → omega_target（横向误差到目标角速度）
  - **内环（100Hz）**：omega_error → delta_v_wheels（角速度误差到轮速差）
  - **底层（100Hz）**：v_target → PWM（纯PI轮速控制）

**设计目标**：提高循迹精度和稳定性，利用陀螺仪的高精度角速度反馈，消除前馈参数标定依赖。

---

## 2. 现有架构分析

### 2.1 控制流程

**当前数据流（前馈+反馈组合）**：

```
[50Hz 决策层]
IR传感器 → perception → lateral_error, heading_error
                             ↓
                        PD控制器 → omega
                             ↓
[100Hz 运动控制层]
(v, omega) → 逆运动学 → (vL_target, vR_target)
                             ↓
                  ┌──────────┴──────────┐
                  ↓                     ↓
         [左轮控制器]           [右轮控制器]
    前馈(惯性+摩擦+静摩擦)   前馈(惯性+摩擦+静摩擦)
              +                      +
         PI反馈(速度误差)        PI反馈(速度误差)
              ↓                      ↓
          PWM_left               PWM_right
```

**关键特征**：
- **heading_error来源**：从lateral_error的时间导数估计，**不是真实陀螺仪数据**
- **前馈控制**：基于电机模型的开环补偿，依赖参数标定
- **反馈控制**：纯PI轮速控制，误差=(target - actual)

### 2.2 前馈控制器分析

**实现位置**：`modules/MotionControl/src/motion_feedforward.c`

**控制方程**：

```
PWM_ff = k_accel × acceleration + k_friction × velocity + k_static × sign(velocity)
```

其中：
- `acceleration = (v_target - v_prev) / dt`（数值微分）
- `k_static`仅在`|velocity| > 0.01 m/s`时施加，避免停止时抖动

**三个前馈系数**：

| 参数 | 值 | 物理意义 | 用途 |
|------|-----|---------|------|
| `FF_K_ACCEL` | 50.0 PWM/(m/s²) | 惯性补偿系数 | 补偿加速时的电机转矩需求 |
| `FF_K_FRICTION` | 300.0 PWM/(m/s) | 黏性摩擦系数 | 补偿恒速运动时的摩擦阻力 |
| `FF_K_STATIC` | 80.0 PWM | 静摩擦补偿 | 克服启动时的最大静摩擦力 |

**标定需求**：
- 需要实车测试不同加速度、速度、地面条件
- 参数依赖车体质量、电机特性、轮胎材质、地面材质
- 更换硬件后必须重新标定

**调用位置**：
- `motion_control.c::WheelController_Update()` - 每个轮子独立一个前馈控制器实例
- 执行频率：100Hz（PID控制频率）

### 2.3 现有参数汇总

| 类别 | 参数名 | 值 | 单位 | 说明 |
|------|--------|-----|------|------|
| **物理参数** | `WHEEL_BASE` | 0.214 | m | 轮距 |
| | `WHEEL_RADIUS` | 0.033 | m | 轮半径 |
| | `ENCODER_PPR` | 60000 | counts/rev | 编码器分辨率 |
| **控制频率** | `MAIN_LOOP_FREQ_HZ` | 500 | Hz | 编码器采样频率 |
| | `PID_CONTROL_FREQ_HZ` | 100 | Hz | PID执行频率 |
| | `DECISION_FREQ_HZ` | 50 | Hz | 决策层频率（隐含） |
| **轮速PI参数** | `SPEED_KP` | 200.0 | PWM/(m/s) | 比例增益 |
| | `SPEED_KI` | 50.0 | PWM/(m/s·s) | 积分增益 |
| | `SPEED_OUTPUT_MAX` | 500.0 | PWM | 反馈输出限幅 |
| **前馈参数** | `FF_K_ACCEL` | 50.0 | PWM/(m/s²) | 加速度系数 |
| | `FF_K_FRICTION` | 300.0 | PWM/(m/s) | 摩擦系数 |
| | `FF_K_STATIC` | 80.0 | PWM | 静摩擦补偿 |
| **速度约束** | `MAX_SPEED` | 1.00 | m/s | 最大线速度 |
| | `MIN_SPEED` | 0.10 | m/s | 最小线速度 |
| | `MAX_OMEGA` | 6.0 | rad/s | 最大角速度 |
| | `MAX_ACCELERATION` | 2.0 | m/s² | 加速度限幅 |
| | `MAX_DECELERATION` | 3.0 | m/s² | 减速度限幅 |
| **决策层PD参数** | `kp_straight` | 0.5 | (rad/s)/cm | 直线段比例增益 |
| （playground_track.c） | `kd_straight` | 0.02 | (rad/s)/(cm/s) | 直线段微分增益 |
| | `kp_curve` | 1.0 | (rad/s)/cm | 弯道段比例增益 |
| | `kd_curve` | 0.04 | (rad/s)/(cm/s) | 弯道段微分增益 |
| | `omega_max_straight` | 2.0 | rad/s | 直线段角速度限幅 |
| | `omega_max_curve` | 2.0 | rad/s | 弯道段角速度限幅 |
| **PWM约束** | `PWM_MAX` | 100 | - | PWM上限 |
| | `PWM_MIN` | -100 | - | PWM下限 |

---

## 3. 新架构设计

### 3.1 架构对比

#### 架构A：标准串级PID（推荐）

```
[外环 - 50Hz 决策层]
lateral_error_IR → PID_lateral (P+I+D) → omega_target
                                              ↓
[内环 - 100Hz 运动控制层]
omega_gyro (ICM42688 Z轴) ←─┐
                             │
omega_error = omega_target - omega_gyro
                             ↓
                    PID_angular (P+I+D) → delta_v_wheels
                             ↓
v_base ± delta_v/2 → (vL_target, vR_target)
                             ↓
[底层 - 100Hz 轮速控制]
PI控制器（无前馈）→ PWM_left, PWM_right
```

**特点**：
- 内环输出是速度增量`delta_v`（单位：m/s）
- 逆运动学前施加差速：`vL_target = v_base - delta_v/2`, `vR_target = v_base + delta_v/2`
- 物理意义清晰：`delta_v`直接对应左右轮速差

#### 架构B：简化串级PID（备选）

```
[外环 - 50Hz]
lateral_error_IR → PID_lateral → omega_target
                                     ↓
[内环 - 100Hz]
omega_error → PID_angular → delta_PWM
                                ↓
PWM_base ± delta_PWM/2 → (PWM_left, PWM_right)
```

**特点**：
- 内环直接输出PWM增量`delta_PWM`
- 跳过轮速目标，直接施加PWM差速
- 实现更简单，但失去速度层的反馈解耦

#### 架构选择：**推荐架构A**

| 对比项 | 架构A（速度增量） | 架构B（PWM增量） |
|--------|------------------|------------------|
| **物理意义** | 清晰（m/s） | 模糊（PWM单位） |
| **参数调试** | 易于理解和调试 | 需要实验试凑 |
| **速度解耦** | 保留轮速PI反馈 | 失去速度层反馈 |
| **扩展性** | 易于增加EKF融合 | 难以融合状态估计 |
| **实现复杂度** | 中等 | 低 |
| **推荐度** | ⭐⭐⭐⭐⭐ | ⭐⭐⭐ |

### 3.2 数据流图

```
传感器层 (500Hz/1000Hz采样)
├─ 编码器 (TIM3=Left, TIM4=Right, 500Hz polling)
├─ IR传感器 (USART2, 异步接收)
└─ 陀螺仪 (ICM42688 SPI2, Z轴角速度, 1000Hz采样)
        ↓
═══════════════════════════════════════════════════
外环控制 (50Hz) - 横向误差到目标角速度
═══════════════════════════════════════════════════
perception_update():
  IR array → lateral_error (cm)
        ↓
PID_lateral (新增):
  Kp × lateral_error + Ki × ∫lateral_error + Kd × Δlateral_error
        ↓
  omega_target (rad/s)  [限幅: ±omega_max]
        ↓
═══════════════════════════════════════════════════
内环控制 (100Hz) - 角速度误差到轮速差
═══════════════════════════════════════════════════
读取陀螺仪:
  icm42688_read() → gyro_raw.z (LSB)
  omega_gyro = gyro_raw.z × scale - bias (rad/s)
        ↓
计算角速度误差:
  omega_error = omega_target - omega_gyro
        ↓
PID_angular (新增):
  Kp × omega_error + Ki × ∫omega_error + Kd × Δomega_error
        ↓
  delta_v_wheels (m/s)  [轮速增量]
        ↓
应用差速:
  vL_target = v_base - delta_v_wheels / 2
  vR_target = v_base + delta_v_wheels / 2
        ↓
═══════════════════════════════════════════════════
底层控制 (100Hz) - 轮速PI控制（无前馈）
═══════════════════════════════════════════════════
状态估计:
  编码器 → vL_actual, vR_actual (m/s)
        ↓
PI控制器（左轮）:
  error_L = vL_target - vL_actual
  PWM_left = Kp × error_L + Ki × ∫error_L  [移除前馈]
        ↓
PI控制器（右轮）:
  error_R = vR_target - vR_actual
  PWM_right = Kp × error_R + Ki × ∫error_R  [移除前馈]
        ↓
电机驱动:
  TB6612 → Motor_left, Motor_right
```

### 3.3 控制周期设计

| 层级 | 频率 | 周期 | 任务 | 实现位置 |
|------|------|------|------|---------|
| **外环** | 50 Hz | 20 ms | 横向误差→目标角速度 | `playground_track.c::pg_decide_50hz()` |
| **内环** | 100 Hz | 10 ms | 角速度误差→轮速差 | `motion_control.c::MotionControl_Update()` |
| **底层** | 100 Hz | 10 ms | 轮速控制（纯PI） | `motion_control.c::WheelController_Update()` |
| **采样** | 500 Hz | 2 ms | 编码器采样 | `playground_track.c::PlaygroundTrack_RunFastCycle()` |
| **陀螺仪** | 1000 Hz | 1 ms | IMU硬件采样（内部FIFO） | 硬件自动 |

**时序说明**：
- 外环50Hz匹配决策层计算能力（perception + 决策）
- 内环100Hz匹配执行器响应时间（TB6612 PWM响应约10ms）
- 陀螺仪1000Hz采样但内环仅100Hz读取，可增加低通滤波抑制噪声

---

## 4. PID参数设计

### 4.1 外环PID（Lateral Error → Omega Target）

**控制对象**：横向偏差 → 目标角速度

**物理模型**：
- 输入：lateral_error（单位：cm，IR传感器质心偏差）
- 输出：omega_target（单位：rad/s，目标角速度）

**从现有PD参数推导**：

现有参数（playground_track.c）：
```c
kp_straight = 0.5f;   // (rad/s) / cm
kd_straight = 0.02f;  // (rad/s) / (cm/s)
omega_max_straight = 2.0f;  // rad/s
```

**新增积分项（Ki）**：
- 现有PD控制无法消除稳态偏差（如地面倾斜、车轮直径差）
- 建议初值：`ki_lateral = 0.1f`（单位：(rad/s) / (cm·s)）

**推导逻辑**：
- Kp决定响应速度：偏差1cm时产生0.5 rad/s角速度
- Kd抑制振荡：偏差变化率1 cm/s时产生0.02 rad/s阻尼
- Ki消除稳态误差：偏差持续1秒时积分项贡献0.1 rad/s

**参数初值表**：

| 场景 | Kp | Ki | Kd | omega_max | 说明 |
|------|----|----|----|-----------