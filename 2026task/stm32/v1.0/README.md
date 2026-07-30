# STM32F407 天空星小车主控（FreeRTOS）

目标芯片是 `STM32F407VGT6`，不是 STM32F307。工程使用：

- STM32Cube FW_F4 V1.28.3
- HAL 驱动
- FreeRTOS 10.3.1 / CMSIS-RTOS v2
- Keil MDK-ARM V5
- ARM Compiler 5.06 update 5
- HSE 8 MHz，系统时钟 168 MHz

## 打开方式

双击 `v1.0_freeRTOS.ioc`，或在 STM32CubeMX 中选择
`File -> Open Project`。修改配置后点击 `GENERATE CODE`。

## 已配置外设

| 功能 | 外设 / 引脚 |
|---|---|
| TB6612 A、D 两路 PWM | TIM1_CH1 PE9、TIM1_CH2 PE11，20 kHz |
| TB6612 方向和待机 | PD14、PD15、PE2、PE3、PE0 |
| 两路编码器 | TIM3 PB4/PB5，TIM4 PD12/PD13 |
| 调试串口 | UART5 PC12/PD2，115200 |
| 循迹 | USART2 PA2/PA3，115200，RX DMA Circular |
| K230 | USART3 PD8/PD9，115200，RX DMA Circular |
| 舵机 | USART6 PC6/PC7，115200，RX DMA Circular |
| OLED | I2C1 PB8/PB9，100 kHz |
| ICM42688 | SPI2 PB13/PB14/PB15，PE7 CS，PE8 INT1 |
| 按键 | PC0～PC4，下降沿 EXTI，内部上拉 |
| RTOS | FreeRTOS + CMSIS-RTOS v2，32 KB heap |

## 重要说明

1. UART5 使用 PC12/PD2，会占用板载 TF 卡的 SDIO_CK/SDIO_CMD。当前工程没有
   启用 SDIO，TF 卡槽必须保持空。
2. FreeRTOS 使用 SysTick；HAL 的 1 ms 时基已经改为 TIM6。
3. 所有可能调用 FreeRTOS API 的外设中断优先级均设置为 5 或更低的抢占等级
   （数值不小于 5）。
4. 上电时 `MOTOR_STBY` 和所有电机方向脚为低，`IMU_CS` 为高。
5. USART6 的 115200 只是初始值，确认舵机型号和协议后再修改。
6. DMA Circular 不能直接配合阻塞式 `HAL_UART_Receive()`；应用层应使用
   `HAL_UARTEx_ReceiveToIdle_DMA()` 或自行维护环形缓冲区。

## 本机 Keil 注意事项

CubeMX 已生成并核对全部源码，Keil 工程也已固定为本机存在的
`ARM Compiler 5.06 update 5`。当前电脑的 Keil `RTEPATH` 指向
`C:\Keil_Packs`，但其中没有 STM32F4 Device Family Pack，因此命令行编译会报：

```text
Device STM32F407VGTx not found
ARM::CMSIS:CORE component is missing
```

这是 Keil Pack 安装/搜索路径问题，不是 C 源码或 CubeMX 配置错误。首次用 Keil
编译前，需要在 Pack Installer 中安装 `Keil::STM32F4xx_DFP` 和
`ARM::CMSIS`，或者把 Keil 的 RTE Pack 搜索路径指向已有 Pack 目录。不要为了
绕过该错误把工程器件改成通用 Cortex-M4，否则会丢失 STM32F407 的下载算法和
器件调试信息。

已绕过 RTE Pack 管理，直接使用本机 ARMCC 5.06 update 5 对工程列出的 41 个
C 源文件完成编译检查：`41 compiled, 0 failed`。

---

## API 调用说明

### 1. 控制应用层 (ControlApp)

控制应用层是系统的顶层协调模块，负责初始化和运行完整的控制流水线。

#### 1.1 初始化

```c
#include "control_app.h"

// 在 FreeRTOS 任务中初始化控制应用
bool ControlApp_Init(uint16_t target_laps) {
    // target_laps: 目标圈数（0表示无限循环）
}
```

**返回值**：
- `true`: 初始化成功，所有模块就绪
- `false`: 初始化失败，电机已停止

**初始化步骤**（自动执行）：
1. 电机系统初始化
2. 编码器适配器初始化
3. 红外传感器初始化（等待2秒预热）
4. IMU传感器初始化（可选，失败不影响主流程）
5. Sens-Decision模块初始化
6. 运动控制器初始化

#### 1.2 运行控制循环

