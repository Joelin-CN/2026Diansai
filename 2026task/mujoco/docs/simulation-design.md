# MuJoCo 仿真工程设计文档

> 版本 v1.0 | 2026-07-28 | 对应固件 m0_controller/test @ main

---

## 1. 项目概述与目标

本仿真工程的目标是在 MuJoCo 物理引擎中重现整个差分驱动小车的软件控制流程，  
验证从传感器原始数据到电机 PWM 输出的完整控制链路，无需部署到真实硬件。

### 1.1 仿真必须覆盖的模块

| 模块 | 对应固件文件 | 仿真方式 |
|---|---|---|
| 小车本体（4 电机 差分） | motor.c / motor_adapter.c | MuJoCo 4 轮铰链关节 + velocity actuator |
| 编码器（4×正交，PPR=334） | encoder.c / encoder_adapter.c | MuJoCo jointpos sensor → 角度差分计数 |
| IMU（ICM42688P，SPI） | icm42688_hal + ahrs_hal | MuJoCo framelinvel + frameangvel + 高斯噪声 |
| IR 循迹阵列（8 通道 UART） | ir_uart_sensor.c + sensor_adapter.c | 几何距离法检测黑线，输出 0–4095 raw |
| 黑色轨道 | — | MuJoCo XML box geom（黑色材质）+ Python 轨道数据 |

### 1.2 验证目标

1. **传感器链路**：仿真传感器输出格式与固件期望的数据格式完全一致。  
2. **感知层**：`perception.py` 产生的 `lateral_error` / `heading_error` 与 `perception.c` 逻辑一致。  
3. **行为规划**：小车在直道、弯道、线丢失时行为状态机正确切换。  
4. **运动控制闭环**：FF+PI 控制器能够让小车稳定跟踪速度指令。  
5. **全流程**：小车能完成至少一圈正方形轨道，不丢线。

---

## 2. 整体架构

```
┌──────────────────────────────────────────────────────────────┐
│  sim_main.py  (Python, conda RL)                             │
│                                                              │
│  每 2 ms (500 Hz) 一次仿真步：                                │
│  ┌────────────────────┐  每 10 步 (50 Hz)                    │
│  │ Sens-Decision 桥   │── perception → behavior → trajectory │
│  │  (Python 移植)     │──────────────────────────────────────│
│  └────────────────────┘  每步 (500 Hz)                       │
│           ↓                                                  │
│  ┌─────────────────────┐                                     │
│  │ Motion Control 桥   │── FF+PI → PWM → ω_target (rad/s)   │
│  └─────────────────────┘                                     │
│           ↓                                                  │
│  ┌──────────────────────────────────┐                        │
│  │ MuJoCo 仿真层                    │                        │
│  │  robot.xml + track.xml + scene.xml                        │
│  │  velocity actuators × 4 轮       │                        │
│  │  传感器回读: jointpos / frame*vel │                        │
│  └──────────────────────────────────┘                        │
└──────────────────────────────────────────────────────────────┘
```

**设计原则**：控制层（Python 移植）与仿真层（MuJoCo）完全解耦，  
控制层通过标准接口读传感器、写执行器，与固件使用 `sensor_hal_t` vtable 相同的设计思路。

---

## 3. 物理参数速查表

| 参数 | 数值 | 来源 |
|---|---|---|
| 轴距 (wheel base) | 0.150 m | motion_config.h |
| 车轮半径 | 0.033 m | motion_config.h |
| 车轮周长 | 0.2073 m | 2π×0.033 |
| 编码器 PPR（轮轴处） | 334 counts/rev | motion_config.h |
| 减速比（文档值） | 28:1 | motion_config.h |
| 最大线速度 | 1.0 m/s | motion_config.h |
| 最大角速度 | 6.0 rad/s | motion_config.h |
| 最大加速度 | 2.0 m/s² | motion_config.h |
| 最大减速度 | 3.0 m/s² | motion_config.h |
| PWM 范围 | ±1000 | motor.h |
| 控制频率 | 500 Hz (2 ms) | control_app.c |
| 决策频率 | 50 Hz (20 ms) | control_app.c |
| 速度 PI Kp / Ki | 200 / 50 | motion_config.h |
| PI 输出限幅 | ±500 PWM | motion_config.h SPEED_OUTPUT_MAX |
| FF k_accel / k_friction / k_static | 50 / 300 / 80 | motion_config.h |
| 最小速度吸附 | 0.10 m/s | motion_config.h MIN_SPEED |
| 指令平滑时间常数 | 0.05 s | motion_config.h CMD_SMOOTH_TAU |
| IR 通道数 | 8 | ir_uart_sensor.h |
| IR 权重 | {-7,-5,-3,-1,1,3,5,7} | config.h（ch0=最左） |
| IR 上线阈值（反射率） | 0.5 | sensor_adapter.c |
| IMU 型号 | ICM42688P | icm42688_hal.h |

