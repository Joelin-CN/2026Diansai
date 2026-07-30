# 坐标系统一修复完整日志

**日期**: 2026-07-30
**任务**: 消除物理 IMU 坐标系与代码帧之间的不一致
**状态**: ✅ 代码修复完成 / ⏳ 实物符号验证待完成

---

## 📋 根本原因

陀螺仪物理安装方向与代码算法约定不同：

| | X 轴 | Y 轴 | Z 轴 |
|---|---|---|---|
| **物理 IMU（硬件）** | 右侧 | 前方 | 上方 |
| **代码帧（EKF/轨迹算法）** | 前方 | 左侧 | 上方 |

IR 传感器阵列的所有坐标数据都是按物理 IMU 帧标注的，
导致直接写入代码后出现轴向混淆和符号错误。

代码帧由 `EKF.c` 的运动方程确认：
```c
ekf->state[0] = x + v * cos_theta * dt;  // θ=0 时 x 增加 → 前方 = +X
ekf->state[1] = y + v * sin_theta * dt;  // θ=90° 时 y 增加 → 左侧 = +Y
```

---

## 🔧 修复内容（共 3 处）

### Fix 1 — IMU 坐标适配器（`preprocess.c`）

物理帧 → 代码帧的变换：
```
code_X =  phys_Y    (前方 = 物理 Y)
code_Y = -phys_X    (左侧 = -(右侧) = -物理 X)
code_Z =  phys_Z    (上方，不变)
```

新增静态函数 `imu_adapt_to_code_frame(imu_data_t *imu)`，
在 `preprocess_update()` 内 IMU 读取成功后立即调用，
对 `accel_mps2` 和 `gyro_radps` 均执行变换。

> `gyro_radps[2]`（yaw 轴）不变：两套坐标系 Z 轴方向相同，正旋转约定相同（逆时针为正）。

---

### Fix 2 — IR 阵列中心位置（`config.c`）

```c
// 修改前（按物理帧"前=+Y"写的，在代码帧里实为"左侧 132.1mm"）：
perception.position.x_m = 0.0f;
perception.position.y_m = 0.1321f;

// 修改后（代码帧 前=+X）：
perception.position.x_m = 0.1321f;   // 前方 132.1mm
perception.position.y_m = 0.0f;      // 横向居中
```

> 注：当前 `perception.c` 的 `lateral_error` 计算不使用 `position` 字段，
> 此修复为几何补偿/传感器融合预留正确基础。

---

### Fix 3 — IR 传感器权重（`config.c`）

IR 各传感器的横向位置在物理帧为 X_phys，转换到代码帧为：
```
code_Y = -X_phys
权重 = code_Y / 10mm
```

| 索引 | X_phys(mm) | code_Y(mm) | 修改前权重 | 修改后权重 |
|------|-----------|-----------|-----------|-----------|
| 0 | +5.69 | −5.69 | +3.9861 ❌ | **−0.5694** |
| 1 | +17.08 | −17.08 | +2.8472 ❌ | **−1.7083** |
| 2 | +28.47 | −28.47 | +1.7083 ❌ | **−2.8472** |
| 3 | +39.86 | −39.86 | +0.5694 ❌ | **−3.9861** |
| 4 | −5.69 | +5.69 | −0.5694 ❌ | **+0.5694** |
| 5 | −17.08 | +17.08 | −1.7083 ❌ | **+1.7083** |
| 6 | −28.47 | +28.47 | −2.8472 ❌ | **+2.8472** |
| 7 | −39.86 | +39.86 | −3.9861 ❌ | **+3.9861** |

旧权重错误原因：符号反向（右侧应为负）+ 大小顺序颠倒（越外侧应绝对值越大）。

修复后语义：
- 车偏右 → 右侧传感器亮（负权重）→ `lateral_error < 0`
- 车偏左 → 左侧传感器亮（正权重）→ `lateral_error > 0`
- 偏越远 → 越外侧传感器亮 → 绝对值越大 ✓

用户已确认各传感器在小车坐标系下的物理坐标吻合。

---

## 📊 修改文件汇总

| 文件 | 修改内容 |
|------|----------|
| `modules/Sens-Decision/src/preprocess.c` | 新增 `imu_adapt_to_code_frame()`，IMU 读取后调用 |
| `modules/Sens-Decision/src/config.c` | IR 位置 `y_m 0.1321→0`，`x_m 0→0.1321` |
| `modules/Sens-Decision/src/config.c` | IR 权重：符号翻转 + 大小顺序修正 |

---

## ⏳ 遗留验证项

### 必做：lateral_error 符号实测

编译烧录后，将小车放在黑线上，通过串口观察 `lateral_error`：

```
手动向右推偏 → lateral_error 应 < 0
手动向左推偏 → lateral_error 应 > 0
```

如果符号反了，只需对 `ir_weights` 数组整体取反（一行改动）。

### 建议：轮半径实测

当前 `wheel_radius_m = 0.033f` 未经实测，若实际值偏差 > 5% 会影响速度和位移估计。

---

## 🔗 相关文档

- `logs/2026-07-30_imu_coord_adapter_fix.md` — Fix 1/2 的详细推导
- `logs/2026-07-30_encoder_ppr_correction.md` — 编码器 PPR=60000 的修正记录
- `modules/Sens-Decision/src/preprocess.c` — IMU 适配器实现
- `modules/Sens-Decision/src/config.c` — 传感器参数配置

---

**日志撰写时间**: 2026-07-30
**执行者**: Claude Code (Opus 4.8) + 用户 Joelin
**最终状态**: ✅ 三处坐标系修复已提交 / ⏳ 实物符号验证待完成