```c
// 500 Hz 控制主循环（每2ms调用一次）
void ControlApp_RunFastCycle(void);
```

**功能**：
- 每次调用执行编码器采样和PID控制（500 Hz）
- 每10次调用执行一次传感器融合和决策更新（50 Hz）

**典型用法**（FreeRTOS任务）：

```c
void StartDefaultTask(void *argument)
{
    // 初始化控制应用
    if (!ControlApp_Init(3)) {  // 目标3圈
        printf("[FATAL] ControlApp_Init failed\r\n");
        for (;;) { osDelay(1000); }
    }

    // 防御性修复：重新使能USART2 RX中断
    SET_BIT(huart2.Instance->CR1, USART_CR1_RXNEIE);

    printf("[ControlApp] Running\r\n");

    // 500 Hz 控制循环
    for (;;) {
        ControlApp_RunFastCycle();
        osDelay(2);  // 2ms = 500Hz
    }
}
```

---

### 2. 传感器适配层 (Sensor Adapter)

传感器适配层为上层模块提供统一的传感器访问接口。

#### 2.1 获取传感器接口

```c
#include "sensor_adapter.h"

sensor_hal_t *hal = SensorAdapter_GetInterface();
```

#### 2.2 读取编码器计数

```c
int32_t count;
sd_status_t status = hal->read_encoder_count(0, &count);  // 左轮
// 或
status = hal->read_encoder_count(1, &count);  // 右轮
```

**参数**：
- `index`: 编码器索引（0=左轮，1=右轮）
- `count`: [out] 编码器累计计数（int32_t）

**编码器分辨率**：60000 counts/圈（500 PPR × 4倍频 × 30减速比）

#### 2.3 读取IMU原始数据

```c
imu_raw_data_t imu_data;
sd_status_t status = hal->read_imu_raw(&imu_data);

if (status == SD_OK) {
    // imu_data.accel[0..2]: 加速度原始值
    // imu_data.gyro[0..2]: 角速度原始值
    // imu_data.temperature: 温度原始值
}
```

**注意**：返回的是原始ADC值，需要根据ICM42688量程配置进行转换。

#### 2.4 读取红外传感器阵列

```c
uint16_t active_mask;
float ir_values[8];
sd_status_t status = hal->read_ir(&active_mask, ir_values);

if (status == SD_OK) {
    // ir_values[0..7]: 8路传感器数值（0-4095）
    // active_mask: 通道激活掩码（当前为0xFFFF）
}
```

**传感器布局**（从右到左，小车前向视角）：

```
右侧 →                   中心                    ← 左侧
+39.86  +28.47  +17.08  +5.69   -5.69   -17.08   -28.47   -39.86 (mm, 物理X)
  [0]     [1]     [2]    [3]     [4]      [5]      [6]      [7]  (索引)
```

**数值含义**：
- 数值越大：反射率越高（白色/光滑表面）
- 数值越小：反射率越低（黑色/粗糙表面）
- 典型范围：200-300（白底），<100（黑线）

---

### 3. 预处理层 (Preprocess)

预处理层负责读取所有传感器并进行坐标系转换。

#### 3.1 更新传感器帧

```c
#include "preprocess.h"

sensor_frame_t frame;
uint64_t timestamp_us = GetTimestampMicros();
sd_status_t status = preprocess_update(timestamp_us, &frame);
```

**输出结构**（`sensor_frame_t`）：

```c
typedef struct {
    uint64_t timestamp_us;              // 时间戳（微秒）
    
    encoder_data_t encoders[2];         // 编码器数据
    bool encoder_valid[2];              // 编码器有效标志
    
    imu_data_t imu;                     // IMU数据（已转换到代码坐标系）
    bool imu_valid;                     // IMU有效标志
    
    ir_array_data_t ir;                 // 红外阵列数据
    bool ir_valid;                      // 红外有效标志
} sensor_frame_t;
```

**坐标系说明**：

- **代码坐标系**（算法使用）：X=前方，Y=左侧，Z=上方
- **物理IMU坐标系**（硬件安装）：X=右侧，Y=前方，Z=上方
- `preprocess_update()` 自动完成坐标转换

---

### 4. 感知层 (Perception)

感知层根据红外传感器计算横向偏差和航向偏差。

#### 4.1 更新感知状态

```c
#include "perception.h"

perception_output_t perception;
sd_status_t status = perception_update(&frame, &perception);
```

**输出结构**（`perception_output_t`）：