---

## 4. 机器人 XML 模型（robot.xml）

### 4.1 底盘结构

```xml
<!-- 估算尺寸：宽 180 mm × 长 200 mm × 高 40 mm，总质量约 500 g -->
<!-- chassis 初始高度：z=0.033（车轮半径），使车轮底面恰好贴地；z=0.053写法已废弃 -->
<body name="chassis" pos="0 0 0.053">
  <!-- ⚠️ 必须有 freejoint，否则底盘固定，机器人无法运动 -->
  <freejoint name="root"/>
  <geom name="chassis_box" type="box" size="0.100 0.090 0.020"
        mass="0.380" rgba="0.3 0.3 0.3 1"/>
  <inertial pos="0 0 0" mass="0.380"
            diaginertia="8e-4 1.2e-3 1.5e-3"/>
  <!-- IR 传感器挂载点：8 个 site，沿 y 轴等距排列，前端 x=+0.105 -->
  <!-- 间距 14 mm，覆盖范围 ±49 mm -->
  <!-- z=-0.020 来自固件 perception config.position.z_m=-0.02f（底盘中心向下 20 mm） -->
  <site name="ir0" pos="0.105 -0.049 -0.020" size="0.003"/>
  <site name="ir1" pos="0.105 -0.035 -0.020" size="0.003"/>
  <site name="ir2" pos="0.105 -0.021 -0.020" size="0.003"/>
  <site name="ir3" pos="0.105 -0.007 -0.020" size="0.003"/>
  <site name="ir4" pos="0.105  0.007 -0.020" size="0.003"/>
  <site name="ir5" pos="0.105  0.021 -0.020" size="0.003"/>
  <site name="ir6" pos="0.105  0.035 -0.020" size="0.003"/>
  <site name="ir7" pos="0.105  0.049 -0.020" size="0.003"/>
  <!-- IMU 挂载点：底盘中心 -->
  <site name="imu_site" pos="0 0 0" size="0.005"/>
</body>
```

### 4.2 车轮关节与执行器

4 个车轮，铰链关节绕 Y 轴旋转（车轮轴方向）。  
左侧：wheel_fl（左前）、wheel_rl（左后）；右侧：wheel_fr（右前）、wheel_rr（右后）。

```xml
<!-- 以左前轮为例，其他三轮对称 -->
<!-- ⚠️ y=0.100（不是 0.090）：底盘 geom 半宽=0.090，轮需在其外侧 -->
<!-- ⚠️ z=-0.020：底盘中心 z=0.053，车轮中心需在 z=0.033（=车轮半径），故 -0.020 -->
<body name="wheel_fl" pos="0.075 0.100 -0.020">
  <joint name="jwheel_fl" type="hinge" axis="0 1 0" damping="0.01"/>
  <geom name="wheel_fl" type="cylinder" size="0.033 0.010"
        euler="90 0 0" mass="0.030" friction="1.0 0.005 0.0001"
        rgba="0.1 0.1 0.1 1"/>
</body>
```

**执行器**：velocity actuator，直接设定目标角速度（rad/s）。  
PWM → ω_target 转换：`ω = pwm / 1000.0 × (V_MAX / R_WHEEL)`，  
其中 `V_MAX = 1.0 m/s`，`R_WHEEL = 0.033 m` → 满PWM 对应 ω_max ≈ 30.3 rad/s。

```xml
<actuator>
  <!-- ⚠️ kv=50（非 kv=10）：kv 太小时轮速跟踪极慢，控制器调试困难 -->
  <!-- kv≈50 使速度误差在 1–2 步内收敛，与 500 Hz 控制周期匹配 -->
  <velocity name="act_fl" joint="jwheel_fl" kv="50" gear="1"/>
  <velocity name="act_fr" joint="jwheel_fr" kv="50" gear="1"/>
  <velocity name="act_rl" joint="jwheel_rl" kv="50" gear="1"/>
  <velocity name="act_rr" joint="jwheel_rr" kv="50" gear="1"/>
</actuator>
```

> **设计决策**：选用 velocity actuator 而非 torque actuator。  
> 目的是验证上层控制逻辑（感知→决策→运动指令），而非验证电机物理模型。  
> FF+PI 速度控制器仍然在 Python 桥接层运行，其输出 PWM 经转换后写入 actuator。

