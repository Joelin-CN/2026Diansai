# MuJoCo 差分驱动循迹小车仿真

基于 MuJoCo 物理引擎的完整控制链路仿真，用于验证固件控制算法。

## 项目结构

```
mujoco/
├── models/              # MuJoCo XML 模型文件
│   ├── scene.xml       # 主场景（包含机器人和赛道）
│   ├── robot.xml       # 机器人模型（4轮差分驱动）
│   └── track.xml       # 黑线赛道
│
├── src/                # 仿真核心模块
│   ├── sim_main.py     # 主仿真循环（500Hz控制）
│   ├── sim_visualize.py # 可视化仿真（带viewer）
│   ├── track_generator.py # 轨道生成器
│   ├── encoder_sim.py  # 编码器仿真
│   ├── imu_sim.py      # IMU仿真
│   └── ir_sensor.py    # IR传感器仿真
│
├── firmware_bridge/    # 固件Python移植
│   ├── perception.py   # 感知模块（加权质心）
│   ├── behavior_planner.py # 行为规划FSM
│   ├── trajectory_generator.py # 轨迹生成（Pure Pursuit）
│   ├── motion_control.py # 运动控制（FF+PI）
│   └── control_bridge.py # 统一控制接口
│
├── tests/              # 单元测试和集成测试
│   ├── test_perception.py # 感知模块测试（6个）
│   ├── test_motion_control.py # 运动控制测试（6个）
│   ├── test_sim_smoke.py # 冒烟测试
│   ├── test_stability.py # 稳定性测试
│   └── test_motion_nogui.py # 运动测试（生成轨迹图）
│
├── docs/               # 文档
│   ├── simulation-design.md # 完整设计文档
│   ├── visualization-report.md # 可视化测试报告
│   └── handoff/        # 交接文档
│
└── logs/               # 仿真输出日志和图表
    └── trajectory_test.png # 轨迹测试结果
```

## 快速开始

### 环境要求

```bash
conda activate RL  # Python 3.11 + MuJoCo 3.11.0
```

### 运行仿真

```bash
cd "E:\B306\2026\电赛\2026 task\mujoco"

# 运行主仿真（60秒，无GUI）
python src/sim_main.py

# 运行可视化仿真（需要图形界面）
python src/sim_visualize.py

# 运行单元测试
python -m pytest tests/test_perception.py -v
python -m pytest tests/test_motion_control.py -v

# 生成轨迹图测试
python tests/test_motion_nogui.py
```

## 核心参数

**机器人物理参数：**
- 轴距：0.150 m
- 轮半径：0.033 m
- 编码器分辨率：334 counts/rev
- 最大速度：1.0 m/s
- 最大角速度：6.0 rad/s

**控制参数：**
- 控制频率：500 Hz（2ms周期）
- 决策频率：50 Hz（20ms周期）
- 速度PI：KP=200, KI=50
- 前馈：k_accel=50, k_friction=300, k_static=80
- Pure Pursuit前瞻距离：0.25 m

**传感器：**
- IR阵列：8通道，权重[-7,-5,-3,-1,1,3,5,7]
- IMU：ICM42688P（±16g, ±2000dps）
- 编码器：4通道正交编码

## 测试状态

| 测试项 | 状态 | 说明 |
|--------|------|------|
| 模型加载 | ✅ | 无XML错误 |
| 感知模块 | ✅ | 6/6单元测试通过 |
| 运动控制 | ✅ | 6/6单元测试通过 |
| 物理仿真 | ✅ | 稳定运行60秒 |
| 执行器驱动 | ✅ | 机器人成功移动 |
| 完整循迹 | ⚠️ | 速度标定中 |

## 已知问题

1. **速度增益过大**：当前速度是预期的50倍，需要调整actuator参数
2. **初始瞬态警告**：t<0.03s有数值警告，但随后稳定（正常现象）

## 设计文档

详细技术规格见：`docs/simulation-design.md`

包含：
- 完整架构设计
- 坐标系变换说明
- 传感器仿真模型
- 控制算法移植细节
- 参数速查表

## 开发者

基于固件仓库 `m0_controller/test` 的控制算法移植而来。

所有参数严格对应固件中的 `motion_config.h` 和 `config.c`。
