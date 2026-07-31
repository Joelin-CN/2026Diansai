# 坐标系深度分析报告

**日期**: 2026-07-30
**分析范围**: STM32智能小车项目全部代码模块
**分析者**: Claude (Opus 4.8)
**状态**: 深度分析完成

---

## 目录

1. [坐标系定义清单](#1-坐标系定义清单)
2. [坐标变换追踪](#2-坐标变换追踪)
3. [数据流坐标系追踪](#3-数据流坐标系追踪)
4. [位置配置坐标系验证](#4-位置配置坐标系验证)
5. [左右定义一致性分析](#5-左右定义一致性分析)
6. [发现的问题](#6-发现的问题)
7. [IR权重符号分析](#7-ir权重符号分析)
8. [修复建议](#8-修复建议)
9. [术语对照表](#9-术语对照表)

---

## 1. 坐标系定义清单

### 1.1 [代码] 坐标系 (Vehicle Body Frame / Algorithm Frame)

| 轴 | 方向 | 依据来源 |
|----|------|----------|
| X | 前方 (Forward) | EKF.c:131 `x' = x + v*cos(theta)*dt` (theta=0 时 x 增加) |
| Y | 左侧 (Left) | EKF.c:132 `y' = y + v*sin(theta)*dt` (theta=90deg 时 y 增加) |
| Z | 上方 (Up) | 右手定则从 X,Y 推导 |

**代码位置及注释**:
- `modules/Sens-Decision/src/EKF.c`:132 - 运动方程隐含定义
- `modules/Sens-Decision/src/config.c`:27-30 - 明确注释 "坐标系说明（代码坐标系）"
- `modules/Sens-Decision/src/config.c`:302-305 - IR阵列位置注释区
- `modules/Sens-Decision/src/preprocess.c`:11-13 - IMU适配器注释
- `API_PITFALLS_GUIDE.md`:418-419 - "代码帧定义"
- `README.md`:236-238 - "坐标系说明"

### 1.2 [IMU物理] 坐标系 (ICM42688 Hardware Frame)

| 轴 | 方向 | 依据来源 |
|----|------|----------|
| X | 右侧 (Right) | `preprocess.c`:12 注释 |
| Y | 前方 (Forward) | `preprocess.c`:12 注释 |
| Z | 上方 (Up) | `preprocess.c`:12 注释 |

**代码位置**:
- `modules/Sens-Decision/src/preprocess.c`:12 - "物理IMU安装方向: X=右侧, Y=前方, Z=上方"
- `API_PITFALLS_GUIDE.md`:401-402 - "物理IMU: X=右侧, Y=前方, Z=上方"
- `logs/2026-07-30_coord_system_complete_fix.md`:15-16 - 表格对比

### 1.3 [IR物理] 传感器布局约定

IR传感器阵列有自己的物理布局描述约定（用于通道编号和位置标注）:

| 约定 | 含义 |
|------|------|
| 通道编号 | 0=最右侧, 7=最左侧 (从小车前方视角) |
| 位置坐标系 | 未明确统一, 多种标注混合使用 |

**代码位置**:
- `README.md`:190-194 - 传感器布局图 (用 "物理X" 标注, 右侧为正)
- `modules/Sens-Decision/src/config.c`:31-40 - Y坐标注释 (标注了"最右侧"对"左侧")

### 1.4 坐标系关系总结

```
[IMU物理]           [代码]
  X = 右侧  --------映射------>  X = 前方
  Y = 前方  --------映射------>  Y = 左侧
  Z = 上方  --------映射------>  Z = 上方

变换: code_X = phys_Y, code_Y = -phys_X, code_Z = phys_Z
```

---

## 2. 坐标变换追踪

### 2.1 已实现的变换: IMU数据

**位置**: `modules/Sens-Decision/src/preprocess.c`:30-44

**函数**: `imu_adapt_to_code_frame(imu_data_t *imu)`

**变换公式**:
```c
// 加速度向量
code_X =  phys_Y    // 前方 = 物理Y
code_Y = -phys_X    // 左侧 = -(物理X/右侧)
code_Z =  phys_Z    // 上方（不变）

// 角速度向量 - 同样的变换
code_gyro_X =  phys_gyro_Y
code_gyro_Y = -phys_gyro_X
code_gyro_Z =  phys_gyro_Z
```

**调用链**:
```
preprocess_update() [preprocess.c:66-69]
  → sensor_read(SENSOR_ID_IMU)  // 读取IMU原始数据 [IMU物理]
  → imu_adapt_to_code_frame()   // 变换到 [代码]
  → frame->imu  // 输出 [代码]
```

**变换完整性评估**:

| 数据字段 | 是否变换 | 状态 |
|----------|----------|------|
| accel_mps2[0,1,2] | 是 | 正确 |
| gyro_radps[0,1,2] | 是 | 正确 |
| 姿态角 (如果有) | 不适用 | 当前代码不转换姿态角 |

**关键观察**: 姿态角（四元数/欧拉角）不由 `imu_adapt_to_code_frame` 处理。但姿态角由 ICM42688 的 AHRS 算法（`ahrs_hal.c`）在传感器内部计算，基础数据是物理帧的加速度/角速度。当前代码流中，IMU 的 `gyro_radps[2]` 虽然被转换，但按照注释（`preprocess.c`:20-21），Z 轴在两个坐标系中共享同一方向和旋转约定，因此 `code_gyro_Z = phys_gyro_Z` 成立。**此处正确**。

### 2.2 未变换的数据

| 数据 | 当前坐标系 | 是否需要变换 | 风险 |
|------|-----------|-------------|------|
| 编码器速度 | N/A (标量) | 否 | 速度是标量，左右轮之分通过索引区分 |
| 编码器位置配置 | [代码] | 否 | 正确 |
| IR阵列位置配置 | 待验证 | 见第4节 | 见第4节 |
| IR传感器读数 | N/A (原始值) | 否 | 红外反射率值不依赖坐标系 |

### 2.3 坐标变换函数清单

| 函数名 | 文件 | 类型 | 变换内容 |
|--------|------|------|----------|
| `imu_adapt_to_code_frame()` | `preprocess.c`:30 | [IMU物理] → [代码] | accel + gyro 3-vector |

**全项目只有一个坐标变换函数**。

---

## 3. 数据流坐标系追踪

### 3.1 数据流1: IMU数据

```
ICM42688 硬件
  ↓ 输出原始数据 [IMU物理] (accel_raw, gyro_raw)
sensor_read(SENSOR_ID_IMU)
  ↓ 刻度因子转换, 输出 imu_data_t [IMU物理]
imu_adapt_to_code_frame()                            [preprocess.c:68]
  ↓ accel_mps2, gyro_radps 转换为 [代码]
frame->imu 存储在 sensor_frame_t                     [代码]
  ↓
目前 IMU 数据在 EKF 中的使用:
  - gyro_radps[2] = 偏航角速度 [代码, Z轴不变]
  - 但 v1.1 更新后, EKF 已将 IMU 陀螺仪观测移除
  - 当前 EKF 使用 2观测 (v_encoder + omega_encoder)
  - IMU 数据传递给上层但当前未参与 EKF 更新
```

**状态**: 变换正确，但数据当前未在 EKF 中使用（EKF 简化为 2 观测模型）。

### 3.2 数据流2: 编码器数据

```
编码器硬件 (TIM3 = 左轮, TIM4 = 右轮)
  ↓ 硬件计数 (int32_t, 带方向)
sensor_read(SENSOR_ID_ENCODER_0/1)
  ↓ 转换为 encoder_data_t
  ↓ speed_mps = (delta_count / PPR) * (2*PI*R) / dt   [接口层计算]
frame->encoders[0] = 左轮, frame->encoders[1] = 右轮   [索引约定]
  ↓
state_evaluator_update()                                [state_evaluate.c:106-110]
  left_speed  = encoders[left_indices[0]].speed_mps    → v_left
  right_speed = encoders[right_indices[0]].speed_mps   → v_right
  v_encoder = (right_speed + left_speed) / 2.0f
  omega_encoder = (right_speed - left_speed) / wheel_track_m
  ↓
  observation[0] = v_encoder
  observation[1] = omega_encoder
  ↓
ekf_update(observation)                                 [EKF.c]
  ↓ 观测模型: H映射 observation[i] → EKF状态
EKF 状态输出: [x, y, theta, v, omega]  [代码]
  ↓
motion_control 读取速度指令和控制输出
```

**坐标系标注**:
- 编码器计数: 标量, 方向由 `direction` 系数处理
- `v_encoder`: 标量 (无方向性, 混合左右轮的平均值)
- `omega_encoder`: 标量 (Z轴角速度)
- EKF 状态 [x, y, theta]: [代码] (X=前方, Y=左侧)

**差速公式分析**:
```c
// state_evaluate.c:110
omega_encoder = (right_speed - left_speed) / wheel_track_m;
```

该公式假设 `omega = (v_right - v_left) / L`。物理含义: 右轮快 → 车左转(正omega/逆时针) → theta 增加。

在代码坐标系中 (theta 由 EKF 运动模型定义: `theta=90deg → Y增加 → 左侧`), 逆时针为正。验证: 如果右轮快, 车逆时针转(左转), theta 增加。与公式一致。**正确**。

### 3.3 数据流3: IR传感器数据

```
IR传感器硬件 (8通道光反射传感器)
  ↓ UART 接收 12-bit ADC 值 [0-4095]
IrUartSensor_Process() / GetAnalog()
  ↓ ir_array_data_t (raw values)
frame->ir [无坐标系, 原始值]
  ↓
perception_update()                                     [perception.c:39]
  ↓ 黑线强度反转: black_strength = white_ref - raw
  ↓ 加权质心: lateral_error = Σ(weight[i] * strength[i]) / Σ(strength[i])
  ↓
perception_result_t.lateral_error  [标量, 单位: 归一化权重·cm]
  ↓
behavior_planner_update()
  ↓ 使用 lateral_error 驱动转向决策
```

**lateral_error 符号约定** (来自 `README.md`:265-268):
```
lateral_error < 0: 车体偏右（线在传感器右侧）→ 需要向左修正
lateral_error > 0: 车体偏左（线在传感器左侧）→ 需要向右修正
lateral_error ≈ 0: 车体居中
```

---

## 4. 位置配置坐标系验证

### 4.1 编码器/轮子位置

**配置** (`config.c`:109-110):
```c
static const float encoder_x[SD_ENCODER_COUNT] = {0.0935f, 0.0935f};
static const float encoder_y[SD_ENCODER_COUNT] = {0.107f, -0.107f};
```

**赋值** (`config.c`:222-223):
```c
encoder->position.x_m = encoder_x[index];  // index=0: 0.0935; index=1: 0.0935
encoder->position.y_m = encoder_y[index];  // index=0: 0.107;  index=1: -0.107
```

**推定坐标系**: **[代码] 坐标系**

| 编码器索引 | 硬件对应 | position (X, Y) | 物理含义 (假设[代码]坐标系) |
|-----------|----------|-----------------|---------------------------|
| 0 (ENCODER_LEFT) | 左轮 TIM3 | (0.0935, **+0.107**) | 前方93.5mm, **左侧107mm** |
| 1 (ENCODER_RIGHT) | 右轮 TIM4 | (0.0935, **-0.107**) | 前方93.5mm, **右侧107mm** |

**判定理由**:
1. 配置的 Y 值 ±0.107 合计 ~0.214m，与 `wheel_track_m = 0.214f` 一致
2. 左轮 Y=+107mm (代码帧左侧), 右轮 Y=-107mm (代码帧右侧)
3. 注释未明确标注坐标系，但数值模式与[代码]坐标系一致

**结论**: 编码器位置使用[代码]坐标系。左轮在Y正方向(左侧)，与代码坐标系定义一致。**正确**。

### 4.2 IR传感器阵列位置

**配置** (`config.c`:328-330):
```c
g_sens_decision_config.perception.position.x_m = 0.183f;   // X=前
g_sens_decision_config.perception.position.y_m = 0.0f;      // Y=居中
g_sens_decision_config.perception.position.z_m = -0.02f;    // Z=下方
```

**推定坐标系**: **[代码] 坐标系**（注释明确标注 `X轴: 前方为正, Y轴: 左侧为正`）

**判定理由**:
1. 注释明确说明使用代码坐标系 (`config.c`:302-305)
2. X=0.183m (前方183mm) 符合物理安装位置描述
3. Y=0.0m (居中) 说明阵列在车辆横向中心
4. Z=-0.02m (下方) 说明阵列低于车辆原点

**结论**: IR阵列位置使用[代码]坐标系。**正确**。

**重要说明** (`config.c` 注释): `perception.c` 当前 `lateral_error` 计算不使用 `position` 字段。这个位置配置是为未来几何补偿/传感器融合预留的。

### 4.3 IMU安装位置

**配置** (`config.c`:286-288):
```c
g_sens_decision_config.imu.position.x_m = 0.0f;
g_sens_decision_config.imu.position.y_m = 0.0f;
g_sens_decision_config.imu.position.z_m = 0.03f;
```

**推定坐标系**: [代码] 坐标系 (Z=上方, 0.03m = 在原点上方30mm)

**结论**: 正确，IMU 安装位置合理（在车辆中心点上方30mm处）。

### 4.4 IR权重数组坐标系

**配置** (`config.c`:78-80):
```c
static const float ir_weights[SD_IR_CHANNEL_COUNT] = {
    3.9861f, 2.8472f, 1.7083f, 0.5694f,     // 通道0-3
    -0.5694f, -1.7083f, -2.8472f, -3.9861f  // 通道4-7
};
```

**注释声称的坐标系**: [代码] (`config.c`:27-30: "坐标系说明（代码坐标系）")

**权重计算公式** (`config.c`:45-47):
```
weight[i] = Y[i] / 10mm（归一化）
右侧（+Y）为正值，左侧（-Y）为负值
```

**矛盾分析**:
- 注释说 "右侧（+Y）为正值"
- 但 [代码] 坐标系定义是 **+Y = 左侧**
- 这是直接的矛盾！
- 真正使用的是 "+Y = 右侧" 的约定，这与 [IMU物理] 坐标系 (+X = 右侧) 一致

**详见第7节 IR权重符号分析**。

---

## 5. 左右定义一致性分析

### 5.1 所有"左/右"定义点

| 位置 | 左的定义 | 右的定义 | 坐标系 | 是否一致 |
|------|---------|---------|--------|---------|
| `config.c:78-80` IR权重 | 通道4-7, 负权重 | 通道0-3, 正权重 | [代码]声称 / [IMU物理]实际 | **见第7节** |
| `config.c:109-110` 编码器位置 | index=0, Y=+0.107 | index=1, Y=-0.107 | [代码] | 与[代码]一致 |
| `config.c:164` 编码器索引 | left_indices[0]=0 | right_indices[0]=1 | 索引映射 | 一致 |
| `state_evaluate.c:106-107` 速度计算 | encoders[0] | encoders[1] | 索引映射 | 一致 |
| `state_evaluate.c:110` 差速公式 | omega=(v_right-v_left)/L | omega=(v_right-v_left)/L | [代码] Z轴 | 一致 |
| `motion_kinematics.c:31-37` 逆运动学 | v_left=v-omega*b/2 | v_right=v+omega*b/2 | N/A (标量) | 一致 |
| `motion_control.c:154-157` WheelController | wheel_left | wheel_right | 硬件通道 | 一致 |
| `motion_feedback.h:69-70` 状态估计 | v_left = left速度 | v_right = right速度 | N/A (标量) | 一致 |
| `encoder_adapter.c` 编码器映射 | TIM3 (PB4+PB5) | TIM4 (PD12+PD13) | 硬件通道 | 一致 |
| `motor_adapter.c` 控制引脚 | MOTOR_B (PE9) | MOTOR_C (PE11) | 硬件通道 | 一致 |

### 5.2 差速公式一致性

**state_evaluate.c:110**:
```c
omega_encoder = (right_speed - left_speed) / g_sens_decision_config.vehicle.wheel_track_m;
```

**motion_kinematics.c:46-47**:
```c
*v = (v_left + v_right) * 0.5f;
*omega = (v_right - v_left) / g_kinConfig.wheel_base;
```

**motion_kinematics.c:31-32,36-37** (逆运动学):
```c
// v_left  = v - (b/2) * omega
// v_right = v + (b/2) * omega
half_base = g_kinConfig.wheel_base / 2.0f;
*v_left  = v - half_base * omega;
*v_right = v + half_base * omega;
```

**三个公式完全一致**: `omega = (v_right - v_left) / wheelbase`

**物理验证**: 若右轮快于左轮 (v_right > v_left)，omega > 0 (逆时针/左转)，theta 增加。在 [代码] 坐标系中 theta=90deg 时 Y 增加 (向左)，与左转一致。**正确**。

### 5.3 编码器方向标记

**config.c:108**:
```c
static const int8_t encoder_directions[SD_ENCODER_COUNT] = {1, -1};
```

- 编码器0 (左轮): direction = +1 (正转时计数增加)
- 编码器1 (右轮): direction = -1 (正转时计数减少)

**分析**: 左右电机镜像安装是差速底盘的典型做法。direction=-1 意味着右轮电机的编码器方向与左轮相反。这是硬件安装问题，软件通过 direction 系数补偿。**正确处理**。

---

## 6. 发现的问题

### 6.1 问题清单

#### P0-Critical: IR权重坐标系混用 (文档内部矛盾)

**严重程度**: Critical (可能导致循迹方向错误)

**问题描述**: `config.c` 中的 `ir_weights` 数组标称使用[代码]坐标系，但实际权重值与[IMU物理]坐标系的传感器位置匹配。同时，`README.md` 和 `API_PITFALLS_GUIDE.md` 中记录了两种不同的权重数组。

**证据1**: `config.c`:27-30 声明使用[代码]坐标系 (X=前, Y=左)，但 `config.c`:46-47 说"右侧(+Y)为正值"。而在[代码]坐标系中 +Y=左侧。此处的 "+Y" 实际上对应的是物理传感器的 "X" 坐标（即 IMU 物理帧中的右侧方向）。

**证据2**: `README.md` 内部矛盾:
- 第279-282行: IR权重 `[3.9861, 2.8472, 1.7083, 0.5694, -0.5694, -1.7083, -2.8472, -3.9861]` 
- 第413-416行: IR权重 `[-0.5694, -1.7083, -2.8472, -3.9861, +0.5694, +1.7083, +2.8472, +3.9861]`
- **两处给出的数组不同，符号完全相反！**

**证据3**: `API_PITFALLS_GUIDE.md`:443-446 记录的权重数组:
```c
-0.5694f, -1.7083f, -2.8472f, -3.9861f,  // 右侧（负权重）
+0.5694f, +1.7083f, +2.8472f, +3.9861f   // 左侧（正权重）
```
这与 `config.c` 实际代码中的权重符号相反！

**证据4**: `logs/2026-07-30_coord_system_complete_fix.md`:73-82 记录的修复:
```
修改后权重 (符号翻转+大小顺序修正):
索引0-3 (右侧物理): -0.5694, -1.7083, -2.8472, -3.9861
索引4-7 (左侧物理): +0.5694, +1.7083, +2.8472, +3.9861
```
但当前 `config.c` 实际代码中的权重与"修改前"一致，而非"修改后"。

**代码位置**: 
- `modules/Sens-Decision/src/config.c`:78-80
- `README.md`:279-282 (版本A), 413-416 (版本B)
- `API_PITFALLS_GUIDE.md`:443-446
- `docs/GEOMETRY_UPDATE_2026-07-30.md`:79-83

**潜在影响**: 如果文档中记录的预期语义是正确的，当前代码可能导致 lateral_error 符号完全错误（车偏右时检测为正确，导致循迹反向）。

---

#### P0-Critical: IR权重坐标系与代码坐标系不匹配

**严重程度**: Critical

**问题描述**: IR 传感器 Y 坐标值没有经过 [IMU物理] → [代码] 的变换。

根据 `logs/2026-07-30_coord_system_complete_fix.md`:67-70 的推导:
```
物理帧 (IMU frame): X_phys = +39.86mm (channel 0, 右侧)
代码帧: code_Y = -X_phys = -39.86mm
```

但当前 `config.c`:32-39 的注释中:
```
通道0: Y = +39.8606 mm（最右侧）
通道7: Y = -39.8606 mm（最左侧）
```

直接使用了物理帧的 X 值作为代码帧的 Y 值，没有按 `code_Y = -phys_X` 变换。这意味着 IR 权重的 Y 坐标系实际使用的是 "右侧为正" 的约定（与 IMU 物理帧一致），而非代码帧的 "左侧为正" 约定。

**代码位置**: `modules/Sens-Decision/src/config.c`:31-47

**潜在影响**: 如果其他模块假定 IR 权重在[代码]坐标系中，符号不一致会导致横向偏差计算错误。

---

#### P1-Major: lateral_error 符号约定文档不一致

**严重程度**: Major

**问题描述**: 不同文档对 lateral_error 的符号约定描述存在矛盾。

`API_PITFALLS_GUIDE.md`:431-435:
```
| 车偏右 | < 0 | 向左转 |
| 车偏左 | > 0 | 向右转 |
```

`docs/GEOMETRY_UPDATE_2026-07-30.md`:193-195:
```
lateral_error > 0: 黑线在车辆左侧 → 需要向左转
lateral_error < 0: 黑线在车辆右侧 → 需要向右转
```

注意: "黑线在车辆左侧" = 车偏右。此时:
- API_PITFALLS_GUIDE 说: 车偏右 → lateral_error < 0 → 向左转
- GEOMETRY_UPDATE 说: 黑线在车辆左侧 → lateral_error > 0 → 向左转

**两个文档对 lateral_error 的符号定义相反！**

**代码位置**:
- `README.md`:265-268
- `API_PITFALLS_GUIDE.md`:431-435
- `docs/GEOMETRY_UPDATE_2026-07-30.md`:193-195

**潜在影响**: 开发者和AI助手无法确定正确的符号约定，调试方向错误。

---

#### P2-Minor: IMU accel_scale 配置声称使用 MPU6050 而非 ICM42688

**严重程度**: Minor (不影响功能但注释错误)

**问题描述**: `config.c`:227-251 中 IMU 加速度计刻度因子的注释说:
```
@origin MPU6050数据手册
量程配置: ±16g
ADC分辨率: 16位有符号整数
灵敏度: 2048 LSB/g
```

但实际使用的 IMU 是 **ICM42688**（项目 README 和 SPI 配置确认），而非 MPU6050。ICM42688 的加速度计刻度因子可能与 MPU6050 不同。虽然巧合的是 MPU6050 ±16g 与 ICM42688 ±16g 可能有相同的 LSB/g 值，但注释应该更新。

**代码位置**: `modules/Sens-Decision/src/config.c`:227-251

**潜在影响**: 如果更换 IMU 量程，使用错误数据手册的值可能导致加速度估计错误。

---

#### P2-Minor: 编码器位置注释缺少坐标系标注

**严重程度**: Minor

**问题描述**: `config.c`:109-110 的编码器位置配置:
```c
static const float encoder_x[SD_ENCODER_COUNT] = {0.0935f, 0.0935f};
static const float encoder_y[SD_ENCODER_COUNT] = {0.107f, -0.107f};
```

没有注释标明使用的坐标系。虽然可以通过上下文推断（与 wheel_track_m 一致），但缺少显式标注增加了理解难度。

**代码位置**: `modules/Sens-Decision/src/config.c`:109-110

---

#### P3-Information: 唯一坐标变换函数只有一处

**严重程度**: Information

**描述**: 全项目只有一个坐标变换函数 `imu_adapt_to_code_frame()`，位于 `preprocess.c` 中。IR 传感器位置数据没有相应的变换函数。原因是 IR 位置数据不需要在运行时进行坐标变换（位置参数直接填入正确的坐标系值即可），但需要确保填入值本身是正确的坐标系。

**状态**: 不是 Bug，但需要开发者注意。如果未来添加更多使用物理坐标系描述的传感器，需要考虑是否需要增加变换。

---

## 7. IR权重符号分析

### 7.1 问题根源

IR 权重问题的核心是: 传感器位置在物理空间中的描述有两套不同的坐标系约定，但在代码中混合使用而没有明确区分。

### 7.2 历史演变

根据日志文件分析，IR权重经历了以下演变:

**阶段1 (初始)**: 未知权重值

**阶段2 (coord_system_complete_fix)**: 将权重从"物理帧直接使用"修正为正确的[代码]坐标系权重:
```
修改后:右负左正 [-0.5694, -1.7083, -2.8472, -3.9861, +0.5694, +1.7083, +2.8472, +3.9861]
```

**阶段3 (GEOMETRY_UPDATE)**: 重新测量传感器物理位置，更新了 X 坐标、IR 阵列位置、轮距。但在更新权重时使用了新的物理位置值但**恢复到了阶段2之前的符号约定**:
```
当前值:右正左负 [3.9861, 2.8472, 1.7083, 0.5694, -0.5694, -1.7083, -2.8472, -3.9861]
```

**阶段4 (当前)**: 代码使用阶段3的值（右正左负），但 `API_PITFALLS_GUIDE.md:443-446` 记录的仍是阶段2的值（右负左正）。

### 7.3 两套权重的语义对比

| 属性 | 版本A (API_PITFALLS_GUIDE) | 版本B (config.c 当前代码) |
|------|---------------------------|--------------------------|
| 通道0-3 (右侧物理) | **负**权重 (-0.57 to -3.99) | **正**权重 (+3.99 to +0.57) |
| 通道4-7 (左侧物理) | **正**权重 (+0.57 to +3.99) | **负**权重 (-0.57 to -3.99) |
| 含义 | 右侧传感器 → 负贡献 | 右侧传感器 → 正贡献 |
| lateral_error 语义 | 车偏右 → 负值 → 向左修正 | 车偏右 → 正值 → 含义待验证 |

### 7.4 推荐验证方法

要确定哪个版本是正确的，必须进行实车测试:

```
1. 将小车居中于黑线上
2. 手动向右推偏 (黑线出现在物理右侧传感器上)
3. 观察 lateral_error 符号:
   - 如果 lateral_error < 0 → 版本A正确
   - 如果 lateral_error > 0 → 版本B正确
4. 手动向左推偏验证对称性
```

**当前建议**: 在实车验证之前，不要更改任何代码。两套权重都有各自的逻辑自洽性（取决于行为规划器如何处理 lateral_error 符号）。**应将实际测试结果作为最终判断依据**。

---

## 8. 修复建议

### 8.1 优先级排序

| 优先级 | 问题 | 建议 |
|--------|------|------|
| **P0-Critical** | IR权重文档矛盾 | **立即**进行实车 lateral_error 符号验证测试 |
| **P0-Critical** | IR权重坐标系 | 基于验证结果统一权重数组，并更新所有文档 |
| **P1-Major** | lateral_error 文档矛盾 | 统一所有文档中的符号约定描述 |
| **P2-Minor** | IMU 数据手册注释 | 从 MPU6050 更正为 ICM42688 |
| **P2-Minor** | 编码器位置注释 | 添加坐标系标注 |
| **P3-Information** | 坐标变换文档 | 可选，为每个传感器配置显式标注坐标系 |

### 8.2 详细修复方案

#### Fix 1: 实车 lateral_error 符号验证 (P0)

**不修改代码**。先进行硬件验证:

1. 编译当前代码烧录到 STM32
2. 将小车放置在白色背景的黑色轨道线上
3. 通过串口实时打印 `lateral_error` 值
4. 手动偏移小车位置，观察符号变化
5. 根据结果确定哪个权重版本是正确的

#### Fix 2: 修正或确认 IR 权重 (P0)

选项A: 如果验证结果显示当前权重符号错误，将权重数组改为:
```c
// [代码] 坐标系: 左侧传感器 +Y, 右侧传感器 -Y
// 代码坐标系: code_Y = -phys_X
static const float ir_weights[8] = {
    -0.5694f, -1.7083f, -2.8472f, -3.9861f,  // 通道0-3（右侧物理，code_Y 为负）
    +0.5694f, +1.7083f, +2.8472f, +3.9861f   // 通道4-7（左侧物理，code_Y 为正）
};
```

选项B: 如果验证结果显示当前权重符号正确，修改注释:
```c
// IR权重: weight[i] = X_phys[i] / 10mm (使用IMU物理帧X轴, 右侧为正)
// 注意: 这里的符号约定不同于代码坐标系 (代码+Y=左)
```

#### Fix 3: 统一 lateral_error 文档 (P1)

在 `README.md` 和 `API_PITFALLS_GUIDE.md` 中使用统一的表述:

```markdown
## lateral_error 符号约定

| 小车状态 | lateral_error | 行为 |
|----------|--------------|------|
| 车偏右 (线在左侧传感器) | [待验证] | 需要向左修正 |
| 车偏左 (线在右侧传感器) | [待验证] | 需要向右修正 |
| 居中 | ~0 | 直行 |
```

#### Fix 4: 修正 IMU 数据手册引用 (P2)

`config.c`:227-251 中提到的 MPU6050 应改为 ICM42688，并确认 LSB/g 值是否正确。

#### Fix 5: 添加编码器位置的坐标系注释 (P2)

```c
/**
 * 编码器当前位置 (代码坐标系: X=前方, Y=左侧)
 * 左轮: (93.5mm 前方, +107mm 左侧)
 * 右轮: (93.5mm 前方, -107mm 右侧)
 */
static const float encoder_x[SD_ENCODER_COUNT] = {0.0935f, 0.0935f};
static const float encoder_y[SD_ENCODER_COUNT] = {0.107f, -0.107f};
```

---

## 9. 术语对照表

| 中文 | 英文 | 含义 | 是否明确定义 |
|------|------|------|-------------|
| 前方 | Forward | 车头朝向, +X 在 [代码] | 是 |
| 左侧 | Left | 面对前方时的左侧, +Y 在 [代码] | 是 |
| 右侧 | Right | 面对前方时的右侧, -Y 在 [代码] | 是 |
| 上方 | Up | 垂直向上, +Z | 是 |
| 轮距 | Wheel Track | 左右轮中心间距, Y 方向 | 是 |
| 横向偏差 | Lateral Error | 车辆相对于轨道线的横向偏移 | **符号待验证** |
| "左侧" (IR阵列) | Left side of IR array | 物理上在小车左侧的传感器 | 是 (通道4-7) |
| "右侧" (IR阵列) | Right side of IR array | 物理上在小车右侧的传感器 | 是 (通道0-3) |

---

## 附录A: 所有坐标相关代码位置

| 文件 | 行号 | 内容 |
|------|------|------|
| `modules/Sens-Decision/src/preprocess.c` | 11-13 | IMU物理坐标系定义 |
| `modules/Sens-Decision/src/preprocess.c` | 16-18 | 坐标变换公式 |
| `modules/Sens-Decision/src/preprocess.c` | 30-44 | `imu_adapt_to_code_frame()` 实现 |
| `modules/Sens-Decision/src/preprocess.c` | 66-69 | IMU变换调用点 |
| `modules/Sens-Decision/src/config.c` | 27-30 | IR权重注释: 代码坐标系定义 |
| `modules/Sens-Decision/src/config.c` | 31-40 | IR传感器Y坐标 (标注为代码坐标系) |
| `modules/Sens-Decision/src/config.c` | 45-47 | IR权重公式: "右侧(+Y)为正值" |
| `modules/Sens-Decision/src/config.c` | 78-80 | ir_weights 当前实际值 |
| `modules/Sens-Decision/src/config.c` | 86-91 | encoder_directions 编码器方向系数 |
| `modules/Sens-Decision/src/config.c` | 109-110 | encoder_x/encoder_y 编码器位置 |
| `modules/Sens-Decision/src/config.c` | 141 | wheel_track_m 轮距 |
| `modules/Sens-Decision/src/config.c` | 222-223 | encoder->position 赋值 |
| `modules/Sens-Decision/src/config.c` | 286-288 | IMU 安装位置 |
| `modules/Sens-Decision/src/config.c` | 302-305 | IR阵列位置: 代码坐标系注释 |
| `modules/Sens-Decision/src/config.c` | 328-330 | perception.position 实际值 |
| `modules/Sens-Decision/src/EKF.c` | 131-132 | EKF运动方程 (隐式定义代码坐标系) |
| `modules/Sens-Decision/src/state_evaluate.c` | 106-110 | 差速公式 v_encoder/omega_encoder |
| `modules/Sens-Decision/src/state_evaluate.c` | 119-120 | observation[0]=v, observation[1]=omega |
| `modules/Sens-Decision/src/perception.c` | 94 | lateral_error 加权质心计算 |
| `modules/MotionControl/src/motion_kinematics.c` | 31-37 | 逆运动学 v_left/v_right 计算 |
| `modules/MotionControl/src/motion_kinematics.c` | 44-47 | 正运动学 omega 计算 |

---

## 附录B: 文档来源索引

| 文档 | 相关章节 | key 信息 |
|------|----------|----------|
| `README.md` | API调用说明 3.预处理层 | 坐标系说明 (代码/物理) |
| `README.md` | API调用说明 4.感知层 | lateral_error 符号约定 |
| `README.md` | API调用说明 5.5 | IR权重 (版本A) |
| `README.md` | 关键参数配置 6.5 | IR权重 (版本B) - **与5.5矛盾** |
| `API_PITFALLS_GUIDE.md` | 7.坐标系 | 代码帧定义 + IR权重 (版本A) |
| `docs/GEOMETRY_UPDATE_2026-07-30.md` | 4.IR权重数组 | 新权重推导 (版本B) |
| `logs/2026-07-30_coord_system_complete_fix.md` | Fix 3 | 坐标修复历史 (版本A) |
| `logs/2026-07-30_imu_coord_adapter_fix.md` | 全文 | IMU适配器推导过程 |

---

**报告撰写时间**: 2026-07-30
**分析耗时**: 深度代码审查
**下一步**: 实车 lateral_error 符号验证测试 (P0)