### 4.3 传感器定义

```xml
<sensor>
  <!-- 编码器：关节角度位置，Python 侧取差分→计数 -->
  <jointpos name="enc_fl" joint="jwheel_fl"/>
  <jointpos name="enc_fr" joint="jwheel_fr"/>
  <jointpos name="enc_rl" joint="jwheel_rl"/>
  <jointpos name="enc_rr" joint="jwheel_rr"/>
  <!-- IMU：底盘坐标系线速度与角速度 -->
  <framelinvel  name="imu_linvel"  objtype="site" objname="imu_site"/>
  <frameangvel  name="imu_angvel"  objtype="site" objname="imu_site"/>
  <!-- 底盘位姿（调试用，不进入控制闭环） -->
  <framepos     name="chassis_pos" objtype="body" objname="chassis"/>
  <framequat    name="chassis_quat" objtype="body" objname="chassis"/>
</sensor>
```

---

## 5. 轨道设计（track.xml）

### 5.1 规格

| 参数 | 数值 |
|---|---|
| 轨道形状 | 正方形（与 square_path.h 对应） |
| 内边长 | 1.2 m |
| 线宽 | 25 mm（标准黑色胶带） |
| 弯道半径 | 100 mm（圆角方形） |
| 地面颜色 | 白色 rgba(0.95, 0.95, 0.95, 1) |
| 轨道颜色 | 黑色 rgba(0.05, 0.05, 0.05, 1) |

### 5.2 轨道 XML 结构

轨道由 4 段直线 + 4 个角落弧线组成，每段用细扁 box geom 表示：

```xml
<!-- 地面：2 m × 2 m 白色平面 -->
<geom name="floor" type="plane" size="2 2 0.1"
      rgba="0.95 0.95 0.95 1" friction="1.0 0.005 0.0001"/>

<!-- 正方形轨道：4 段直线，中心线半径 = (1.2/2 + 0.0125) = 0.6125 m -->
<!-- 直线段：长 1.2 m，宽 0.025 m，厚 0.001 m -->
<geom name="track_top"    type="box" pos="0  0.6125 0.0005"
      size="0.625 0.0125 0.0005" rgba="0.05 0.05 0.05 1"/>
<geom name="track_bottom" type="box" pos="0 -0.6125 0.0005"
      size="0.625 0.0125 0.0005" rgba="0.05 0.05 0.05 1"/>
<geom name="track_left"   type="box" pos="-0.6125 0 0.0005"
      euler="0 0 90" size="0.625 0.0125 0.0005" rgba="0.05 0.05 0.05 1"/>
<geom name="track_right"  type="box" pos=" 0.6125 0 0.0005"
      euler="0 0 90" size="0.625 0.0125 0.0005" rgba="0.05 0.05 0.05 1"/>
<!-- 弯道：用多段短 box 近似圆弧（每段 15°，共 6 段/弯角），在 Python 脚本中生成 XML -->
```

### 5.3 Python 轨道数据（用于 IR 仿真）

**⚠️ 不使用路点列表**。轨道用 8 个几何原语表示（4 段直线 + 4 段圆弧），计算解析垂直距离：