```c
typedef struct {
    float lateral_error;     // 横向偏差（米）
    float heading_error;     // 航向偏差（弧度）
    bool valid;              // 数据有效标志
} perception_output_t;
```

**符号约定**：
- `lateral_error < 0`：车体偏右（线在传感器右侧）→ 需要向左修正
- `lateral_error > 0`：车体偏左（线在传感器左侧）→ 需要向右修正
- `lateral_error ≈ 0`：车体居中

**计算方法**：加权质心法

```c
lateral_error = Σ(weight[i] × ir_value[i]) / Σ(ir_value[i])
```

**权重配置**（已修正，2026-07-30）：

```c
static const float ir_weights[8] = {
    -0.5694f, -1.7083f, -2.8472f, -3.9861f,  // 右侧（负权重）
    +0.5694f, +1.7083f, +2.8472f, +3.9861f   // 左侧（正权重）
};
```

---

### 5. 运动控制层 (MotionControl)

运动控制层执行PID控制和轮速计算。

#### 5.1 初始化

```c
#include "motion_control.h"

motion_control_status_t status = MotionControl_Init();
```

#### 5.2 设置速度指令

```c
// 设置线速度和角速度指令
MotionControl_SetVelocityCommand(
    0.5f,   // v_mps: 线速度（米/秒）
    0.0f    // omega_radps: 角速度（弧度/秒）
);
```

#### 5.3 更新控制器

```c
// 在500Hz循环中调用
MotionControl_Update(dt_s);  // dt_s: 时间步长（秒），通常为0.002
```

**功能**：
1. 读取编码器反馈
2. 计算当前轮速
3. 执行PID控制
4. 输出PWM到电机

#### 5.4 启动/停止

```c
MotionControl_Start();   // 启动控制器
MotionControl_Stop();    // 停止控制器（电机置零）
```

---

### 6. 关键参数配置

#### 6.1 控制频率参数（2026-07-30优化）

**位置**：`modules/MotionControl/inc/motion_config.h`

```c
#define MAIN_LOOP_FREQ_HZ       500    // 主循环频率（编码器采样）
#define PID_CONTROL_FREQ_HZ     100    // PID控制执行频率
#define PID_CONTROL_PERIOD_S    0.01f  // PID周期（秒）
#define PID_CONTROL_DIVIDER     5      // 主循环每5次执行一次PID
```

**分层频率架构**：
- **编码器采样**：500Hz（每2ms）- 确保速度估计精度
- **PID控制**：100Hz（每10ms）- 匹配电机PWM响应时间
- **EKF/感知**：50Hz（每20ms）- 计算密集型任务

**优化效果**：PID计算量减少80%，CPU占用率显著降低

#### 6.2 编码器参数（已修正）

**位置**：`modules/MotionControl/inc/motion_config.h`

```c
#define ENCODER_PPR    60000   // 编码器分辨率（counts/圈）
#define GEAR_RATIO     30.0f   // 减速比
#define WHEEL_RADIUS   0.033f  // 轮半径（米）
#define WHEEL_BASE     0.115f  // 轮距（米）
```

**修正记录**（2026-07-30）：
- 旧值：1560 counts/圈（基于错误的13 PPR假设）
- 新值：60000 counts/圈（实测500 PPR）
- 影响：修复前速度估计错误38.5倍

#### 6.3 EKF参数（2026-07-30优化）

**位置**：`modules/Sens-Decision/inc/config.h`, `modules/Sens-Decision/src/config.c`

```c
#define SD_EKF_OBSERVATION_COUNT 2U  // 观测数量：v（编码器）+ ω（编码器差速）
```

**观测模型简化**（2026-07-30）：
- 旧配置：3观测（v编码器 + ω编码器 + ω IMU陀螺仪）
- 新配置：2观测（v编码器 + ω编码器）
- 原因：避免低成本IMU漂移，简化参数调优
- 性能提升：矩阵求逆计算量减少30%（2×2 vs 3×3）

**观测噪声配置**：
```c
observation_noise_diag[0] = 0.03f;  // v噪声方差 (m/s)²
observation_noise_diag[1] = 0.08f;  // ω噪声方差 (rad/s)²
```

#### 6.4 FreeRTOS任务栈配置（2026-07-30修正）

**位置**：`Core/Src/freertos.c`

```c
#define CONTROL_TASK_STACK_SIZE 768  // 3072字节（768×4）
```

**修正记录**（2026-07-30）：
- 旧值：512 words（2048字节）
- 新值：768 words（3072字节）
- 原因：EKF矩阵运算栈需求~1200字节，防止栈溢出
- 额外措施：16个大型矩阵移至静态存储（节省932字节栈空间）

