# 参数快速参考表 (Parameter Quick Reference)

**版本**: v1.0 | **日期**: 2026-07-30 | **用途**: 一页纸速查手册

---

## 1️⃣ 关键物理参数速查表

| 参数 | 当前值 | 单位 | 文件位置 | 验证状态 |
|------|--------|------|----------|----------|
| **WHEEL_RADIUS** | 0.033 | m | `modules/MotionControl/inc/motion_config.h:107` | ✅ 已实测 |
| **WHEEL_BASE** | 0.214 | m | `modules/MotionControl/inc/motion_config.h:63` | ✅ 已实测 |
| **ENCODER_PPR** | 60000 | counts/圈 | `modules/MotionControl/inc/motion_config.h:172` | ✅ 已实测 |
| **WHEEL_CIRCUMFERENCE** | 0.2073 | m | 计算值: 2π × WHEEL_RADIUS | ✅ 自动计算 |
| **GEAR_RATIO** | 300:1 | - | 间接包含在ENCODER_PPR中 | ℹ️ 隐含参数 |

**⚠️ 一致性要求**: 这些参数必须在 `motion_config.h` 和 `Sens-Decision/config.c` 中保持一致！

---

## 2️⃣ PID和前馈参数速查表

### PID 参数
| 参数 | 当前值 | 建议范围 | 调整影响 | 文件位置 |
|------|--------|----------|----------|----------|
| **SPEED_KP** | 200.0 | 100~300 | 响应速度，过大会振荡 | `motion_config.h:380` |
| **SPEED_KI** | 50.0 | 20~100 | 消除稳态误差，过大会低频摆动 | `motion_config.h:436` |
| **SPEED_KD** | 0.0 | 0~10 | 阻尼作用，通常不需要 | - |

**调参顺序**: 先调KP (响应速度) → 再调KI (稳态误差)

### 前馈参数
| 参数 | 当前值 | 建议范围 | 物理意义 | 文件位置 |
|------|--------|----------|----------|----------|
| **FF_K_ACCEL** | 50.0 | 30~100 | 加速补偿：克服惯性 | `motion_config.h:546` |
| **FF_K_FRICTION** | 300.0 | 200~400 | 匀速补偿：克服摩擦 | `motion_config.h:598` |
| **FF_K_STATIC** | 80.0 | 60~120 | 启动补偿：克服静摩擦 | `motion_config.h:652` |

**调参技巧**:
- 加速响应慢 → 增大 `FF_K_ACCEL`
- 恒速时PID持续输出正值 → 增大 `FF_K_FRICTION`
- 启动困难不转 → 增大 `FF_K_STATIC`

---

## 3️⃣ EKF参数速查表

| 参数 | 维度 | 当前值 | 建议范围 | 物理意义 | 文件位置 |
|------|------|--------|----------|----------|----------|
| **process_noise_diag** | x, y, θ, v, ω | 0.01 | 0.001~0.1 | 模型不确定性 | `Sens-Decision/config.c:375` |
| **observation_noise_diag[0]** | v | 0.03 | 0.01~0.1 | 线速度测量噪声 (m/s)² | `Sens-Decision/config.c:426` |
| **observation_noise_diag[1]** | ω | 0.08 | 0.03~0.2 | 角速度测量噪声 (rad/s)² | `Sens-Decision/config.c:427` |

**调参原则**:
- 增大 `process_noise` → 更信任传感器，跟踪更快但更抖
- 增大 `observation_noise` → 更信任模型，滤波更平滑但响应慢

---

## 4️⃣ 速度和限制参数速查表