```python
# track_generator.py 输出的轨道段数据结构
# 轨道中心线：内边长 1.2 m，角落圆弧 R=0.100 m，中心线偏移 = 0.6125 m

# 直线段端点（中心线坐标）
CL = 0.6125  # centre-line offset from origin (m)
CR = 0.100   # corner arc radius (m)
ST = CL - CR  # straight-segment end x/y = 0.5125 m

TRACK_SEGMENTS = [
    # --- 4 直线段 ---
    {'type': 'line',  'p1': np.array([-ST,  CL]), 'p2': np.array([ ST,  CL])},  # 上
    {'type': 'line',  'p1': np.array([ CL,  ST]), 'p2': np.array([ CL, -ST])},  # 右
    {'type': 'line',  'p1': np.array([ ST, -CL]), 'p2': np.array([-ST, -CL])},  # 下
    {'type': 'line',  'p1': np.array([-CL, -ST]), 'p2': np.array([-CL,  ST])},  # 左
    # --- 4 圆弧段（圆心在内角，R=0.100，各跨 90°）---
    {'type': 'arc', 'center': np.array([ ST,  ST]), 'radius': CR,
     'a_start': 0,         'a_end': np.pi/2},     # 右上角
    {'type': 'arc', 'center': np.array([ ST, -ST]), 'radius': CR,
     'a_start': -np.pi/2,  'a_end': 0},           # 右下角
    {'type': 'arc', 'center': np.array([-ST, -ST]), 'radius': CR,
     'a_start': np.pi,     'a_end': 3*np.pi/2},   # 左下角
    {'type': 'arc', 'center': np.array([-ST,  ST]), 'radius': CR,
     'a_start': np.pi/2,   'a_end': np.pi},       # 左上角
]

def min_dist_to_track(p):
    """返回点 p（2D）到轨道中心线的最短垂直距离（m）。精确解析解，无离散误差。"""
    min_d = np.inf
    for seg in TRACK_SEGMENTS:
        if seg['type'] == 'line':
            ab = seg['p2'] - seg['p1']
            t  = np.clip(np.dot(p - seg['p1'], ab) / (np.dot(ab, ab) + 1e-12), 0, 1)
            min_d = min(min_d, np.linalg.norm(p - (seg['p1'] + t * ab)))
        else:  # arc
            d  = p - seg['center']
            angle = np.arctan2(d[1], d[0])
            # 判断投影角是否在弧内
            a0, a1 = seg['a_start'], seg['a_end']
            # 归一化到 [a0, a0+2π)
            angle_norm = a0 + ((angle - a0) % (2 * np.pi))
            if angle_norm <= a1 + 1e-9:
                min_d = min(min_d, abs(np.linalg.norm(d) - seg['radius']))
            else:
                # 超出弧端点，取两端点距离最小值
                ep0 = seg['center'] + seg['radius'] * np.array([np.cos(a0), np.sin(a0)])
                ep1 = seg['center'] + seg['radius'] * np.array([np.cos(a1), np.sin(a1)])
                min_d = min(min_d, np.linalg.norm(p - ep0), np.linalg.norm(p - ep1))
    return min_d
```

> 几何原语方案的优势：精确解析距离（无5mm离散误差），弯道准确，代码量更少。

---

## 6. 传感器仿真

### 6.1 IR 循迹阵列（8 通道）

**固件数据流**：UART 帧 → `IrUartSensor_Process()` → `g_analog[8]`（uint16, 0–4095） →  
`ReadIr()` → `reflectance = 1.0 - raw/4096`，`reflectance > 0.5` 时判定在线（即 raw < 2048）。

**仿真响应模型**：Sigmoid 连续曲线，物理上对应传感器光斑扩散的渐变响应。

```
raw = 4095 / (1 + exp(-K * (dist - half_width)))

dist=0mm(线中心)  → raw ≈  94  → reflectance≈0.98 → 在线 ✓
dist=12.5mm(边缘) → raw = 2048 → reflectance=0.50 → 恰在阈值 ✓
dist=20mm         → raw ≈ 3710 → reflectance≈0.09 → 白区 ✓
```

K=300 时过渡区宽约 ±10 mm（光斑等效半径），与实物 IR 模块特性吻合。

**仿真实现**（`src/ir_sensor.py`）：

```python
HALF_WIDTH   = 0.0125   # m，25mm 黑胶带半宽
SIGMOID_K    = 300.0    # 曲线斜率，等效光斑半径≈10mm
NOISE_SIGMA  = 30       # ADC 电气噪声（LSB，仿真可适当减小）

def _sigmoid_raw(dist: float) -> float:
    """距离中心线 dist (m) 处的 IR raw 期望值（连续，含光学扩散）。"""
    x = SIGMOID_K * (dist - HALF_WIDTH)
    frac_white = 1.0 / (1.0 + np.exp(-np.clip(x, -30, 30)))  # 防止溢出
    return 4095.0 * frac_white

def read_ir_raw(model, data) -> np.ndarray:
    """
    返回 shape=(8,) 的 uint16 数组，模拟固件 g_analog[8]。
    依赖全局 TRACK_SEGMENTS（由 track_generator.py 定义）。
    """
    raw = np.empty(8, dtype=np.uint16)
    for i, sid in enumerate(IR_SITE_IDS):          # 预计算好的 site ID 列表
        world_xy = data.site_xpos[sid, :2]          # (x, y) 世界坐标
        dist     = min_dist_to_track(world_xy)       # §5.3 解析距离函数
        raw[i]   = int(np.clip(
            _sigmoid_raw(dist) + np.random.normal(0, NOISE_SIGMA),
            0, 4095
        ))
    return raw
```

**关键特性对比**：