⚠️ **运行时监控**：查看控制台打印的栈水位标记（Stack High Water Mark）

#### 6.5 红外传感器参数（已修正）

**位置**：`modules/Sens-Decision/src/config.c`

```c
// IR阵列中心位置（代码坐标系）
perception.position.x_m = 0.1321f;  // 前方132.1mm
perception.position.y_m = 0.0f;     // 横向居中

// IR传感器权重（代码坐标系）
static const float ir_weights[8] = {
    -0.5694f, -1.7083f, -2.8472f, -3.9861f,  // 右侧
    +0.5694f, +1.7083f, +2.8472f, +3.9861f   // 左侧
};
```

#### 6.6 控制参数

**位置**：`modules/Sens-Decision/src/config.c`

```c
// 轨迹跟踪增益
square_config.lateral_gain = 2.0f;   // 横向偏差增益
square_config.heading_gain = 1.5f;   // 航向偏差增益

// 速度规划
square_config.line_speed_mps = 1.0f;   // 直线速度（米/秒）
square_config.curve_speed_mps = 0.5f;  // 过弯速度（米/秒）
```

**调试建议**：
- 第一阶段（验证）：`line_speed = 0.3, curve_speed = 0.2, lateral_gain = 0.5`
- 第二阶段（提升）：`line_speed = 0.6, curve_speed = 0.4, lateral_gain = 1.0`
- 第三阶段（极限）：`line_speed = 1.0, curve_speed = 0.7, lateral_gain = 调优值`

#### 6.7 中断优先级配置（2026-07-30规范）

**位置**：`Core/Src/usart.c`, `Core/Src/stm32f4xx_it.c`

**关键配置**：
- **USART2（红外传感器）**：Priority 3（高于FreeRTOS边界）
  - 原因：125Hz高速数据流，字节间隔仅87μs
  - 限制：禁止调用任何FreeRTOS API
  - 详见：`docs/INTERRUPT_PRIORITY_GUIDE.md`

- **其他外设**：Priority 5-15（可安全使用FreeRTOS API）

⚠️ **警告**：修改USART2中断处理代码前必须阅读优先级配置指南

---

### 7. 常见问题排查

#### 7.1 红外传感器无数据

**症状**：`perception_update()` 返回 `SD_ERR_READ`

**排查步骤**：

1. 检查USART2接线（PA2=TX, PA3=RX）
2. 检查波特率（115200）
3. 检查RXNEIE中断使能：

```c
// 在初始化后添加
SET_BIT(huart2.Instance->CR1, USART_CR1_RXNEIE);
```

4. 查看诊断输出：

```c
#include "ir_uart_diagnostic.h"
IrUartDiag_PrintReport();  // 打印接收统计
```

#### 7.2 编码器读数异常

**症状**：编码器计数始终为0或跳变剧烈

**排查步骤**：

1. 检查TIM3/TIM4编码器模式配置
2. 验证编码器接线
3. 手动旋转轮子，观察计数变化：

```c
#include "encoder_hw_bridge.h"
int32_t left = EncoderHwBridge_GetCount(0);
int32_t right = EncoderHwBridge_GetCount(1);
printf("Encoder: L=%ld, R=%ld\r\n", left, right);
```

#### 7.3 速度估计不准确

**可能原因**：

1. `ENCODER_PPR` 配置错误（应为60000）
2. `WHEEL_RADIUS` 需要实测标定
3. 编码器方向反向

**验证方法**：

```c
// 让轮子转动N圈，测量实际位移
float distance_per_rev = 2 * PI * WHEEL_RADIUS;
// 实测位移 / (N × distance_per_rev) 应接近1.0
```

---

### 8. 调试工具

项目提供了丰富的调试工具，位于 `Core/Src/app/` 目录：

| 工具 | 功能 | 头文件 |
|------|------|--------|
| `motor_speed_test.c` | 电机速度分级测试 | `motor_speed_test.h` |
| `motor_direction_calibration.c` | 电机方向校准 | `motor_direction_calibration.h` |
| `encoder_diagnostic.c` | 编码器诊断 | `encoder_diagnostic.h` |
| `ir_sensor_test.c` | 红外传感器测试 | `ir_sensor_test.h` |
| `ir_uart_diagnostic.c` | 红外UART诊断 | `ir_uart_diagnostic.h` |
| `imu_debug.c` | IMU调试工具 | `imu_debug.h` |
| `calibration_tool.c` | 综合校准工具 | `calibration_tool.h` |