| 参数 | 当前值 | 单位 | 建议范围 | 用途 | 文件位置 |
|------|--------|------|----------|------|----------|
| **MAX_SPEED** | 1.00 | m/s | 0.5~1.5 | 最大线速度限制 | `motion_config.h:665` |
| **MIN_SPEED** | 0.10 | m/s | 0.05~0.2 | 最小可控速度 | `motion_config.h:668` |
| **MAX_ACCELERATION** | 2.0 | m/s² | 1.0~3.0 | 最大加速度限制 | `motion_config.h:671` |
| **MAX_DECELERATION** | 3.0 | m/s² | 2.0~5.0 | 最大减速度限制 | `motion_config.h:674` |
| **MAX_OMEGA** | 6.0 | rad/s | 4.0~8.0 | 最大角速度限制 | `motion_config.h:677` |
| **CMD_SMOOTH_TAU** | 0.05 | s | 0.02~0.1 | 指令平滑时间常数 | `motion_config.h:688` |

**安全建议**: 调试时先降低速度和加速度限制，稳定后再逐步提升

---

## 5️⃣ 硬件参数速查表

| 参数 | 当前值 | 单位 | 计算公式 | 文件位置 |
|------|--------|------|----------|----------|
| **MOTOR_PWM_ARR** | 8399 | counts | (TIM_CLK / PWM_FREQ) - 1 | `Core/Src/app/motor.c:52` |
| **TIM_CLK** | 168,000,000 | Hz | STM32F407 APB2时钟 | 硬件固定 |
| **PWM_FREQ** | 20,000 | Hz | TIM_CLK / (ARR + 1) | 设计值 |
| **MOTOR_SPEED_MAX** | 100 | % | PWM占空比范围 | `motor.c:70` |
| **CONTROL_FREQ** | 100 | Hz | 控制环频率 | 主循环 |

**PWM频率说明**:
- 当前: 20 kHz (推荐范围: 15~25 kHz)
- 修改公式: `ARR = 168,000,000 / 目标频率(Hz) - 1`

---

## 6️⃣ 参数一致性检查清单

### ✅ 必须一致的参数组

| 参数组 | 文件1 | 文件2 | 检查方法 |
|--------|-------|-------|----------|
| **WHEEL_RADIUS** | `motion_config.h` | `Sens-Decision/config.c` (注释) | 手动对比 |
| **WHEEL_BASE** | `motion_config.h` | `Sens-Decision/config.c` (注释) | 手动对比 |
| **ENCODER_PPR** | `motion_config.h` | `Sens-Decision/config.c` (注释) | 手动对比 |

### 🔗 关联参数 (自动计算)

| 主参数 | 派生参数 | 关系 |
|--------|----------|------|
| **WHEEL_RADIUS** | WHEEL_CIRCUMFERENCE | `2π × WHEEL_RADIUS` |
| **MOTOR_PWM_ARR** | PWM_FREQ | `168MHz / (ARR + 1)` |
| **MAX_SPEED** | 最大轮速 | `MAX_SPEED / WHEEL_RADIUS` |

### 📋 修改参数后的必查项

1. **修改物理参数** → 检查两个config文件是否同步
2. **修改PID/FF参数** → 重新测试速度响应曲线
3. **修改EKF参数** → 观察位置估计抖动程度
4. **修改PWM参数** → 用示波器验证频率

---

## 📌 快速诊断指南

| 症状 | 可能原因 | 检查参数 |
|------|----------|----------|
| 速度响应慢 | PID-P太小 | 增大 `SPEED_KP` |
| 速度振荡 | PID-P太大 | 减小 `SPEED_KP` |
| 稳态误差 | PID-I不足 | 增大 `SPEED_KI` |
| 低频摆动 | PID-I过大 | 减小 `SPEED_KI` |
| 加速无力 | 前馈不足 | 增大 `FF_K_ACCEL` |
| 启动困难 | 静摩擦补偿不足 | 增大 `FF_K_STATIC` |
| 位置抖动 | EKF观测噪声太小 | 增大 `observation_noise` |
| 位置滞后 | EKF过程噪声太小 | 增大 `process_noise` |

---

**💡 提示**: 建议将此文档打印并贴在调试区域，调参时随时查阅