| 指标 | 旧方案（路点+分段线性） | 新方案（几何段+Sigmoid） |
|---|---|---|
| 轨道距离精度 | ±5mm（离散误差），弯道更差 | 解析精确，弯道与直道一致 |
| 边界连续性 | 不连续跳变（Δraw=300） | C¹ 连续，过渡平滑 |
| weighted centroid 品质 | 边缘传感器值偏低 | 随位置线性渐变，centroid 更准 |
| 代码依赖 | TRACK_WAYPOINTS 大列表 | TRACK_SEGMENTS 8 条记录 |

### 6.2 编码器

**固件期望**：`g_encoderCount[4]`（int32，累积计数，PPR=334/rev）。

**仿真实现**（`src/encoder_sim.py`）：

```python
class EncoderSim:
    """将 MuJoCo jointpos（弧度，无界）转换为固件格式的编码器计数"""
    PPR = 334  # counts/rev at wheel shaft

    def __init__(self):
        self._prev_angle = np.zeros(4)   # rad
        self._counts     = np.zeros(4, dtype=np.int32)

    def update(self, data):
        sensor_ids = [enc_fl_id, enc_fr_id, enc_rl_id, enc_rr_id]
        angles = np.array([data.sensordata[i] for i in sensor_ids])
        delta_rad = angles - self._prev_angle
        self._counts += np.round(delta_rad / (2 * np.pi) * self.PPR).astype(np.int32)
        self._prev_angle = angles.copy()
        return self._counts.copy()
```

逻辑 ID 到 MuJoCo 关节的映射（`EncoderId_t` 定义于 `motion_feedback.h`，非 encoder_adapter.h）：
- `ENCODER_LEFT_FRONT  (0)` → `jwheel_fl`
- `ENCODER_LEFT_REAR   (1)` → `jwheel_rl`
- `ENCODER_RIGHT_FRONT (2)` → `jwheel_fr`
- `ENCODER_RIGHT_REAR  (3)` → `jwheel_rr`

> ⚠️ 旧文档用 M1/M2/M3/M4 命名（硬件层 encoder.h），此处统一改为 Sens-Decision 层的逻辑名。

### 6.3 IMU（ICM42688P）

**固件期望**：`imu_raw_data_t`，含 `int16 accel[3]`（LSB，±16g 量程）和 `int16 gyro[3]`（LSB，±2000 dps）。

**仿真实现**（`src/imu_sim.py`）：

```python
# ⚠️ MuJoCo framelinvel / frameangvel 均为世界坐标系（global frame）
# 真实 ICM42688P 输出机体坐标系数据，Sens-Decision EKF 期望机体系输入
# 需要用底盘旋转矩阵 R = data.xmat[chassis_id].reshape(3,3) 做 world → body 变换：
#   linvel_body = R.T @ linvel_world
#   angvel_body = R.T @ angvel_world
ACCEL_SCALE = 2048   # LSB/g （±16g，16-bit）
GYRO_SCALE  = 16.4   # LSB/dps（±2000 dps，16-bit）

def read_imu_raw(model, data, chassis_id, prev_linvel, dt):
    R = data.xmat[chassis_id].reshape(3, 3)  # 世界→机体旋转矩阵（转置得 body←world）

    linvel_world = data.sensordata[imu_linvel_ids]   # m/s，世界系
    angvel_world = data.sensordata[imu_angvel_ids]   # rad/s，世界系

    linvel_body  = R.T @ linvel_world
    angvel_body  = R.T @ angvel_world

    # 加速度 = 线速度导数（体系）；step=0 时 prev=zeros，第一帧有冲击，可忽略
    accel_mps2 = (linvel_body - prev_linvel) / dt
    accel_g    = accel_mps2 / 9.80665

    accel_raw  = np.round(accel_g * ACCEL_SCALE).astype(np.int16)
    gyro_dps   = np.degrees(angvel_body)
    gyro_raw   = np.round(gyro_dps * GYRO_SCALE).astype(np.int16)

    # 高斯噪声
    accel_raw += np.random.normal(0, 3, 3).astype(np.int16)
    gyro_raw  += np.random.normal(0, 2, 3).astype(np.int16)
    return accel_raw, gyro_raw, linvel_body.copy()
```

---

## 7. 控制桥接层（固件 Python 移植）

所有控制逻辑放在 `firmware_bridge/` 目录，参数严格与 `motion_config.h` 保持一致。

### 7.1 感知层（perception.py）

完全对应 `modules/Sens-Decision/src/perception.c`：