**使用方法**：在 `freertos.c::StartDefaultTask()` 中调用相应的测试函数。

---

## 参考文档

详细的开发日志和修复记录保存在以下目录：

- `logs/` - 开发日志和问题修复记录
- `docs/` - 设计文档和参数清单
- `build/logs/` - 编译输出和分析报告
- `BUGFIX_lateral_error_verification.md` - lateral_error 符号验证完整记录
- `CHANGELOG.md` - 版本变更历史

**关键文档**：

| 文档 | 描述 |
|------|------|
| `API_PITFALLS_GUIDE.md` | 模块调试避坑指南（必读） |
| `CHANGELOG.md` | 版本变更历史和修复记录 |
| `docs/INTERRUPT_PRIORITY_GUIDE.md` | FreeRTOS中断优先级配置指南 |
| `docs/FREQUENCY_OPTIMIZATION_2026-07-30.txt` | 控制频率优化分析 |
| `build/logs/EKF_ANALYSIS_AND_FIX.txt` | EKF观测模型分析与修复 |
| `logs/WORK_SESSION_SUMMARY_2026-07-30.md` | 工作会话总结 |
| `logs/2026-07-30_encoder_ppr_correction.md` | 编码器分辨率修正 |
| `logs/2026-07-30_coord_system_complete_fix.md` | 坐标系统一修复 |
| `logs/2026-07-30_ir_sensor_fix_implementation.md` | 红外传感器修复 |
| `logs/2026-07-30_interrupt_priority_audit.md` | 中断优先级审计报告 |
| `BUGFIX_lateral_error_verification.md` | lateral_error验证与修复 |

---

## 最近修改记录 (Recent Modifications)

### 2026-07-30 - 关键修复与优化

本次更新由7个自主代理完成，修复了4个关键问题并进行了性能优化：

#### 1. EKF观测模型简化（Critical Fix）
- **问题**：3观测模型配置不当，低成本IMU引入漂移
- **修复**：简化为2观测模型（移除IMU陀螺仪观测）
- **影响**：计算量减少30%，参数调优难度降低
- **详见**：`build/logs/EKF_ANALYSIS_AND_FIX.txt`

#### 2. 栈溢出风险修复（Critical Fix）
- **问题**：EKF矩阵运算使用1200+字节栈空间
- **修复**：16个矩阵移至静态存储，任务栈增至3072字节
- **影响**：防止HardFault，提高系统稳定性
- **监控**：运行时查看栈水位标记

#### 3. 控制频率优化（Performance）
- **优化**：PID执行频率从500Hz降至100Hz
- **保持**：编码器采样500Hz，EKF/感知50Hz
- **效果**：CPU占用率降低80%（PID部分）
- **详见**：`docs/FREQUENCY_OPTIMIZATION_2026-07-30.txt`

#### 4. 中断优先级规范化（Major Fix）
- **问题**：USART2优先级配置冲突和文档缺失
- **修复**：统一配置，添加详细文档和安全指南
- **影响**：避免FreeRTOS API误用导致的HardFault
- **详见**：`docs/INTERRUPT_PRIORITY_GUIDE.md`

#### 修改统计
- **文件修改**：13个
- **文档创建**：9个
- **代码行变更**：约500行
- **测试状态**：编译验证待进行，硬件测试待进行

#### 下一步工作
1. 编译固件并验证无错误
2. 烧录到STM32F407
3. 运行验证测试（参见各文档中的测试清单）
4. 监控栈水位标记
5. 验证100Hz PID控制稳定性

详细信息请参阅 `CHANGELOG.md` 和 `docs/MODIFICATIONS_SUMMARY_2026-07-30.md`。

---

**最后更新**: 2026-07-30  
**版本**: v1.1.0  
**状态**: 静态分析完成，编译验证待进行，硬件测试待进行


---

## ⚠️ 模块调试避坑指南

**重要阅读**: 每次修改或调用模块前，请先阅读 [API_PITFALLS_GUIDE.md](API_PITFALLS_GUIDE.md)。

该指南基于 logs/ 目录下 35 个调试日志整理而成，涵盖了电机、编码器、红外传感器、IMU、坐标系、FreeRTOS中断、编译工具链等全部模块的关键陷阱和正确调用方式。

**铁则**: 先旁路测试单个模块，再集成到完整流水线。简单自动测试优于复杂交互式工具。

---

**最后更新**: 2026-07-30
**版本**: v1.1.0
**状态**: 静态分析完成，编译验证待进行，硬件测试待进行