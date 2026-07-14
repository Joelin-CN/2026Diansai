# 运动控制层设计文档

## 项目信息
- **日期**: 2026-07-14
- **项目**: 电赛循迹小车运动控制层
- **芯片**: MSPM0G3507 (32MHz Cortex-M0+)
- **底盘**: 四轮差速驱动

---

## 1. 设计目标

### 1.1 功能目标
实现基于循迹传感器的三环串级控制系统，充分利用现有硬件（12路循迹传感器、4路编码器、ICM42688陀螺仪），提升循迹精度、速度稳定性和抗扰能力。

### 1.2 性能指标
| 指标 | 目标值 |
|------|--------|
| 循迹精度 | ±1cm |
| 速度稳定性 | ±5% |
| 控制频率 | 500Hz |
| CPU占用率 | <6% |
| 响应时间 | <10ms |

---

## 2. 系统架构

### 2.1 三环控制结构

```
┌─────────────────────────────────────────┐
│ 外环: 横向位置控制 (100Hz)              │
│   输入: 12路循迹传感器误差               │
│   算法: PID                              │
│   输出: ω_ref (期望角速度)               │
└──────────────┬──────────────────────────┘
               ↓
┌─────────────────────────────────────────┐
│ 中环: 角速度控制 (500Hz)                │
│   输入: ω_ref vs ICM42688陀螺仪         │
│   算法: PI反馈                           │
│   输出: Δv (差速修正量)                  │
└──────────────┬──────────────────────────┘
               ↓
┌─────────────────────────────────────────┐
│ 内环: 轮速控制 (500Hz)                  │
│   ┌──────────────┐   ┌──────────────┐  │
│   │ 前馈控制      │ + │ 反馈控制     │  │
│   │ • 惯性补偿    │   │ • 速度PI     │  │
│   │ • 摩擦补偿    │   │ • 抗饱和     │  │
│   └──────────────┘   └──────────────┘  │
│   输出: PWM (-1000~+1000)               │
└─────────────────────────────────────────┘
```

### 2.2 模块划分

```
modules/Motion Control/
├── motion_config.h              [参数配置]
├── motion_kinematics.h/c        [差速运动学]
├── motion_feedback.h/c          [反馈控制+状态估计]
├── motion_feedforward.h/c       [前馈控制]
└── motion_control.h/c           [主控制器]
```

---

## 3. 核心模块设计

### 3.1 配置模块 (motion_config.h)

**职责**: 集中管理所有物理参数、控制参数

**内容**:
- 物理参数: 轮距、轮径、编码器PPR、减速比
- PID参数: 三个环路的Kp/Ki/Kd
- 前馈参数: 惯性、摩擦、静摩擦系数
- 约束参数: 最大速度、加速度

---

### 3.2 运动学模块 (motion_kinematics)

**职责**: 差速底盘正/逆运动学转换

**API**:
```c
// 逆运动学: 底盘速度 → 轮速
void DiffKin_Inverse(float v, float omega, 
                     float *v_left, float *v_right);

// 正运动学: 轮速 → 底盘速度
void DiffKin_Forward(float v_left, float v_right,
                     float *v, float *omega);
```

**实现**:
```
逆运动学:
  v_left = v - (轮距/2) * omega
  v_right = v + (轮距/2) * omega

正运动学:
  v = (v_left + v_right) / 2
  omega = (v_right - v_left) / 轮距
```

---

### 3.3 反馈控制模块 (motion_feedback)

**职责**: PID控制器 + 状态估计

**组件**:
1. **PID控制器**: 通用PID实现，支持抗积分饱和
2. **状态估计器**: 编码器脉冲 → 轮速转换

**API**:
```c
// PID控制器
float PID_Update(PID_t *pid, float setpoint, float measurement, float dt);
void PID_Reset(PID_t *pid);

// 状态估计
void StateEst_Init(StateEstimator_t *est, EncoderInterface_t *encoder);
void StateEst_Update(StateEstimator_t *est);
float StateEst_GetLeftSpeed(StateEstimator_t *est);
float StateEst_GetRightSpeed(StateEstimator_t *est);
```