```python
WEIGHTS   = [-7.0, -5.0, -3.0, -1.0, 1.0, 3.0, 5.0, 7.0]  # 与 config.c sd_config_reset_defaults() 一致
IR_THRESHOLD      = 0.5
HEADING_ALPHA     = 0.3   # config.c: heading_filter_alpha = 0.3f
CURVE_ERR_THRESH  = 0.45  # config.c: curve_error_threshold = 0.45f
CURVE_DERIV_THRESH= 1.5   # config.c: curve_derivative_threshold = 1.5f
INTERSECT_CH      = 4     # config.c: intersection_active_channels = 4U

def perception_update(ir_raw, dt_s, state):
    reflectance = 1.0 - ir_raw / 4096.0
    reflectance = np.clip(reflectance, 0.0, 1.0)

    active_mask   = (reflectance > IR_THRESHOLD).astype(np.uint8)
    active_count  = int(active_mask.sum())

    weighted_sum  = float(np.sum(np.array(WEIGHTS) * reflectance))
    max_abs_w     = 7.0   # max(|weights|) = 7
    lateral_error = weighted_sum / max_abs_w

    derivative = (lateral_error - state.prev_lateral) / dt_s
    # ⚠️ 第一次调用时直接赋值（无IIR），与 perception.c lines 85-90 一致
    if not state.initialized:
        state.heading_error = derivative
        state.initialized   = True
    else:
        alpha = HEADING_ALPHA
        state.heading_error = alpha * state.heading_error + (1.0 - alpha) * derivative
    state.prev_lateral = lateral_error

    line_valid       = active_count > 0
    state.lost_count = 0 if line_valid else state.lost_count + 1

    event = classify_event(lateral_error, state.heading_error, active_count)
    return lateral_error, state.heading_error, active_mask, line_valid, event


def classify_event(lateral_error, heading_error, active_count):
    if active_count == 0:
        return ROAD_EVENT_LINE_LOST
    if active_count >= INTERSECT_CH:
        return ROAD_EVENT_INTERSECTION
    if abs(lateral_error) >= CURVE_ERR_THRESH and abs(heading_error) >= CURVE_DERIV_THRESH:
        return ROAD_EVENT_CURVE_ENTRY
    return ROAD_EVENT_NONE
```

### 7.2 行为规划器（behavior_planner.py）

对应 `modules/Sens-Decision/src/behavior_planner.c` 的 FSM：

状态：`IDLE → LINE_FOLLOW → APPROACH_CURVE → CURVE → LINE_LOST_DEGRADED → STOPPED → FAULT`

输出：`(behavior_state, speed_limit)`

### 7.3 轨迹生成（trajectory_generator.py）

对应 `modules/Sens-Decision/src/trajectory_generator.c`：
- 输入：`behavior_state`、`lateral_error`、`heading_error`
- 输出：`v_cmd (m/s)`、`omega_cmd (rad/s)`（平滑后指令，已含 jerk limiting）
- 核心：**Pure Pursuit**，lookahead=0.25m，`κ = 2 × lateral_error / L²`，`ω = v × κ`

```python
LOOKAHEAD   = 0.25   # m，前瞻距离（motion_config.h LOOKAHEAD_DISTANCE）
MAX_JERK    = 5.0    # m/s³（motion_config.h）
# CMD_SMOOTH_ALPHA = 1 - exp(-DT / CMD_SMOOTH_TAU)，TAU=0.05s，DT=0.02s → α≈0.3297
CMD_SMOOTH_ALPHA = 0.3297

def trajectory_generator_update(behavior_state, speed_limit,
                                 lateral_error, heading_error,
                                 dt_s, state):
    # Pure Pursuit：将横向误差映射为曲率
    kappa  = 2.0 * lateral_error / (LOOKAHEAD ** 2)
    v_raw  = min(speed_limit, state.v_prev + MAX_JERK * dt_s)  # jerk limit（加速段）
    v_raw  = max(v_raw, state.v_prev - MAX_JERK * 1.5 * dt_s)  # jerk limit（减速段更快）
    omega_raw = v_raw * kappa

    # 一阶低通平滑（CMD_SMOOTH_TAU=0.05s）
    v_cmd     = CMD_SMOOTH_ALPHA * v_raw     + (1.0 - CMD_SMOOTH_ALPHA) * state.v_smooth
    omega_cmd = CMD_SMOOTH_ALPHA * omega_raw + (1.0 - CMD_SMOOTH_ALPHA) * state.omega_smooth
    state.v_smooth     = v_cmd
    state.omega_smooth = omega_cmd
    state.v_prev       = v_cmd

    # LINE_LOST 时强制停车
    if behavior_state == BEHAVIOR_LINE_LOST_DEGRADED:
        v_cmd = omega_cmd = 0.0

    return v_cmd, omega_cmd
```

### 7.4 运动控制（motion_control.py）

