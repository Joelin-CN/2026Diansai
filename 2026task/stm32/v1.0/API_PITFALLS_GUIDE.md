# 模块调试避坑指南 (API Pitfalls Guide)

**目的**: 记录每个模块调试过程中发现的关键问题和正确调用方式，避免重复踩坑。

**最后更新**: 2026-07-30

**数据来源**: `logs/` 目录下的 35 个调试日志文件

---

## 目录

1. [电机模块 (Motor)](#1-电机模块-motor)
2. [编码器模块 (Encoder)](#2-编码器模块-encoder)
3. [红外传感器模块 (IR Sensor)](#3-红外传感器模块-ir-sensor)
4. [IMU模块 (ICM42688)](#4-imu模块-icm42688)
5. [传感器适配层 (Sensor Adapter)](#5-传感器适配层-sensor-adapter)
6. [预处理层 (Preprocess)](#6-预处理层-preprocess)
7. [坐标系 (Coordinate System)](#7-坐标系-coordinate-system)
8. [FreeRTOS与中断 (RTOS & IRQ)](#8-freertos与中断-rtos--irq)
9. [编译与工具链 (Build & Toolchain)](#9-编译与工具链-build--toolchain)
10. [控制应用层 (ControlApp)](#10-控制应用层-controlapp)
11. [EKF模块 (EKF)](#11-ekf模块-ekf)
12. [内存安全 (Memory Safety)](#12-内存安全-memory-safety)
13. [控制频率优化 (Control Frequency)](#13-控制频率优化-control-frequency)
14. [参数调优建议](#14-参数调优建议)

---

## 1. 电机模块 (Motor)

### 1.1 🔴 TIM1高级定时器必须使能MOE位

**问题**: 电机完全不转，但所有配置看起来都正确。

**根因**: TIM1是高级定时器，HAL库默认不设置MOE（Main Output Enable）位。

**错误代码**:
```c
void Motor_Init(void) {
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);  // ❌ PWM不会输出！
}
```

**正确代码**:
```c
void Motor_Init(void) {
    __HAL_TIM_MOE_ENABLE(&htim1);              // ← 必须先调用
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);  // ✅ 现在才能输出PWM
}
```

**来源**: `logs/2026-07-30_motor_debug_complete.md`

---

### 1.2 🔴 PWM占空比计算Bug（10倍误差）

**问题**: `Motor_SetSpeed(100, 100)` 实际只有10%占空比。

**根因**: `MOTOR_SPEED_MAX` 定义为1000而非100，导致除法错误。

**错误代码**:
```c
#define MOTOR_SPEED_MAX 1000  // ❌ 导致10倍误差
// Motor_SetSpeed(100, 100) → pulse = 100 * 8399 / 1000 = 840 = 10% PWM
```

**正确代码**:
```c
#define MOTOR_SPEED_MAX 100   // ✅ 输入范围：-100到+100（百分比）
// Motor_SetSpeed(100, 100) → pulse = 100 * 8399 / 100 = 8399 = 100% PWM
```

**调用规范**:
```c
Motor_SetSpeed(30, 30);    // 30% PWM
Motor_SetSpeed(-50, -50);  // -50% PWM（后退）
Motor_SetSpeed(0, 0);      // 停止（制动模式）
```

**来源**: `logs/2026-07-30_motor_debug_complete.md`, `logs/2026-07-30_motor_code_review_and_fix.md`

---

### 1.3 🔴 TB6612停止模式陷阱：Coast ≠ Brake

**问题**: 调用 `Motor_SetSpeed(0, 0)` 后，电机继续高速滑行而非立即制动。

**根因**: IN1=L, IN2=L 在TB6612中是 **Coast（高阻/滑行）**，不是 Brake（制动）。

**TB6612关键真值表**:

| IN1 | IN2 | PWM | 输出模式 |
|-----|-----|-----|---------|
| L | L | X | **Stop/Coast（滑行）** |
| H | H | X | **Short Brake（制动）** ← 正确做法 |
| H | L | H | CW（正转） |
| L | H | H | CCW（反转） |

**错误代码**:
```c
// ❌ 注释说"brake mode"，实际是Coast（滑行）
HAL_GPIO_WritePin(in1p, in1, GPIO_PIN_RESET);  // IN1=L → Coast!
HAL_GPIO_WritePin(in2p, in2, GPIO_PIN_RESET);  // IN2=L
```

**正确代码**:
```c
// ✅ Short Brake: 两脚同高 → 输出端短接到GND → 强制制动
HAL_GPIO_WritePin(in1p, in1, GPIO_PIN_SET);    // IN1=H
HAL_GPIO_WritePin(in2p, in2, GPIO_PIN_SET);    // IN2=H → Short Brake ✓
```

**验证数据**: 从40%速度停止后，Coast模式左轮滑行100,940 counts；Brake模式应大幅减少。

**来源**: `logs/2026-07-30_encoder_verification_and_motor_stop_fix.md`

---

### 1.4 ⚠️ API注释陷阱

**问题**: 头文件注释说输入范围是 `[-1000, +1000]`，实际是 `[-100, +100]`。

**教训**: 错误的注释比没有注释更危险。

**来源**: `logs/2026-07-30_motor_code_review_and_fix.md`

---

### 1.5 🔌 引脚映射

```
MOTOR_B → 左轮: TIM1_CH1(PE9), IN1: PD14, IN2: PD15
MOTOR_C → 右轮: TIM1_CH2(PE11), IN1: PE2, IN2: PE3
STBY: PE0 → 必须为HIGH电机才使能
```

## 2. 编码器模块 (Encoder)

### 2.1 🔴 PPR 从 1,560 → 60,000（38.5倍误差！）

**问题**: 速度估计与实际速度差38.5倍，PID控制严重异常。

**根因**: 编码器线数误认为13 PPR，实际是 500 PPR。

**历史演变**:
```
初始错误: 13 PPR × 4 × 30 = 1,560 counts/rev  → 严重错误
最终正确: 500 PPR × 4 × 30 = 60,000 counts/rev → 实测确认
```

**正确配置**:
```c
// modules/MotionControl/inc/motion_config.h
#define ENCODER_PPR 60000

// modules/Sens-Decision/src/config.c
encoder->pulses_per_revolution = 60000U;
```

**实测验证** (10% PWM, 3秒): 左轮128,950 counts, 实际转动2.2圈 → 实测~56,950 counts/圈（偏差5%）

**影响**:
```
修复前: 0.5m/s → 估计 0.013m/s → PID认为太慢 → 加大油门 → 失控
修复后: 0.5m/s → 估计 0.5m/s   → 正常控制
```

**来源**: `logs/2026-07-30_encoder_ppr_correction.md`

---

### 2.2 正确读取编码器

```c
#include "encoder_hw_bridge.h"

int32_t left  = EncoderHwBridge_GetCount(0);  // 左轮 = TIM3 (PB4+PB5)
int32_t right = EncoderHwBridge_GetCount(1);  // 右轮 = TIM4 (PD12+PD13)
```

**硬件参数**: 500 PPR, 30:1减速比, 编码器在电机轴上 → 轮子转1圈=60,000 counts

---

### 2.3 ⚠️ 编码器计数方向验证

给正PWM前进 → 编码器计数应增加(正值)；给负PWM后退 → 编码器计数应减少(负值)

## 3. 红外传感器模块 (IR Sensor)

### 3.1 🔴 致命问题：USART2中断与HAL库冲突

**症状**: 只有6%的帧解析成功（61帧/10秒 vs 期望1250帧）

**根因**: `USART2_IRQHandler()` 手动读取DR后调用 `HAL_UART_IRQHandler()` 导致双重读取

**错误代码**:
```c
void USART2_IRQHandler(void) {
    uint8_t byte = huart2.Instance->DR;  // 手动读（清除RXNE）
    IrUartSensor_RxByte(byte);
    HAL_UART_IRQHandler(&huart2);        // ← ❌ HAL再次检查RXNE
}
```

**正确代码**:
```c
void USART2_IRQHandler(void) {
    if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_RXNE)) {
        uint8_t byte = (uint8_t)(huart2.Instance->DR & 0xFF);
        IrUartSensor_RxByte(byte);
    }
    if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_ORE)) {
        __HAL_UART_CLEAR_OREFLAG(&huart2);
    }
    // 不调用 HAL_UART_IRQHandler(&huart2);
}
```

**来源**: `logs/2026-07-30_ir_sensor_root_cause_analysis.md`, `logs/2026-07-30_ir_sensor_fix_implementation.md`

---

### 3.2 🔴 RXNEIE被HAL静默禁用

**症状**: 初始化后CR1=0x0000200C [RXNEIE=OFF!]，但传感器在正常发送数据

**触发条件**: `HAL_UART_Transmit()`(printf) 发送时会清所有UART的RXNEIE；ORE错误会触发HAL禁用中断

**解决方案**: 在关键操作后防御性重新使能
```c
// 在初始化后、任何printf后、主循环开始时
SET_BIT(huart2.Instance->CR1, USART_CR1_RXNEIE);

// 验证方法
if (huart2.Instance->CR1 & USART_CR1_RXNEIE) {
    printf("CR1=0x%08lX [RXNEIE=ON]\r\n", huart2.Instance->CR1);
}
```

**来源**: `logs/2026-07-30_rxneie_force_enable_fix.md`

---

### 3.3 🔴 驱动层 Process() 必须被调用

**症状**: 中断已收到数据，但 `IrUartSensor_GetAnalog()` 永远返回false

**根因**: 驱动层是"中断缓冲+轮询解析"三层模式：
1. 中断: 字节→g_frame_buffer[], 设g_frame_ready
2. **Process()**: 帧校验→解析→g_analog[]  ← 必须主动调用！
3. GetAnalog(): 返回已解析数据

**错误调用**:
```c
IrUartSensor_GetAnalog(raw);  // ❌ 永远返回false
```

**正确调用**:
```c
IrUartSensor_Process();       // ✅ 先解析（驱动帧→已解析数据）
IrUartSensor_GetAnalog(raw);  // ✅ 再读取
```

**适配器层已修复**: `sensor_adapter.c::read_ir()` 中已添加Process()调用

**来源**: `BUGFIX_lateral_error_verification.md` 第3.2节

---

### 3.4 🔴 竞态条件导致帧数据损坏（已修复为双缓冲）

**症状**: 错误率1300%+

**根因**: 中断和主循环共享 `g_rx_frame[]`，存在竞态窗口

**修复方案**: 双缓冲机制——中断写buffer[write_idx]，主循环读buffer[read_idx]，永不冲突

**来源**: `logs/2026-07-30_all_attempts_summary.md`

---

### 3.5 红外传感器正确使用流程

```c
#include "ir_uart_sensor.h"

// 1. 初始化
IrUartSensor_Init();
IrUartSensor_RequestAnalogMode();

// 2. 防御性使能RX中断
SET_BIT(huart2.Instance->CR1, USART_CR1_RXNEIE);

// 3. 主循环（≥100Hz，最好125Hz）
for (;;) {
    IrUartSensor_Process();                    // 解析帧 ← 关键！
    uint16_t raw[8];
    if (IrUartSensor_GetAnalog(raw)) {
        // raw[0..7]: 8通道 0-4095, 从右到左排列
    }
    osDelay(8);  // 建议≤10ms
}
```

**传感器布局**（从小车前方看）:
```
右侧  +39.86  +28.47  +17.08  +5.69   -5.69   -17.08   -28.47   -39.86 (mm)  左侧
        [0]     [1]     [2]    [3]     [4]      [5]      [6]      [7]
```

**调试工具**: `IrUartDiag_PrintReport()` 打印统计, `IrUartDiag_SensorTest()` 运行测试

**来源**: `logs/2026-07-30_ir_sensor_debug_summary.md`

## 4. IMU模块 (ICM42688)

### 4.1 ⚠️ 上电初始化时序

**症状**: 首次WHO_AM_I读取返回0x00

**原因**: ICM42688需要10-50ms上电稳定。`icm42688_hal.c`内部已处理延时和软复位。

**经验**: 自己的手动SPI读取如果首次返回0x00，多读几次即可。

**来源**: `logs/2026-07-29_imu_debug_complete.md`

---

### 4.2 ⚠️ 陀螺仪零偏标定（每次上电必须做）

**重要性**: 未标定的零偏可达±10 deg/s → Yaw每秒漂移10°

**正确流程**:
```c
// 1. 传感器必须静止
// 2. 采集200个样本
// 3. 计算平均值作为零偏
icm42688_calibrate_gyro_bias(&sensor, 200);
```

### 4.3 ⚠️ 6轴AHRS Yaw漂移

**原因**: 无磁力计校正，陀螺仪积分误差随时间累积

**缓解方案**: 1)添加磁力计 2)用轨道几何约束 3)静止时重新标定

### 4.4 正确初始化IMU

```c
#include "icm42688_hal.h"
#include "ahrs_hal.h"

// 1. 绑定平台适配器
icm42688_platform_bind(&stm32_platform);

// 2. 初始化传感器 (±8g, ±1000dps, 1kHz)
icm42688_config_t cfg = {
    .interface_type = ICM42688_INTERFACE_SPI,
    .acc_sample = ICM42688_ACC_SAMPLE_SGN_8G,
    .gyro_sample = ICM42688_GYRO_SAMPLE_SGN_1000DPS,
    .sample_rate = ICM42688_SAMPLE_RATE_1000,
};
icm42688_init(&cfg);

// 3. 标定零偏（传感器必须静止！）
icm42688_calibrate_gyro_bias(200);

// 4. 初始化AHRS
ahrs_init(AHRS_ALGORITHM_MAHONY, 1000.0f);

// 5. 在50Hz循环中更新
icm42688_data_t data;
icm42688_read(&data);
ahrs_update(data.acc_raw, data.gyro_raw, 0.02f);  // dt=20ms
```

**来源**: `logs/2026-07-29_imu_debug_complete.md`

---

## 5. 传感器适配层 (Sensor Adapter)

### 5.1 🔴 适配器必须阅读驱动实现后才能编写

**教训**: 不能仅凭函数名推测行为！原实现假设GetAnalog()自包含解析，但实际驱动需显式调用Process()。

**已修复**: `sensor_adapter.c::read_ir()` 中已添加 `IrUartSensor_Process()` 调用。

**来源**: `BUGFIX_lateral_error_verification.md`

## 6. 预处理层 (Preprocess)

### 6.1 🔴 IMU数据必须经过坐标系转换

**问题**: 物理IMU坐标系和代码算法坐标系不一致

**坐标系映射**:
```
物理IMU: X=右侧, Y=前方, Z=上方
代码算法: X=前方, Y=左侧, Z=上方

变换: code_X=phys_Y, code_Y=-phys_X, code_Z=phys_Z
```

**代码位置**: `preprocess.c::imu_adapt_to_code_frame()`，在 `preprocess_update()` 中自动调用

**来源**: `logs/2026-07-30_coord_system_complete_fix.md`

---

## 7. 坐标系 (Coordinate System)

### 7.1 代码帧定义

**X=前方, Y=左侧, Z=上方**

确认依据（EKF运动方程）:
```c
ekf->state[0] = x + v * cos_theta * dt;  // θ=0时x增加 → +X=前方
ekf->state[1] = y + v * sin_theta * dt;  // θ=90°时y增加 → +Y=左侧
```

### 7.2 lateral_error 符号约定

```c
lateral_error = Σ(weight[i] × ir_value[i]) / Σ(ir_value[i])
```

| 车体偏差 | lateral_error | 修正方向 |
|---------|--------------|---------|
| 车偏右（线在左侧传感器） | **> 0** | 向左转 |
| 车偏左（线在右侧传感器） | **< 0** | 向右转 |
| 居中 | **~ 0** | 直行 |

**验证**: 旁路测试——车居中≈0, 车向左移动（线在右侧传感器）<0, 车向右移动（线在左侧传感器）>0

### 7.3 IR权重（代码帧，已修正2026-07-30）

```c
static const float ir_weights[8] = {
    +3.9861f, +2.8472f, +1.7083f, +0.5694f,  // 通道0-3（左侧，正权重）
    -0.5694f, -1.7083f, -2.8472f, -3.9861f   // 通道4-7（右侧，负权重）
};
```

**说明**：
- 通道0（最左侧）→ +3.9861（最大正值）
- 通道7（最右侧）→ -3.9861（最大负值）
- 代码坐标系：+Y=左侧，-Y=右侧

### 7.4 IR阵列位置（代码帧）

```c
perception.position.x_m = 0.183f;  // 前方183mm
perception.position.y_m = 0.0f;     // 横向居中
```

**来源**: `logs/2026-07-30_coord_system_complete_fix.md`, `logs/2026-07-30_imu_coord_adapter_fix.md`, `BUGFIX_lateral_error_verification.md`

## 8. FreeRTOS与中断 (RTOS & IRQ)

### 8.1 ⚠️ 中断优先级必须低于 configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY

**配置**: `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY = 5`

**规则**:
```
0-4:  不受FreeRTOS临界区影响，但禁止调用FreeRTOS API
5:    临界区边界（可能在taskENTER_CRITICAL()中被阻塞）
6-15: 可安全调用FreeRTOS API
```

**USART2（红外传感器）**: Priority 3 — 高速125Hz数据流需要低延迟
**UART5（printf）**: Priority 7 — 可安全使用FreeRTOS

**来源**: `logs/2026-07-30_ir_sensor_root_cause_analysis.md`

---

### 8.2 🔴 printf可能静默禁用USART2接收中断

**根因**: `HAL_UART_Transmit()` 发送时临时清除所有UART的RXNEIE标志，防止中断嵌套。发送完可能不恢复。

**解决方案**:
```c
// 在初始化完成后
printf("[ControlApp] Running\r\n");

// 防御性恢复USART2中断！
SET_BIT(huart2.Instance->CR1, USART_CR1_RXNEIE);
```

**来源**: `BUGFIX_lateral_error_verification.md` 第4.1节

---

## 9. 编译与工具链 (Build & Toolchain)

### 9.1 🔴 newlib-nano 浮点数 printf 陷阱

**症状**: `printf("%f", 1.5)` 输出为空

**根因**: `--specs=nano.specs` 默认不含浮点数printf（节省Flash）

**修复**:
```cmake
# cmake/gcc-arm-none-eabi.cmake
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -u _printf_float")
```

**代价**: Flash增加~7-10KB（1MB Flash可接受）

**来源**: `logs/2026-07-29_imu_debug_complete.md`

---

### 9.2 Keil编译错误: Device STM32F407VGTx not found

**原因**: Keil RTE Pack搜索路径未指向正确目录

**解决**: Pack Installer中安装 `Keil::STM32F4xx_DFP` 和 `ARM::CMSIS`

**来源**: `logs/vscode-debug-fix-2026-07-29.md`

## 10. 控制应用层 (ControlApp)

### 10.1 完整控制流水线架构

```
500 Hz 主循环 (每2ms):
  ControlApp_RunFastCycle()
    ├─ Encoder_Poll()                    ← 编码器采样（每次）
    └─ MotionControl_Update(dt)          ← PID控制+PWM输出（每次）

    每20ms (cycle % 10 == 0):
      ├─ preprocess_update()             ← 传感器读取（IMU+IR+Encoder）
      └─ state_evaluator_update()        ← EKF状态估计
      └─ perception_update()             ← lateral_error/heading_error
      └─ behavior_planner_update()       ← 行为决策
      └─ trajectory_generate()           ← 轨迹生成
      └─ SquarePath_CorrectOmega()       ← 路径跟踪修正
      └─ MotionControl_SetVelocityCommand() ← 更新速度指令
```

### 10.2 调试策略：先旁路后集成

**铁则**: 不要一开始就用完整流水线调试！

**正确流程**:
```
Step 1: 电机 → motor_speed_test.c 旁路测试（自动循环）
Step 2: 编码器 → 直接 EncoderHwBridge_GetCount()
Step 3: IR传感器 → IrUartSensor_GetAnalog() 旁路测试
Step 4: IMU → imu_debug.c 旁路测试
Step 5: 集成 → ControlApp_Init() + ControlApp_RunFastCycle()
```

**经验**: 简单自动测试 > 复杂交互式工具。初期交互式测试曾因串口问题无响应。

### 10.3 正确启动流程

```c
void StartDefaultTask(void *argument)
{
    // 1. 初始化
    if (!ControlApp_Init(3)) {  // 目标3圈
        printf("[FATAL] Init failed\r\n");
        for (;;) { osDelay(1000); }
    }

    // 2. 防御性修复：printf可能清除了USART2中断
    SET_BIT(huart2.Instance->CR1, USART_CR1_RXNEIE);

    printf("[ControlApp] Running -- target laps: 3\r\n");

    // 3. 500 Hz 控制循环
    for (;;) {
        ControlApp_RunFastCycle();
        osDelay(2);  // 2ms = 500Hz
    }
}
```

**来源**: `BUGFIX_lateral_error_verification.md`, `logs/WORK_SESSION_SUMMARY_2026-07-30.md`

---

## 11. EKF模块 (EKF)

### 11.1 🟢 观测模型简化 (2026-07-30)

**变更**: 从3观测减少到2观测

**修改前**:
```c
#define SD_EKF_OBSERVATION_COUNT 3U

observation[0] = v_encoder;           // 编码器速度
observation[1] = omega_encoder;       // 编码器差速计算的角速度
observation[2] = imu.gyro_radps[2];   // IMU陀螺仪角速度 ← 已移除
```

**修改后**:
```c
#define SD_EKF_OBSERVATION_COUNT 2U

observation[0] = v_encoder;           // 编码器速度
observation[1] = omega_encoder;       // 编码器差速计算的角速度
// IMU陀螺仪观测已移除
```

**原因**: 
- 避免低成本IMU（如MPU6050）的零偏漂移（典型>0.1 deg/s）
- 简化参数调优（减少1个观测噪声参数）
- 提高计算效率（2×2矩阵求逆 vs 3×3，快30%）
- 避免传感器冲突导致的卡尔曼增益震荡

**影响**: 循迹小车在平整地面上，编码器差速法已足够准确，移除IMU反而提高稳定性

**来源**: `build/logs/EKF_ANALYSIS_AND_FIX.txt`

---

### 11.2 ⚠️ 观测噪声配置差异化

**问题**: 原实现对所有观测使用统一噪声方差（0.05），无法正确权衡不同传感器

**正确配置**:
```c
// modules/Sens-Decision/src/config.c
ekf_config.observation_noise_diag[0] = 0.03f;  // v (encoder) - 更精确
ekf_config.observation_noise_diag[1] = 0.08f;  // ω (encoder diff) - 噪声较大
```

**调优指导**:
- 如果v跟踪响应慢 → 减小 `observation_noise_diag[0]`（增加编码器权重）
- 如果v估计抖动严重 → 增大 `observation_noise_diag[0]`（增加滤波）
- 如果theta漂移 → 减小 `observation_noise_diag[1]`（增加角速度观测权重）
- 如果转向响应慢 → 减小 `observation_noise_diag[1]`

**来源**: `build/logs/EKF_ANALYSIS_AND_FIX.txt`

---

### 11.3 ⚠️ 如何恢复3观测模型

如果未来需要重新启用IMU陀螺仪融合（例如使用高精度IMU）：

**步骤**:
1. `config.h`: `SD_EKF_OBSERVATION_COUNT` 改回 `3U`
2. `state_evaluate.c`: 恢复 `observation[2] = frame->imu.gyro_radps[2];`
3. `EKF.c`: 恢复3×5观测矩阵H和3×3矩阵求逆
4. `config.c`: 配置3个观测噪声（需标定IMU vs 编码器权重）

**标定方法**:
```
固定转速转圈（1 rad/s） → 记录encoder_omega和imu_omega
计算标准差: σ_enc, σ_imu
设置噪声比例: R[1][1]/R[2][2] = (σ_enc/σ_imu)²
```

**来源**: `build/logs/EKF_ANALYSIS_AND_FIX.txt` 第六节

---

## 12. 内存安全 (Memory Safety)

### 12.1 🟢 栈溢出预防 (2026-07-30)

**问题**: EKF矩阵运算在栈上分配大量临时变量（1200+字节）

**修复方案**: 
1. 移动16个大型矩阵到静态存储（总计932字节）
2. 增加defaultTask栈大小：2048 → 3072字节（512 → 768 words）
3. 添加运行时栈水位标记监控

**静态矩阵列表** (`modules/Sens-Decision/src/EKF.c`):
```c
static float s_F[5][5];              // ekf_predict: 100 bytes
static float s_H[2][5];              // ekf_update: 40 bytes
static float s_K[5][2];              // Kalman gain: 40 bytes
// ... 共16个矩阵，932字节
```

**线程安全性**: 
- ✅ 安全：只有一个任务（defaultTask）在50Hz调用EKF
- ⚠️ 警告：不要从多个线程/ISR调用EKF函数（不是线程安全的）

**验证方法**:
```c
// 查看控制台输出
// [FreeRTOS] defaultTask stack high water mark: XXX bytes remaining
```

如果剩余栈空间<512字节，需要进一步增加栈大小。

**来源**: Agent-2 栈分析报告, `modules/Sens-Decision/src/EKF.c` 头部注释

---

### 12.2 ⚠️ FreeRTOS任务栈配置

**位置**: `Core/Src/freertos.c`

**正确配置**:
```c
osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 768 * 4,  // 3072字节 (2026-07-30修正)
  .priority = (osPriority_t) osPriorityNormal,
};
```

**历史问题**:
- 原配置：512 words（2048字节）→ 栈溢出风险
- 新配置：768 words（3072字节）→ 安全余量

**调试建议**:
```c
// 在任务中添加栈监控
UBaseType_t uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
printf("Stack remaining: %lu bytes\r\n", uxHighWaterMark * 4);
```

**来源**: `Core/Src/freertos.c`, Agent-2报告

---

## 13. 控制频率优化 (Control Frequency)

### 13.1 🟢 分层频率架构 (2026-07-30)

**优化**: PID执行频率从500Hz降至100Hz

**架构对比**:

修改前（统一500Hz）:
```
500 Hz 主循环:
  ├─ Encoder_Poll() - 500Hz
  ├─ MotionControl_Update() [PID] - 500Hz ❌ 过于频繁
  └─ EKF/Perception - 50Hz (每10周期)
```

修改后（分层架构）:
```
500 Hz 主循环:
  ├─ Encoder_Poll() - 500Hz ✓ (每次)
  ├─ MotionControl_Update() [PID] - 100Hz ✓ (每5次)
  └─ EKF/Perception - 50Hz ✓ (每10次)
```

**原理**: 
- 电机PWM响应时间约10ms → 100Hz PID已充分
- 编码器保持500Hz高频采样 → 确保速度估计精度
- 500Hz PID执行造成不必要的CPU开销（浪费80%计算）

**性能提升**:
- PID计算量减少80%（500次/秒 → 100次/秒）
- CPU空闲时间增加
- 控制质量不降低（100Hz对10ms响应时间足够）

**来源**: `docs/FREQUENCY_OPTIMIZATION_2026-07-30.txt`

---

### 13.2 ⚠️ 参数配置变更

**位置**: `modules/MotionControl/inc/motion_config.h`

**新增参数**:
```c
#define MAIN_LOOP_FREQ_HZ       500    // 编码器采样频率
#define PID_CONTROL_FREQ_HZ     100    // PID执行频率
#define PID_CONTROL_PERIOD_S    0.01f  // PID周期（10ms）
#define PID_CONTROL_DIVIDER     5      // 主循环每5次执行1次PID
```

**重要变更**:
- `CONTROL_PERIOD_S` 现在是 0.01s（而非0.002s）
- 所有PID积分/微分计算使用10ms时间步
- 加速度限制使用10ms时间步

**影响**: 如果手动调整了PID参数（Kp/Ki/Kd），需要重新调优：
- Ki可能需要增大约5倍（因为更新频率降低5倍）
- Kd可能需要减小约5倍

**来源**: `docs/FREQUENCY_OPTIMIZATION_2026-07-30.txt` 第3节

---

### 13.3 ⚠️ 代码调用变更

**位置**: `Core/Src/app/track_control_app.c`

**调用逻辑**:
```c
void TrackControlApp_RunFastCycle(void) {
    static uint8_t cycle_counter = 0;
    
    // 每周期执行: 编码器采样 (500Hz)
    Encoder_Poll();
    
    // 每5周期执行: PID控制 (100Hz)
    if (cycle_counter % 5 == 0) {
        MotionControl_Update(CONTROL_PERIOD_S);  // dt = 0.01s
    }
    
    // 每10周期执行: EKF/感知/决策 (50Hz)
    if (cycle_counter % 10 == 0) {
        // EKF pipeline
    }
    
    cycle_counter++;
    if (cycle_counter >= 10) cycle_counter = 0;
}
```

**注意事项**:
- 主循环仍然是500Hz（每2ms调用一次）
- 不要修改 `osDelay(2)` 的值
- `MotionControl_Update()` 接收的dt参数是0.01s（而非0.002s）

**来源**: `Core/Src/app/track_control_app.c`, `docs/FREQUENCY_OPTIMIZATION_2026-07-30.txt`

---

## 14. 参数调优建议

### 11.1 分阶段调试

> **v1.3.0 更新**: 曲线速度和接近弯道速度已移除。速度现由 `speed_error_gain` 根据横向偏差连续调节。

**第1阶段：验证基本功能（低速）**:
```c
speed_mode_set(SPEED_MODE_DEBUG);  // 0.2 m/s, speed_error_gain = 0.3
// 目标：稳定完成1圈（先关航向修正）
```

**第2阶段：提升性能（中速）**:
```c
speed_mode_set(SPEED_MODE_SLOW);   // 0.5 m/s, speed_error_gain = 0.3
// 目标：稳定完成10圈
```

**第3阶段：冲击极限（高速）**:
```c
speed_mode_set(SPEED_MODE_NORMAL); // 1.0 m/s, speed_error_gain 可微调
// 目标：最短时间完成20圈
```

### 11.2 关键参数速查

| 参数 | 值 | 说明 |
|------|-----|------|
| ENCODER_PPR | 60000 | 500 PPR × 4 × 30 |
| WHEEL_RADIUS | 0.033 m | 33mm（需实测标定） |
| WHEEL_BASE | 0.214 m | 214mm（实测） |
| IR_CENTER_X | 0.183 m | 183mm 前方 |
| IR_CENTER_Y | 0.0 m | 横向居中 |
| USART2_IRQ | Priority 3 | 高速传感器 |
| IR传感器帧率 | 88-125 Hz | 115200 bps |

### 11.3 调试工具速查

| 模块 | 工具 | 用途 |
|------|------|------|
| 电机 | motor_speed_test.c | 分级速度测试 |
| 电机 | motor_direction_calibration.c | 方向校准 |
| 电机 | motor_hw_diagnostic.c | TB6612硬件诊断 |
| 编码器 | encoder_diagnostic.c | 编码器诊断 |
| IR传感器 | ir_uart_diagnostic.c | UART接收统计 |
| IR传感器 | ir_sensor_test.c | 传感器通信测试 |
| IR传感器 | ir_raw_capture.c | 原始数据捕获 |
| IMU | imu_debug.c | IMU数据监控 |
| IMU | spi_diagnostic.c | SPI通信诊断 |
| 综合 | calibration_tool.c | 综合校准 |

**使用方式**: 在 `freertos.c::StartDefaultTask()` 中调用对应测试函数。

---

---

## 14. 传感器配置陷阱 (Sensor Configuration)

### 14.1 🔴 四轮→双轮迁移不完整 (2026-07-30 已修复)

**问题**: 编码器配置按四轮填充，但系统只有2个编码器

**症状**:
- `sensors_init_all()` 必然失败
- 配置验证检测到编码器索引重复
- 数组越界访问 `g_encoder_private[2]` 和 `[3]`
- 状态估计器速度计算出错

**根因**:
- 编码器枚举定义了4个：`ENCODER_LEFT_FRONT`, `ENCODER_LEFT_REAR`, `ENCODER_RIGHT_FRONT`, `ENCODER_RIGHT_REAR`
- 但系统只有2个实际编码器：左轮、右轮
- 传感器初始化表配置了6个对象（4个编码器 + IMU + IR），但数组越界

**修复**:
1. 编码器枚举：4个→2个（`ENCODER_LEFT`, `ENCODER_RIGHT`）
2. 引入 `INVALID_ENCODER_INDEX` 标记未使用位置
3. 传感器初始化表：6个→4个对象
4. 配置验证：检查编码器索引有效性
5. 状态估计器：使用2编码器计算速度

**修复文件**: `config.h`, `interface.h`, `interface.c`, `config.c`, `state_evaluate.c`

**来源**: Agent-8修复报告 (Phase 1 P0)

---

## 15. 红外传感器算法陷阱 (IR Sensor Algorithm)

### 15.1 🔴 阈值判断完全反向 (2026-07-30 已修复)

**问题**: `if (raw > threshold)` 判断黑线，但黑线ADC值更低

**数据特征**:
- 白色背景：ADC值约270
- 黑色线条：ADC值约100

**错误逻辑**:
```c
// ❌ 完全反向的判断
if (ir_raw[i] > 0.5f) {
    // 认为检测到黑线
    // 但实际上 raw > 0.5 表示白色！
}
```

**后果**:
- 所有传感器永远激活（白色背景被误判为黑线）
- 系统一直判定为"路口"（8个传感器全亮）
- 质心计算被白色区域主导
- 循迹完全失效，准确率0%

**修复**: 黑线强度反转算法
```c
// ✅ 正确的黑线检测
float black_strength[8];
for (int i = 0; i < 8; i++) {
    black_strength[i] = white_reference[i] - ir_raw[i];  // 反转
    if (black_strength[i] > threshold) {
        // 检测到黑线
    }
}
```

**必须校准**:
1. **白平衡校准**：`IrCalibration_WhiteBalance()` - 在白色背景上采集参考值
2. **阈值校准**：`IrCalibration_BlackThreshold()` - 在黑线上确定阈值

**校准参数** (新增到 `config.h` 和 `config.c`):
- `white_reference[8]` - 白色参考值
- `black_strength_threshold` - 黑线强度阈值

**验证**: 检测准确率从0%提升至>95%

**修复文件**: `perception.c`, `config.h`, `config.c`, `ir_calibration.c/h`, `perception_debug.c/h`

**来源**: Agent-10修复报告 (Phase 2 P1)

---

## 16. 速度配置陷阱 (Speed Configuration)

### 16.1 🔴 应用层速度配置未生效 (2026-07-30 已修复)

**问题**: 应用层和决策层使用独立的速度配置变量

**配置流向**:
```
应用层 (track_control_app.c):
  g_track_config.line_speed_mps = 0.5f     ← 设置了
  g_track_config.curve_speed_mps = 0.3f    ← 设置了
  ↓
  ❌ 未传递 ❌
  
决策层 (config.c):
  g_sens_decision_config.behavior.line_speed_mps = 1.0f   ← 实际使用
  g_sens_decision_config.behavior.curve_speed_mps = 0.5f  ← 实际使用
```

**症状**:
- 应用层配置：0.5/0.3 m/s（安全调试速度）
- 实际运行：1.0/0.5 m/s（高速！）
- 首次调试速度过快，可能冲出轨道

**根因**: 行为规划器从 `g_sens_decision_config.behavior.*` 读取速度，应用层配置完全未传递到这里

**修复**: 速度模式系统
```c
// 新增 speed_mode.c/h
speed_mode_set(SPEED_MODE_DEBUG);   // 0.2 m/s (首次调试，超低速)
speed_mode_set(SPEED_MODE_SLOW);    // 0.5 m/s (常规调试)
speed_mode_set(SPEED_MODE_NORMAL);  // 1.0 m/s (正常运行)
speed_mode_set(SPEED_MODE_FAST);    // 1.5 m/s (竞速模式)
```

**速度模式配置表** (v1.3.0 更新):

| 模式 | 直线速度 | 使用场景 |
|------|----------|----------|
| DEBUG | 0.2 m/s | 首次调试，验证传感器 |
| SLOW | 0.5 m/s | 常规调试，PID调优 |
| NORMAL | 1.0 m/s | 正常运行 |
| FAST | 1.5 m/s | 竞速模式 |

> **v1.3.0 注意**: 弯道速度由 `speed_error_gain` (默认0.3) 根据横向偏差实时调节，不再需要预设弯道速度。
> 速度公式: `speed = line_speed * clamp(1.0 - 0.3 * |lateral_error|, 0.4, 1.0)`

**使用方法**:
```c
// 在 track_control_app.c::TrackControlApp_Init() 中
speed_mode_set(SPEED_MODE_DEBUG);  // ← 修改这一行即可切换速度
```

**修复文件**: `track_control_app.c`, `speed_mode.c/h`, `CMakeLists.txt`

**来源**: Agent-11修复报告 (Phase 2 P1)

---

## 总结：最危险的坑

### 原有问题 (v1.0-v1.1)
1. **电机PWM 10倍误差** - `MOTOR_SPEED_MAX` 设1000导致只有10%占空比
2. **编码器PPR差了38.5倍** - 误以为13PPR，实际500PPR
3. **USART2 HAL双重读取** - 手动读DR后又调用HAL，导致中断禁用
4. **IR传感器Process()未被调用** - 数据中断接收了但永远解析不出来
5. **printf静默禁用USART2中断** - 每次printf后RXNEIE可能丢失
6. **EKF观测模型配置不当** - 低成本IMU引入漂移，应简化为2观测模型
7. **FreeRTOS栈溢出** - EKF矩阵运算需1200+字节栈，必须增加任务栈大小
8. **控制频率过高** - 500Hz PID浪费80%计算，应降至100Hz

### 新增问题 (v1.2.0)
9. **四轮→双轮迁移不完整** - 编码器配置4个但只有2个，导致数组越界
10. **初始化失败后继续运行** - 传感器故障但控制循环启动，违反fail-safe原则
11. **红外传感器阈值判断反向** - 黑线检测完全失效，准确率0%
12. **速度配置未传递** - 应用层配置0.5m/s，实际运行1.0m/s

---

**指南创建时间**: 2026-07-30
**最后更新**: 2026-07-30 (v1.3.0 - 移除曲线/弯道速度建议，更新分阶段调试指南)
**数据来源**: logs/ 目录下 35+ 个调试日志 + build/logs/ 分析报告 + Agent-8/9/10/11修复报告
**创建者**: Claude (Opus 4.8) + 用户 Joelin
**版本**: v1.3 (移除过时的曲线检测调优建议，更新速度模式表)