---

### 3.4 前馈控制模块 (motion_feedforward)

**职责**: 基于模型的转矩前馈（准电流环效果）

**前馈项**:
1. **惯性补偿**: PWM = K_accel × 加速度
2. **摩擦补偿**: PWM = K_friction × 速度
3. **静摩擦补偿**: PWM = K_static × sign(速度)

**API**:
```c
void Feedforward_Init(Feedforward_t *ff, 
                      float k_accel, float k_friction, float k_static);
float Feedforward_Update(Feedforward_t *ff, float target_v, float dt);
```

**效果**: 达到真电流环70-80%性能

---

### 3.5 主控制器 (motion_control)

**职责**: 三环协调、传感器读取、执行器输出、状态机管理

**控制流程**:
```
1. 读取传感器
   - 循迹传感器 → 横向误差
   - 编码器 → 左右轮速
   - IMU → 角速度

2. 外环控制 (100Hz)
   - 横向误差 → PID → 期望角速度

3. 中环控制 (500Hz)
   - 期望角速度 vs 实际角速度 → PI → 差速修正

4. 内环控制 (500Hz)
   - 左轮: 前馈 + 反馈 → PWM_left
   - 右轮: 前馈 + 反馈 → PWM_right

5. 输出到电机
```

**状态机**:
- INIT: 初始化
- IDLE: 空闲
- RUNNING: 运行中
- LINE_LOST: 丢线搜索
- EMERGENCY: 紧急停止
- ERROR: 故障

---

## 4. 接口设计

### 4.1 虚表接口

**感知层接口**:
```c
typedef struct {
    LineSensorInterface_t *lineSensor;  // 循迹传感器
    EncoderInterface_t *encoder;        // 编码器
    ImuInterface_t *imu;                // IMU
} PerceptionInterface_t;
```

**执行器层接口**:
```c
typedef struct {
    MotorInterface_t *motor;  // 电机控制
} ActuatorInterface_t;
```

### 4.2 对外API

```c
// 初始化
bool MotionControl_Init(MotionControl_t *ctrl,
                        PerceptionInterface_t *perception,
                        ActuatorInterface_t *actuator);

// 主控制循环 (500Hz调用)
void MotionControl_Update(MotionControl_t *ctrl);

// 参数配置
void MotionControl_SetBaseSpeed(MotionControl_t *ctrl, float speed);
void MotionControl_SetLateralPID(MotionControl_t *ctrl, 
                                  float kp, float ki, float kd);
void MotionControl_SetOmegaPI(MotionControl_t *ctrl, float kp, float ki);
void MotionControl_SetSpeedPI(MotionControl_t *ctrl, float kp, float ki);
void MotionControl_SetFeedforward(MotionControl_t *ctrl,
                                   float k_accel, float k_friction, float k_static);

// 控制命令
void MotionControl_Start(MotionControl_t *ctrl);
void MotionControl_Stop(MotionControl_t *ctrl);
void MotionControl_EmergencyStop(MotionControl_t *ctrl);

// 状态查询
ControlState_t MotionControl_GetState(MotionControl_t *ctrl);
int16_t MotionControl_GetLateralError(MotionControl_t *ctrl);
void MotionControl_GetWheelSpeed(MotionControl_t *ctrl, 
                                  float *left, float *right);
```

---

## 5. 参数标定

### 5.1 物理参数标定

| 参数 | 标定方法 |
|------|----------|
| **轮径** | 推车10米，记录编码器脉冲数计算 |
| **轮距** | 原地旋转360°，根据左右轮脉冲差计算 |
| **编码器PPR** | 手动转1圈，统计脉冲数（已知334PPR） |

### 5.2 PID参数调试顺序