完全对应 `modules/Motion Control/`，所有参数来自 `motion_config.h`：

```python
WHEEL_BASE      = 0.150    # m
WHEEL_R         = 0.033    # m
SPEED_KP        = 200.0
SPEED_KI        = 50.0
FF_KACCEL       = 50.0
FF_KFRICT       = 300.0
FF_KSTATIC      = 80.0
V_MAX           = 1.0      # m/s
PWM_MAX         = 1000
SPEED_OUT_MAX   = 500      # PI 输出限幅（motion_config.h SPEED_OUTPUT_MAX）
INTEGRAL_MAX    = 10.0     # 积分限幅（motion_config.h）
MIN_SPEED       = 0.10     # m/s 速度吸附死区（motion_config.h MIN_SPEED）
                           # |v_target| < MIN_SPEED 时清零积分，FF 强制为 0

def motion_control_update(v_cmd, omega_cmd, v_left_actual, v_right_actual, dt, state):
    # 逆运动学
    v_left_target  = v_cmd - (WHEEL_BASE / 2) * omega_cmd
    v_right_target = v_cmd + (WHEEL_BASE / 2) * omega_cmd

    for side, v_target, v_actual, ctrl in [
        ('left',  v_left_target,  v_left_actual,  state.left_ctrl),
        ('right', v_right_target, v_right_actual, state.right_ctrl),
    ]:
        # ⚠️ 最小速度吸附：低速时避免小 PWM 导致电机抖振
        if abs(v_target) < MIN_SPEED:
            ctrl.integrator = 0.0
            data.ctrl[...] = 0.0   # 直接停轮，清积分
            ctrl.prev_target = 0.0
            yield side, 0, 0.0
            continue

        # Feedforward
        accel = (v_target - ctrl.prev_target) / dt
        ff    = FF_KACCEL * accel + FF_KFRICT * v_target + FF_KSTATIC * np.sign(v_target)

        # PI feedback
        error           = v_target - v_actual
        ctrl.integrator = np.clip(ctrl.integrator + error * dt, -INTEGRAL_MAX, INTEGRAL_MAX)
        fb              = np.clip(SPEED_KP * error + SPEED_KI * ctrl.integrator,
                                  -SPEED_OUT_MAX, SPEED_OUT_MAX)

        pwm             = int(np.clip(ff + fb, -PWM_MAX, PWM_MAX))
        ctrl.prev_target = v_target

        # PWM → 目标角速度（rad/s），写入 velocity actuator
        omega_wheel = pwm / PWM_MAX * (V_MAX / WHEEL_R)
        yield side, pwm, omega_wheel
```

> **指令平滑**（`CMD_SMOOTH_TAU=0.05s`）已在 §7.3 的 trajectory_generator 层执行；  
> motion_control 层收到的 `v_cmd/omega_cmd` 已经平滑，无需再做低通。这与固件一致：  
> `cmd_smooth` 在 `trajectory_generator.c` 内部完成，`motion_control.c` 直接使用平滑后值。

---

## 8. 仿真主循环（sim_main.py）

```python
DT_SIM      = 0.002   # 2 ms，与固件 500 Hz 对齐
STEPS_TOTAL = 30000   # 60 s 仿真时长
SENS_PERIOD = 10      # 每 10 步执行一次 50 Hz 决策

model = mujoco.MjModel.from_xml_path("models/scene.xml")
data  = mujoco.MjData(model)
model.opt.timestep = DT_SIM

enc_sim   = EncoderSim()
imu_sim   = ImuSim()
ctrl      = ControlBridge()      # 封装上述 7.1-7.4
v_cmd = omega_cmd = 0.0

for step in range(STEPS_TOTAL):
    # ── 读传感器 ──
    enc_counts  = enc_sim.update(data)           # int32[4]
    ir_raw      = read_ir_raw(model, data)                       # uint16[8]，TRACK_SEGMENTS 是全局量
    acc_raw, gyro_raw = imu_sim.read(data)       # int16[3] each

    # ── 50 Hz 决策 ──
    if step % SENS_PERIOD == 0:
        v_cmd, omega_cmd = ctrl.sens_decision_update(
            ir_raw, enc_counts, acc_raw, gyro_raw, DT_SIM * SENS_PERIOD)

    # ── 500 Hz 运动控制 ──
    v_left_actual  = enc_sim.wheel_speed('left')   # m/s
    v_right_actual = enc_sim.wheel_speed('right')
    pwm_l, pwm_r, omega_l, omega_r = ctrl.motion_control_update(
        v_cmd, omega_cmd, v_left_actual, v_right_actual, DT_SIM)

    # ── 写执行器（velocity actuator，rad/s）──
    data.ctrl[ACT_FL] = omega_l   # 左前
    data.ctrl[ACT_RL] = omega_l   # 左后（同侧同速）
    data.ctrl[ACT_FR] = omega_r
    data.ctrl[ACT_RR] = omega_r

    mujoco.mj_step(model, data)

    # ── 日志（每 500 步 / 1 s 打印一次）──
    if step % 500 == 0:
        log_state(step, data, enc_counts, ir_raw, v_cmd, omega_cmd)
```

