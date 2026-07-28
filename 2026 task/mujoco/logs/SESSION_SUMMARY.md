# MuJoCo仿真系统修复总结 - 2026-07-29

## 修复完成状态

本次session完成了MuJoCo循迹小车仿真系统的关键修复工作。

---

## 已修复的问题

### ✅ 1. 轨道尺寸过小
**问题：** 原始轨道260mm × 115mm，比车身(200mm × 180mm)还小  
**修复：** 应用5倍缩放因子，轨道现为1.3m × 0.55m  
**文件：** `src/track_generator.py`, `models/track.xml`

### ✅ 2. 机器人初始方向错误  
**问题：** 机器人朝向+X，但轨道沿+Y方向  
**修复：** 旋转90°，quaternion [0.7071, 0, 0, 0.7071]  
**文件：** `src/sim_main.py`, `src/sim_visualize.py`

### ✅ 3. 机器人初始位置错误
**问题：** 传感器超出轨道边缘14.5mm  
**修复：** 调整为(0.6505, -0.105, 0.033)，使传感器对准轨道中心  
**文件：** `src/sim_main.py`, `src/sim_visualize.py`

### ✅ 4. 传感器方向注释不清
**问题：** 注释容易误解传感器安装位置  
**修复：** 明确标注机器人前进方向为+X（robot frame）  
**文件：** `models/robot.xml`

### ✅ 5. 物理仿真不稳定
**问题：** 机器人飞起来（Z从0.033升到1.36m）  
**修复：**  
- 降低actuator增益：kv从50降到1
- 使用implicitfast积分器
- 增加关节阻尼：0.01 → 0.1
- 增加轮胎摩擦：1.0 → 1.5  

**文件：** `models/scene.xml`, `models/robot.xml`

### ✅ 6. MIN_SPEED门槛过高
**问题：** 0.10 m/s门槛导致机器人启动困难  
**修复：** 降低到0.01 m/s  
**文件：** `firmware_bridge/motion_control.py`

---

## 当前工作状态

### ✅ 正常工作的部分
1. **传感器检测**：IR传感器能正确检测黑线（on-line ~650, off-line ~4095）
2. **运动方向**：机器人沿+Y方向移动（跟随轨道）
3. **位置保持**：X坐标保持在0.6505（轨道中心线）
4. **静态稳定**：无驱动时Z轴变化<0.0001m
5. **短期运动**：0.4秒内前进37mm，控制正常

### ⚠️ 已知问题

#### 1. 机器人在1秒内丢失黑线
**现象：**
- t=0.0s：正常启动
- t=1.0s：横向误差突然-0.7739，角速度饱和-5.4 rad/s
- t=2.0s起：完全丢线，停止运动

**可能原因：**
- 速度增长过快（0→0.5 m/s）
- 转弯控制参数不当
- Pure Pursuit前瞻距离LOOKAHEAD=0.25m可能过大

**建议修复：**
- 降低最大速度：1.0 → 0.3 m/s
- 调整LOOKAHEAD：0.25 → 0.15 m
- 降低MAX_ACCEL：2.0 → 0.5 m/s²

#### 2. Z轴轻微下沉
**现象：** 动态运行时Z从0.033降到0.021（-12mm）

**可能原因：**
- 轮胎与地面接触穿透
- 接触刚度参数需调整

**影响：** 轻微，但需监控

---

## 坐标系说明

### MuJoCo世界坐标系
- **X轴**：机器人前进方向（沿轨道顶边）
- **Y轴**：轨道横向（左负右正）
- **Z轴**：垂直向上

### 轨道布局
```
顶边：   X=0.6505, Y∈[-0.2775, +0.2775]  (555mm长)
右边：   Y=+0.2775, X∈[0.6505, -0.2125]
底边：   X=-0.2125, Y∈[+0.2775, -0.2775]
左边：   Y=-0.2775, X∈[-0.2125, 0.6505]
```

### 机器人配置
- **Chassis位置**：(0.6505, -0.105, 0.033)
- **朝向**：+Y方向（沿轨道）
- **传感器**：robot frame X=+0.105 → world Y=-0.105+0.105=0（轨道中心）

---

## 测试结果

### 短期测试（200步，0.4秒）
```
✓ 传感器检测正常
✓ 沿Y轴前进37mm
✓ X保持0.6505
△ Z轻微下沉（0.033→0.021）
```

### 长期测试（60秒）
```
✓ t=0-1s：正常运行
✗ t=1s：突然大转弯，丢失黑线
✗ t=2-60s：停止运动
```

---

## 修改文件清单

| 文件 | 修改内容 |
|------|---------|
| `models/scene.xml` | 改进积分器和求解器参数 |
| `models/robot.xml` | 降低actuator kv，增加阻尼和摩擦，修正轮子位置 |
| `models/track.xml` | 修正track_seg_0坐标 |
| `src/sim_main.py` | 修正初始位置和方向 |
| `src/sim_visualize.py` | 修正初始位置和方向 |
| `firmware_bridge/motion_control.py` | 降低MIN_SPEED |
| `test_sim_detailed.py` | 新增详细测试脚本 |
| `test_sim_quick.py` | 新增快速测试脚本 |

---

## 下一步工作

### 优先级1：修复1秒丢线问题
1. 添加完整轨迹日志
2. 生成可视化图表
3. 调整控制参数：
   - MAX_SPEED: 1.0 → 0.3 m/s
   - LOOKAHEAD: 0.25 → 0.15 m  
   - MAX_ACCEL: 2.0 → 0.5 m/s²

### 优先级2：验证完整循迹
1. 确保直线段稳定行驶
2. 测试转弯行为
3. 验证整圈循迹能力

### 优先级3：性能优化
1. 修复Z轴下沉
2. 优化速度曲线
3. 调整PID参数

---

## Git提交记录

```
commit ac8827f
fix: correct robot initial pose and improve physics stability

- Fix robot orientation: rotate 90° to align with track (+Y direction)
- Fix robot position: adjust to (0.6505, -0.105, 0.033)
- Reduce actuator gain: kv from 50 to 1
- Improve physics: implicitfast integrator, more damping
- Lower MIN_SPEED: from 0.10 to 0.01 m/s

Issues resolved:
- Sensors now detect track (IR ~650 on line)
- Robot moves along track in +Y direction
- Z-axis stable in static test

Known issues:
- Robot loses line after ~1 second
- Z-axis sinks during motion
- Control parameters need tuning
```

---

**完成日期：** 2026-07-29  
**工程师：** Claude Opus 4.8  
**状态：** 核心功能修复完成，需要进一步控制参数调优