1. **内环（轮速）**: 
   - 断开外环，单独测试
   - 先调Kp到临界振荡，再加Ki消除稳态误差
   - 目标：速度跟踪误差<5%

2. **中环（角速度）**:
   - 固定内环，断开外环
   - 先调Kp，再加Ki
   - 目标：角速度响应快速无超调

3. **外环（横向位置）**:
   - 完整系统测试
   - 先调Kp使车能跟线，再加Kd减少振荡
   - 目标：循迹平滑无摆动

### 5.3 前馈参数标定

| 参数 | 标定方法 |
|------|----------|
| **K_accel** | 测试不同加速度下的PWM需求 |
| **K_friction** | 匀速运动时的PWM vs 速度关系 |
| **K_static** | 最小启动PWM值 |

---

## 6. 性能评估

### 6.1 计算量分析

| 模块 | 指令数/周期 | 占比 |
|------|------------|------|
| 传感器读取 | 270 | 33% |
| 外环PID | 28 | 3% |
| 中环PI | 55 | 7% |
| 内环前馈+反馈 | 220 | 27% |
| 电机输出 | 40 | 5% |
| 其他开销 | 200 | 25% |
| **总计** | **~813** | **1.3%** |

**结论**: 32MHz主频下，500Hz控制占用<2% CPU

### 6.2 内存占用

- **Flash**: 约8-10KB
- **SRAM**: 约3KB (包含状态、缓冲区)
- **栈**: 约1KB

---

## 7. 风险与约束

### 7.1 硬件约束

| 约束 | 影响 | 解决方案 |
|------|------|----------|
| **无电流采样** | 无法真电流环 | 用转矩前馈近似（70-80%效果） |
| **I2C速率100kHz** | 循迹传感器读取延迟0.2ms | 可升级到400kHz减少延迟 |
| **M0+无FPU** | 浮点运算慢 | 关键路径用定点数优化 |

### 7.2 性能风险

| 风险 | 缓解措施 |
|------|----------|
| **高速切弯** | 自适应速度规划（弯道减速） |
| **地面不平** | IMU角速度闭环抗扰 |
| **电池电压下降** | 可选增加ADC电压补偿 |

---

## 8. 测试计划

### 8.1 单元测试
- PID控制器阶跃响应测试
- 前馈模型精度测试
- 运动学正逆变换测试

### 8.2 集成测试
- 单环测试（内环→中环→外环）
- 丢线处理测试
- 急停响应测试

### 8.3 性能测试
- 直线循迹测试（测横向误差RMS）
- S弯循迹测试（测最大误差）
- 速度阶跃测试（测稳定时间）
- 长时间运行测试（测稳定性）

---

## 9. 实施计划

### Phase 1: 基础模块 (1-2天)
1. 实现运动学模块
2. 实现状态估计器
3. 实现PID控制器
4. 单元测试

### Phase 2: 控制器集成 (2-3天)
5. 实现前馈控制器
6. 实现主控制器框架
7. 集成三环控制
8. ICM42688驱动实现

### Phase 3: 调试优化 (2-3天)
9. 参数标定
10. 逐环调试
11. 性能测试
12. 优化和文档

**总计**: 5-8天完成

---

## 10. 后续扩展

### 10.1 可选功能
- 电池电压补偿（需增加ADC）
- 自适应参数调整
- 数据记录和回放
- 无线参数调试

### 10.2 性能提升
- 卡尔曼滤波（编码器+IMU融合）
- 模型预测控制（MPC）
- 曲率自适应速度规划

---

## 附录

### A. 电机参数 (MG513X)
- 电磁转矩系数 KT = 0.00984 Nm/A
- 反电动势系数 Ke = 0.00103 V/rpm
- 电阻 R = 2.3Ω
- 电感 L = 4.45mH
- 减速比 = 1:28
- 额定电压 = 12V

### B. 参考资料
- 《小车底盘控制系统完整设计框架.md》
- 《运动控制层架构详解.md》
- MSPM0G3507数据手册
- TB6612FNG数据手册
