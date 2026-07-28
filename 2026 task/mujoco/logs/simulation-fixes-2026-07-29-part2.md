# 仿真系统修复日志 Part 2 - 2026-07-29

## 修复事件：机器人初始位置和方向错误

**日期：** 2026-07-29  
**严重程度：** Critical  
**状态：** ✅ 部分修复，仍需优化控制参数

---

## 问题描述

### 问题1：机器人初始位置错误
- 机器人chassis位置设为(0.6605, 0.0)，但传感器在chassis前方+0.105m
- 实际传感器位置：(0.7555, 0.0)，超出轨道边缘14.5mm
- 结果：传感器检测不到黑线（IR=4095）

### 问题2：机器人初始方向错误  
- 机器人朝向+X方向
- 轨道顶边沿着+Y方向（从Y=-0.2775到+0.2775）
- 结果：机器人垂直于轨道前进，立即驶离黑线

### 问题3：机器人飞起来
- 初始配置下，机器人Z坐标从0.033快速上升到1.36m
- 原因：actuator kv=50增益过大，轮子产生过大力矩

### 问题4：轮子位置配置错误
- 轮子相对chassis Z=0，导致轮子底部应该在世界Z=0，但实际悬空

---

## 根本原因分析

### 坐标系混淆
- **用户坐标系**：Y轴前进，单位mm
- **MuJoCo坐标系**：Z轴向上，单位m
- **映射关系**：user_y → mujoco_x, user_x → mujoco_y

### 轨道布局（MuJoCo坐标）
```
Track segments (after 5x scaling):
  Top edge:    X=0.6505, Y from -0.2775 to +0.2775 (555mm long)
  Right edge:  Y=+0.2775, X from 0.6505 to -0.2125
  Bottom edge: X=-0.2125, Y from +0.2775 to -0.2775
  Left edge:   Y=-0.2775, X from -0.2125 to 0.6505
```

### 传感器布局（robot frame）
- 8个IR传感器在chassis前方X=+0.105m
- 横向排列，Y从-0.049到+0.049（98mm宽度）
- 指向下方（Z=-0.020）

### 旋转后的传感器位置
- Robot frame (X=+0.105, Y=varying) 
- 旋转90°后 → World frame (chassis_y + 0.105, chassis_x - sensor_x_offset)

---

## 修复方案

### 修复1：调整机器人初始位置和方向

**文件：** `src/sim_main.py`, `src/sim_visualize.py`

**修改前：**
```python
INITIAL_POS = [0.6605, 0.0, 0.033]   # 错误：传感器超出轨道
INITIAL_QUAT = [1, 0, 0, 0]           # 错误：机器人朝向+X
```

**修改后：**
```python
# 机器人朝向+Y方向（沿轨道）
# 传感器应该在轨道中心线：X=0.6505, Y=0
# 旋转90°后，传感器位置 = (chassis_y + 0.105, chassis_x)
# 所以：chassis_y = -0.105, chassis_x = 0.6505
INITIAL_POS = [0.6505, -0.105, 0.033]    
INITIAL_QUAT = [0.7071, 0, 0, 0.7071]  # 90° Z轴旋转
```

### 修复2：降低actuator增益

**文件：** `models/robot.xml`

**修改：**
```xml
<!-- 修改前：kv=50 -->
<velocity name="act_lf" joint="jwheel_lf" kv="1" gear="1"/>
<!-- 其他轮子同样 -->
```

### 修复3：改进物理参数

**文件：** `models/scene.xml`

**修改：**
```xml
<!-- 使用更稳定的积分器和求解器参数 -->
<option timestep="0.002" integrator="implicitfast" gravity="0 0 -9.81" 
        iterations="50" tolerance="1e-10">
  <flag warmstart="enable" contact="enable"/>
</option>

<default>
  <joint damping="0.5" armature="0.01"/>  <!-- 增加阻尼 -->
  <geom solref="0.002 1" solimp="0.9 0.95 0.001"/>  <!-- 更硬的接触 -->
</default>
```

**文件：** `models/robot.xml`

**修改轮子摩擦和阻尼：**
```xml
<joint damping="0.1"/>  <!-- 从0.01增加到0.1 -->
<geom friction="1.5 0.005 0.0001"/>  <!-- 从1.0增加到1.5 -->
```

