# IMU 坐标系适配器与 IR 位置修复日志

**日期**: 2026-07-30
**任务**: 坐标系统一——IMU 适配器 + IR 位置修正
**状态**: ✅ 已完成（IR 权重符号待验证，见"遗留问题"）

---

## 📋 问题背景

### 根本原因

物理 IMU 安装方向与代码帧定义不一致：

| | X 轴 | Y 轴 | Z 轴 |
|---|---|---|---|
| **物理 IMU**（硬件实际） | 右侧（+X=右） | 前方（+Y=前） | 上方 |
| **代码帧**（EKF/轨迹算法） | 前方（+X=前） | 左侧（+Y=左） | 上方 |

IR 传感器的物理坐标数据也是按物理 IMU 方向标注的，导致 `config.c` 里的 IR 位置被写到了错误的轴上。

### 为什么代码帧是 前(+X)，左(+Y)

`EKF.c` 的运动方程铁证：

```c
ekf->state[0] = x + v * cos_theta * dt;   // θ=0 时 x 增加 → 前方 = +X
ekf->state[1] = y + v * sin_theta * dt;   // θ=90° 时 y 增加 → 左侧 = +Y
```

Pure Pursuit 的坐标变换也验证了这一点（`trajectory_generate.c:260`）：

```c
y_local = -sin_theta * dx + cos_theta * dy;  // y_local > 0 = 目标在左侧
```

### 为什么加适配器而不是改算法

EKF 和 Pure Pursuit 的数学是标准的、经过验证的。
适配器是一个 3×3 旋转矩阵，放在传感器输入边界处——这是机器人工程的标准做法（传感器帧 → 车体代码帧），改动最小、风险最低。

---

## 🔧 修复内容

### Fix 1：IMU 坐标适配器（preprocess.c）

**变换关系**：

```
物理 IMU (X=右, Y=前, Z=上) → 代码帧 (X=前, Y=左, Z=上)

code_X =  phys_Y    (前方 = 物理 Y)
code_Y = -phys_X    (左侧 = -(右侧) = -物理 X)
code_Z =  phys_Z    (上方，不变)
```

**新增静态函数**（`preprocess.c`，函数体在 `preprocess_update` 之前）：

```c
static void imu_adapt_to_code_frame(imu_data_t *imu) {
    float tmp[3];

    tmp[0] =  imu->accel_mps2[1];
    tmp[1] = -imu->accel_mps2[0];
    tmp[2] =  imu->accel_mps2[2];
    memcpy(imu->accel_mps2, tmp, sizeof(tmp));

    tmp[0] =  imu->gyro_radps[1];
    tmp[1] = -imu->gyro_radps[0];
    tmp[2] =  imu->gyro_radps[2];
    memcpy(imu->gyro_radps, tmp, sizeof(tmp));
}
```

**调用位置**（`preprocess_update` 内，IMU 读取成功后立即执行）：

```c
// 修改前：
if (status == SD_OK) {
    frame->imu_valid = true;
}

// 修改后：
if (status == SD_OK) {
    imu_adapt_to_code_frame(&frame->imu);
    frame->imu_valid = true;
}
```

**关于 gyro_z（yaw 轴）**：两套坐标系都以 Z 朝上、逆时针为正旋转，所以 `gyro_radps[2]` 符号不变。当前 EKF 观测量里实际只用了 `gyro_radps[2]`，因此这一项即便不加适配器也是对的——但加了之后 gyro_x/y 也变得正确，为未来可能用到的倾斜补偿打好基础。

---

### Fix 2：IR 传感器位置修正（config.c）

**旧配置**（错误：用了物理 IMU 的"前=+Y"惯例）：

```c
// IR传感器阵列位置 (新坐标系: 前=+Y, 右=+X)
// 注意: 代码可能假设旧坐标系，需要验证各模块的坐标系定义
g_sens_decision_config.perception.position.x_m = 0.0f;      // 阵列中心在中心线 ← X=0正确
g_sens_decision_config.perception.position.y_m = 0.1321f;   // "前方"132.1mm ← 但Y是左侧！错误
```

在代码帧（前=+X）中，`y_m = 0.1321f` 的含义是"左侧 132.1mm"，而不是"前方 132.1mm"。

**新配置**（正确：使用代码帧"前=+X"）：

```c
// IR传感器阵列位置 (代码坐标系: 前=+X, 左=+Y)
// 阵列安装在车头前方132.1mm，横向居中于中心线
g_sens_decision_config.perception.position.x_m = 0.1321f;   // 前方132.1mm (代码帧 前=+X) ✅
g_sens_decision_config.perception.position.y_m = 0.0f;      // 横向居中 ✅
```

**影响范围说明**：检查 `perception.c`，`lateral_error` 的计算**没有用到 `position` 字段**（只用了 `weights`），所以此项修复对当前的感知输出无立即影响。但在做精确传感器偏置补偿（几何投影、延时补偿等）时，这个值必须正确。

---

## 📊 影响分析

| 模块 | 修复前 | 修复后 |
|------|--------|--------|
| EKF 角速度观测（gyro_z） | ✅ 正确（Z 轴不变） | ✅ 正确（不变） |
| EKF 加速度输入 | ❌ 轴向混淆（X/Y 对调且符号错） | ✅ 正确 |
| EKF 角速度 gyro_x/y | ❌ 轴向混淆 | ✅ 正确 |
| IR 传感器位置记录 | ❌ 位于"左侧132.1mm" | ✅ 位于"前方132.1mm" |
| lateral_error 计算 | 未受影响（不用 position） | 未受影响（不用 position） |

---

## ⚠️ 遗留问题：IR 权重符号（待验证）

本次未修改 `ir_weights` 数组，原因是符号约定需要与下游控制器对齐，理论推导容易出错，**实测最可靠**。

### 验证方法

将小车放在黑线上，观察 `lateral_error`：

```
手动将小车向右推偏 → 观察 lateral_error 符号
手动将小车向左推偏 → 观察 lateral_error 符号
```

根据控制器的期望约定，判断符号是否正确。如果符号反了，只需对整个 `ir_weights` 数组取反（一行改动）。

另有一个独立的**权重大小顺序问题**也需要一并讨论——当前近中心传感器的绝对权重反而大于外侧传感器，与物理位置成反向关系，下一步将专门讨论。

---

## 📄 修改的文件

| 文件 | 修改内容 |
|------|----------|
| `modules/Sens-Decision/src/preprocess.c` | 新增 `imu_adapt_to_code_frame()` 静态函数，并在 IMU 读取后调用 |
| `modules/Sens-Decision/src/config.c` | IR 位置：`x_m 0→0.1321`，`y_m 0.1321→0` |

---

**日志撰写时间**: 2026-07-30
**执行者**: Claude Code (Opus 4.8) + 用户 Joelin
**最终状态**: ✅ IMU 适配器已加入 / ✅ IR 位置已修正 / ⏳ IR 权重符号待实测验证