---

## 9. 文件结构

```
mujoco/
├── AGENTS.md                  # 环境说明（已存在）
├── README.md                  # 项目介绍（已存在，待填充）
├── docs/
│   └── simulation-design.md   # 本文档
├── models/
│   ├── robot.xml              # 机器人几何 + 传感器 + 执行器
│   ├── track.xml              # 轨道几何（含 Python 生成的弯道段）
│   └── scene.xml              # 顶层 include：robot + track + 物理参数
├── src/
│   ├── sim_main.py            # 主仿真循环
│   ├── ir_sensor.py           # IR 传感器仿真（几何距离法）
│   ├── encoder_sim.py         # 编码器仿真
│   ├── imu_sim.py             # IMU 仿真（加噪声）
│   ├── track_generator.py     # 生成 track.xml 弯道段 + TRACK_WAYPOINTS
│   └── visualizer.py          # MuJoCo viewer 封装（可选）
├── firmware_bridge/
│   ├── __init__.py
│   ├── perception.py          # perception.c 移植
│   ├── behavior_planner.py    # behavior_planner.c 移植
│   ├── trajectory_generator.py
│   ├── motion_control.py      # motion_control 移植（含 FF+PI）
│   ├── state_estimator.py     # 编码器里程计（简化 EKF）
│   └── control_bridge.py      # 统一入口，组合以上模块
├── tests/
│   ├── test_perception.py     # 单元测试：感知层与参考 C 输出对比
│   ├── test_motion_control.py # 单元测试：FF+PI 控制器阶跃响应
│   └── test_sim_smoke.py      # 冒烟测试：仿真启动，机器人前进 1 m 不崩溃
└── logs/                      # 仿真运行日志（.csv / .txt，gitignore）
```

---

## 10. 环境配置

```bash
# 激活 MuJoCo 专用环境
conda activate RL

# 验证 mujoco 可用
python -c "import mujoco; print(mujoco.__version__)"

# 运行仿真
cd "E:\B306\2026\电赛\2026 task\mujoco"
python src/sim_main.py

# 运行单元测试
python -m pytest tests/ -v
```

**依赖**（conda RL 环境应已包含）：
- `mujoco >= 3.1`
- `numpy`
- `matplotlib`（可选，用于轨迹绘图）

---

## 11. 验证测试场景

| 场景 | 初始位置 | 验证指标 | 通过条件 |
|---|---|---|---|
| S1 直道跟踪 | 正方形上边中点，朝 +X | lateral_error 稳态 | \|error\| < 0.15（归一化） |
| S2 直角弯道 | 距弯角 30 cm | 成功转弯不丢线 | 弯道后 2 s 内重新入线 |
| S3 丢线恢复 | 线上，手动关 IR | LINE_LOST 状态 → 停车 | 3 s 内进入 STOPPED |
| S4 全圈完成 | 正方形任意起点 | 完成 1 圈 | 圈计数 += 1，无急停 |
| S5 速度控制 | 直道 | 实际轮速 vs 目标 | 稳态误差 < 5% |

---

## 12. 关键设计决策记录

| 决策 | 选择 | 理由 |
|---|---|---|
| 执行器类型 | velocity actuator | 验证目标是上层控制逻辑，非电机物理；velocity actuator 隔离了电机动力学 |
| IR 检测方式 | Python 几何距离法 | 无需 MuJoCo raycasting，稳定可控；轨道几何在 XML 和 Python 侧双重维护 |
| 控制代码 | Python 移植（非 ctypes 调 C） | 仿真可独立运行，无需交叉编译；参数统一来自 motion_config.h 的副本 |
| 仿真步长 | 2 ms（与固件 500 Hz 对齐） | 控制循环与仿真时间完全对齐，无插值误差 |
| 轨道设计 | 正方形，内边长 1.2 m | 与 square_path.h 场景对应；空间合理，弯道可测 |
| 左右轮 actuator | FL+RL 共享同一 ω，FR+RR 共享同一 ω | 与固件 Motor_SetSpeed(left, right) 语义一致 |