### 修复4：降低MIN_SPEED门槛

**文件：** `firmware_bridge/motion_control.py`

**修改：**
```python
MIN_SPEED = 0.01  # m/s (从0.10降低到0.01，允许更柔和的启动)
```

---

## 验证结果

### 测试1：传感器位置验证
```
旋转后传感器位置(世界坐标):
  IR3: X=0.6575, Y=0.0000, dist=7.0 mm  ✓
  IR4: X=0.6435, Y=0.0000, dist=7.0 mm  ✓
  (中间传感器距离轨道中心7mm，可接受)
```

### 测试2：短期运动测试(200步，0.4秒)
```
- IR检测正常：中间传感器~650，边缘~4095
- 机器人沿+Y前进：Y从-0.105到-0.068 (37mm)
- X保持稳定：X=0.6505
- Z轻微下沉：从0.033到0.021（需要关注）
```

### 测试3：长期仿真测试(60秒)
```
- t=0.0s: 启动正常，检测到黑线
- t=1.0s: 横向误差突然变大(-0.7739)，角速度饱和(-5.4 rad/s)
- t=2.0s开始: 丢失黑线，停止运动
- 问题：机器人在第一秒内可能转弯过猛或速度过快冲出轨道
```

---

## 遗留问题

### 问题1：机器人在1秒内丢失黑线
**状态：** 🔄 需要调查

**可能原因：**
1. 速度增长过快（从0到0.5 m/s在1秒内）
2. Pure Pursuit控制器参数不当（LOOKAHEAD=0.25m）
3. 轮速控制增益不匹配（PWM→omega转换）
4. 机器人物理模型不稳定（Z下沉）

**下一步：**
1. 添加详细的位置/速度/传感器日志
2. 可视化机器人轨迹
3. 调整Pure Pursuit前瞻距离
4. 优化速度增长曲线
5. 检查Z轴稳定性问题

### 问题2：Z轴下沉
**状态：** ⚠️ 观察中

**观察：**
- 静态测试：Z保持稳定（变化<0.0001m）
- 动态测试：Z从0.033降到0.021（-12mm）

**可能原因：**
- 轮胎与地面的接触模型穿透
- 悬架compliance过大
- 需要调整接触参数

### 问题3：PWM→omega转换
**状态：** ⚠️ 待验证

**当前实现：**
```python
omega_target = (pwm / PWM_MAX) * (1.0 / WHEEL_RADIUS)
# PWM=1000 -> omega=30.3 rad/s
```

**疑问：**
- 这个映射是否与actuator kv=1匹配？
- 固件中的PWM→速度关系是什么？

---

## 文件修改列表

✅ `models/scene.xml` - 改进物理参数  
✅ `models/robot.xml` - 降低actuator kv，增加轮子摩擦和阻尼  
✅ `models/track.xml` - 修正track_seg_0坐标(0.6605→0.6505)  
✅ `src/sim_main.py` - 修正初始位置和方向  
✅ `src/sim_visualize.py` - 修正初始位置和方向  
✅ `firmware_bridge/motion_control.py` - 降低MIN_SPEED门槛  
✅ `test_sim_detailed.py` - 测试脚本  
✅ `test_sim_quick.py` - 快速测试脚本

---

## 下次工作计划

1. **诊断1秒丢线问题**
   - 添加完整的轨迹记录（position, velocity, IR, commands）
   - 生成可视化图表
   - 分析失控时刻的传感器和控制状态

2. **优化控制参数**
   - 降低最大速度限制（从1.0到0.3 m/s）
   - 调整Pure Pursuit LOOKAHEAD（从0.25到0.15）
   - 调整速度增长率（MAX_ACCEL从2.0到0.5）

3. **修复Z轴下沉**
   - 检查轮胎半径和地面高度配置
   - 调整接触参数solimp/solref

4. **验证完整循迹**
   - 确保机器人能稳定沿直线段行驶
   - 测试转弯行为
   - 验证整圈循迹能力

---

**创建者：** Claude (MuJoCo仿真工程师)  
**审核状态：** 待进一步测试和优化
