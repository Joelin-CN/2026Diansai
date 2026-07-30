# UART0调试与软件测试模式开发日志

**日期**: 2026-07-23  
**目标**: 为MSPM0G3507控制器固件添加UART0串口调试功能，并实现纯软件算法测试模式

---

## 1. 项目背景

### 1.1 初始状态
- 项目基于MSPM0G3507 MCU，使用FreeRTOS
- 包含多个算法模块：Sens-Decision、Motion Control、ICM42688、MCP23017
- UART0已在SysConfig中配置（115200波特率，TX=PA10，RX=PA11）
- 原始代码中使用`printf()`但未重定向，无串口输出

### 1.2 需求
1. 实现printf到UART0的重定向，用于调试输出
2. 验证Sens-Decision模块的`SD_LOG_*()`宏能正常输出
3. 创建纯软件测试模式，不依赖任何硬件外设进行算法验证

---

## 2. UART0串口调试实现

### 2.1 UART0使用情况确认

**检查命令**:
```bash
grep -rn "UART0\|DL_UART" modules/ src/ inc/ --include="*.c" --include="*.h"
```

**结论**: ✅ UART0完全未被占用，可以安全使用作为调试输出

### 2.2 创建UART调试模块

#### 文件结构
```
inc/uart_debug.h       # UART调试接口
src/uart_debug.c       # UART调试实现
```

#### uart_debug.h - 接口定义
```c
void UartDebug_Init(void);
void UartDebug_PutChar(char c);
void UartDebug_Print(const char *str);
void UartDebug_Println(const char *str);
void UartDebug_PrintU32(uint32_t value);
void UartDebug_PrintI32(int32_t value);
void UartDebug_PrintHex(uint32_t value, bool prefix);
void UartDebug_PrintFloat(float value);
```

#### uart_debug.c - 核心实现

**基础输出函数**:
```c
void UartDebug_PutChar(char c)
{
    while (DL_UART_isTXFIFOFull(UART0_INST));
    DL_UART_Main_transmitData(UART0_INST, c);
}
```

### 2.3 printf重定向（microLIB）

项目使用Arm Compiler 6 + microLIB，需要实现`fputc()`函数：

```c
#if defined(__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050)

int fputc(int ch, FILE *f)
{
    (void)f;
    UartDebug_PutChar((char)ch);
    return ch;
}

int ferror(FILE *f)
{
    (void)f;
    return 0;
}

#endif
```

**关键点**:
- microLIB使用`fputc()`而非`_sys_write()`
- 必须同时实现`ferror()`
- 所有`printf()`调用自动重定向到UART0

### 2.4 编译配置

**eide.yml配置**:
```yaml
global:
  output-debug-info: enable
  use-microLIB: true  # 使用microLIB
```

### 2.5 测试验证

#### 测试代码
```c
int main(void)
{
    SYSCFG_DL_init();
    
    /* Direct hardware test */
    while (DL_UART_isTXFIFOFull(UART0_INST));
    DL_UART_Main_transmitData(UART0_INST, 'X');
    
    /* UartDebug test */
    UartDebug_Println("Test");
    
    /* printf test */
    printf("Counter: %u\n", counter);
}
```

#### 测试结果
```
X
A
Test
Counter: 0
Counter: 1
Counter: 2
...
```

✅ **所有输出方式正常工作**

---

## 3. 软件测试模式开发

### 3.1 设计思路

使用**条件编译**实现两种模式：
- **硬件模式** (`SOFTWARE_TEST_MODE = 0`): 完整的硬件初始化和真实传感器读取
- **软件测试模式** (`SOFTWARE_TEST_MODE = 1`): 跳过硬件，生成模拟数据，验证算法逻辑

### 3.2 实现方案

#### 控制开关
在 `src/control_app.c` 顶部添加：
```c
/* Software-only test mode: skip all hardware initialization */
#define SOFTWARE_TEST_MODE 1
```

#### ControlApp_Init() 修改

```c
bool ControlApp_Init(uint8_t target_laps) {
#if SOFTWARE_TEST_MODE
    /* 软件测试模式 - 只初始化算法 */
    printf("[ControlApp] *** SOFTWARE TEST MODE - NO HARDWARE ***\n");
    
    // 清空所有状态
    memset(&g_motion_control, 0, sizeof(g_motion_control));
    memset(&g_state_evaluator, 0, sizeof(g_state_evaluator));
    // ... 其他结构体
    
    // 验证目标圈数
    if (target_laps < 1 || target_laps > 5) {
        return false;
    }
    g_target_laps = target_laps;
    
    // 加载Sens-Decision配置
    sd_config_reset_defaults();
    
    // 初始化算法模块
    state_evaluator_init(&g_state_evaluator, &g_sens_decision_config.ekf);
    perception_init(&g_perception);
    behavior_planner_init(&g_behavior_planner);
    
    // 初始化轨迹生成器
    trajectory_generator_init(&g_trajectory_generator, &g_sens_decision_config.trajectory);
    if (trajectory_set_path(&g_trajectory_generator, SquarePath_GetPoints(),
                           SquarePath_GetPointCount()) != SD_OK) {
        return false;
    }
    
    // 配置路径参数
    g_square_config.lateral_gain = 2.0f;
    g_square_config.heading_gain = 1.5f;
    g_square_config.max_omega_radps = 2.0f;
    g_square_config.target_laps = target_laps;
    
    printf("[ControlApp] *** Algorithm initialization complete! ***\n");
    return true;

#else
    /* 硬件模式 - 完整初始化 */
    Motor_Init();
    Motor_Stop();
    // ... 完整的硬件初始化流程
#endif
}
```

#### ControlApp_RunFastCycle() 修改

```c
void ControlApp_RunFastCycle(void) {
#if SOFTWARE_TEST_MODE
    static uint32_t log_counter = 0;

    if ((g_cycle_counter % 10U) == 0U) {
        const float dt = 0.020f;

        /* 生成模拟传感器数据 */
        g_sensor_frame.timestamp_us = g_cycle_counter * 2000ULL;

        /* 编码器数据 */
        for (int i = 0; i < SD_ENCODER_COUNT; i++) {
            g_sensor_frame.encoders[i].count = 0;
            g_sensor_frame.encoders[i].speed_mps = 0.0f;
            g_sensor_frame.encoders[i].timestamp_us = g_sensor_frame.timestamp_us;
            g_sensor_frame.encoder_valid[i] = true;  // ⭐ 关键：标记有效
        }

        /* IMU数据 */
        g_sensor_frame.imu.accel_mps2[0] = 0.0f;
        g_sensor_frame.imu.accel_mps2[1] = 0.0f;
        g_sensor_frame.imu.accel_mps2[2] = 9.81f;
        g_sensor_frame.imu.gyro_radps[0] = 0.0f;
        g_sensor_frame.imu.gyro_radps[1] = 0.0f;
        g_sensor_frame.imu.gyro_radps[2] = 0.0f;
        g_sensor_frame.imu.temperature_c = 25.0f;
        g_sensor_frame.imu.timestamp_us = g_sensor_frame.timestamp_us;
        g_sensor_frame.imu_valid = true;  // ⭐ 关键：标记有效

        /* IR数据 */
        g_sensor_frame.ir.active_mask = 0xFFFF;
        for (int i = 0; i < SD_IR_CHANNEL_COUNT; i++) {
            g_sensor_frame.ir.values[i] = 100.0f;
        }
        g_sensor_frame.ir.timestamp_us = g_sensor_frame.timestamp_us;
        g_sensor_frame.ir_valid = true;  // ⭐ 关键：标记有效

        /* 运行算法流水线 */
        sd_status_t status = state_evaluator_update(&g_state_evaluator, &g_sensor_frame);
        
        if (status == SD_OK) {
            status = perception_update(&g_perception, &g_sensor_frame.ir,
                                       g_sensor_frame.timestamp_us,
                                       &g_perception_result);
        }
        if (status == SD_OK) {
            g_behavior_input.vehicle = &g_state_evaluator.state;
            g_behavior_input.perception = &g_perception_result;
            g_behavior_input.path_curvature = 0.0f;
            g_behavior_input.command =
                g_behavior_output.state == BEHAVIOR_STATE_IDLE
                    ? BEHAVIOR_CMD_START
                    : BEHAVIOR_CMD_NONE;
            status = behavior_planner_update(&g_behavior_planner, &g_behavior_input,
                                             &g_behavior_output);
        }
        if (status == SD_OK) {
            status = trajectory_generate(&g_trajectory_generator,
                                         &g_state_evaluator.state,
                                         &g_behavior_output, dt, &g_trajectory);
        }

        /* 日志输出 */
        if (status == SD_OK) {
            if ((log_counter % 50) == 0) {  // 每秒输出一次
                printf("[Cycle %u] Algorithm OK - v=%.2f, omega=%.2f\n",
                       (unsigned int)g_cycle_counter,
                       g_trajectory.v, corrected_omega);
                printf("  State: x=%.2f, y=%.2f, theta=%.2f\n",
                       g_state_evaluator.state.x,
                       g_state_evaluator.state.y,
                       g_state_evaluator.state.theta);
            }
            g_critical_failure_count = 0;
        } else {
            printf("[Cycle %u] [WARNING] Algorithm failed (count: %u)\n",
                   (unsigned int)g_cycle_counter,
                   (unsigned int)++g_critical_failure_count);
        }
        log_counter++;
    }

    g_cycle_counter++;

#else
    /* 硬件模式 - 读取真实传感器 */
    preprocess_update(PlatformTime_GetUs64(), &g_sensor_frame);
    // ... 完整的控制循环
    MotionControl_Update(&g_motion_control);
#endif
}
```

### 3.3 关键数据结构

#### sensor_frame_t (preprocess.h)
```c
typedef struct {
    uint64_t timestamp_us;
    encoder_data_t encoders[SD_ENCODER_COUNT];
    bool encoder_valid[SD_ENCODER_COUNT];  // ⭐ 必须设置
    imu_data_t imu;
    bool imu_valid;                        // ⭐ 必须设置
    ir_array_data_t ir;
    bool ir_valid;                         // ⭐ 必须设置
} sensor_frame_t;
```

#### ir_array_data_t (interface.h)
```c
typedef struct {
    uint16_t active_mask;
    float values[SD_IR_CHANNEL_COUNT];
    uint64_t timestamp_us;
} ir_array_data_t;
```

#### vehicle_state_t (state_evaluate.h)
```c
typedef struct {
    float x;        // 不是 x_m
    float y;        // 不是 y_m
    float theta;    // 不是 theta_rad
    float v;
    float omega;
    float P[SD_EKF_STATE_COUNT][SD_EKF_STATE_COUNT];
    uint64_t timestamp_us;
    bool localization_valid;
} vehicle_state_t;
```

---

## 4. 调试过程与问题解决

### 4.1 问题1：无串口输出（初次运行）

**现象**: 编译成功，烧录后串口完全无输出

**原因**: 程序卡在`ControlApp_Init()`的某个硬件初始化步骤

**解决方案**: 添加详细的分步日志
```c
printf("[ControlApp] Step 1: Motor init...\n");
Motor_Init();
printf("[ControlApp] Step 2: Encoder init...\n");
Encoder_Init();
printf("[ControlApp] Step 3: MCP23017 init...\n");
// ...
```

**定位结果**: 程序卡在`MCP23017_Init()`（I2C通信超时）

### 4.2 问题2：MCP23017初始化超时

**现象**: 
```
[ControlApp] Step 2: Encoder init...
(无后续输出)
```

**原因**: MCP23017通过I2C通信，硬件未连接时会在超时循环中等待

**解决方案**: 
1. 临时将硬件初始化失败改为警告而非致命错误
2. 最终采用软件测试模式，完全跳过硬件

### 4.3 问题3：算法失败 `SD_ERR_DATA_INVALID`

**现象**:
```
[Cycle 0] state_evaluator_update failed: -7
[Cycle 10] state_evaluator_update failed: -7
[Cycle 20] state_evaluator_update failed: -7
```

**原因分析**:
检查`state_evaluator_update()`源码（state_evaluate.c:35-45）：
```c
for (i = 0; i < SD_ENCODER_COUNT; ++i) {
    if (!frame->encoder_valid[i]) {  // ⭐ 检查有效性标志
        return SD_ERR_DATA_INVALID;
    }
}

if (!frame->imu_valid) {  // ⭐ 检查IMU有效性
    return SD_ERR_DATA_INVALID;
}
```

**根本原因**: 模拟数据中未设置`encoder_valid[]`, `imu_valid`, `ir_valid`标志

**解决方案**: 
```c
// 设置所有有效性标志
g_sensor_frame.encoder_valid[i] = true;
g_sensor_frame.imu_valid = true;
g_sensor_frame.ir_valid = true;
```

### 4.4 问题4：编译错误（结构体字段名）

**错误信息**:
```
error: no member named 'left' in 'ir_array_data_t'
error: no member named 'x_m' in 'vehicle_state_t'
```

**原因**: 使用了错误的字段名

**正确字段名**:
- `ir_array_data_t`: `active_mask` + `values[]` (不是`left`/`center`/`right`)
- `vehicle_state_t`: `x`, `y`, `theta` (不是`x_m`, `y_m`, `theta_rad`)
- `sensor_frame_t`: `encoders[]` (不是`wheel_speeds_radps[]`)

---

## 5. 最终测试结果

### 5.1 启动日志
```
=====================================
MSPM0G3507 Controller Firmware
=====================================
UART0 Debug: Enabled (115200 baud)
Target Laps: 3
Initializing...

[ControlApp] *** SOFTWARE TEST MODE - NO HARDWARE ***
[ControlApp] Initializing algorithm modules only...
[ControlApp] Validating target laps: 3
[ControlApp] Loading Sens-Decision config defaults...
[ControlApp] Initializing Sens-Decision modules...
[ControlApp] Setting up trajectory generator...
[ControlApp] Configuring square path parameters...
[ControlApp] *** Algorithm initialization complete! ***
[ControlApp] Note: Motion Control and hardware are NOT initialized
[INFO] ControlApp initialized successfully
[INFO] Control task created
[INFO] Starting FreeRTOS scheduler...
```

### 5.2 运行日志
```
[Cycle 0] Algorithm OK - v=0.00, omega=0.00
  State: x=0.00, y=0.00, theta=0.00
  Perception: lateral_err=0.00, heading_err=0.00
[Sens-Decision] warning: behavior changed from IDLE to LINE_FOLLOW
[Cycle 500] Algorithm OK - v=0.03, omega=0.25
  State: x=0.00, y=0.00, theta=0.00
  Perception: lateral_err=0.00, heading_err=0.00
[Cycle 1000] Algorithm OK - v=0.03, omega=0.25
  State: x=0.00, y=0.00, theta=0.00
  Perception: lateral_err=0.00, heading_err=0.00
...
```

### 5.3 验证结果

✅ **所有模块正常工作**:
- State Evaluator（状态评估器）
- Perception（感知模块）
- Behavior Planner（行为规划器）- 从IDLE切换到LINE_FOLLOW
- Trajectory Generator（轨迹生成器）

✅ **算法输出正常**:
- 速度命令: v=0.03 m/s, omega=0.25 rad/s
- 状态估计: 位置(0,0), 航向0（预期，因为模拟速度为0）
- 感知结果: 横向误差和航向误差都是0

✅ **FreeRTOS调度正常**:
- 500Hz控制循环运行
- 50Hz决策循环运行
- 每秒输出一次日志

---

## 6. 代码修改总结

### 6.1 新增文件
```
inc/uart_debug.h           # UART调试接口
src/uart_debug.c           # UART调试实现（含printf重定向）
tests/test_uart0_simple.c  # UART测试程序（备用）
```

### 6.2 修改文件
```
src/main.c                 # 添加启动日志
src/control_app.c          # 添加软件测试模式（条件编译）
.eide/eide.yml            # 项目名修改（测试期间）
```

### 6.3 关键代码位置

#### printf重定向
`src/uart_debug.c:11-22`
```c
int fputc(int ch, FILE *f) {
    UartDebug_PutChar((char)ch);
    return ch;
}
```

#### 软件测试模式开关
`src/control_app.c:8`
```c
#define SOFTWARE_TEST_MODE 1  // 1=软件测试, 0=硬件模式
```

#### 模拟数据生成
`src/control_app.c:298-326`

---

## 7. 使用指南

### 7.1 切换测试模式

**软件测试模式**（当前）:
```c
#define SOFTWARE_TEST_MODE 1
```
- 跳过所有硬件初始化
- 生成模拟传感器数据
- 只验证算法逻辑

**硬件模式**:
```c
#define SOFTWARE_TEST_MODE 0
```
- 完整硬件初始化
- 读取真实传感器数据
- 控制实际电机

### 7.2 编译与烧录

1. 在EIDE中按 `F7` 编译
2. 点击"下载"或使用快捷键烧录
3. 输出文件: `build/Debug/NewProject1.hex`

### 7.3 串口监控

**连接方式**:
- TX (PA10) -> USB转串口的RX
- GND -> GND

**串口设置**:
- 波特率: 115200
- 数据位: 8
- 校验位: None
- 停止位: 1
- 流控: None

### 7.4 添加自定义日志

在任何源文件中：
```c
#include <stdio.h>

// 使用printf输出（自动通过UART0）
printf("[MyModule] Status: %d\n", value);

// Sens-Decision模块使用内置宏
SD_LOG_INFO("Initialization complete");
SD_LOG_DEBUG("Sensor value: %.2f", sensor_val);
SD_LOG_WARNING("Low battery: %.1fV", voltage);
SD_LOG_ERROR("Communication timeout");
```

---

## 8. 下一步工作

### 8.1 硬件测试准备

**切换到硬件模式**:
1. 修改 `src/control_app.c:8`: `#define SOFTWARE_TEST_MODE 0`
2. 连接所有硬件外设：
   - 电机驱动器
   - 编码器
   - MCP23017 (I2C地址正确)
   - ICM42688 (SPI连接)
   - 红外传感器阵列
3. 重新编译烧录

**需要验证的硬件**:
- [ ] MCP23017 I2C通信
- [ ] ICM42688 SPI通信和校准
- [ ] 编码器计数
- [ ] 电机PWM输出
- [ ] 红外传感器读数

### 8.2 增强软件测试

**动态模拟数据**:
```c
// 模拟机器人运动
static float sim_x = 0.0f;
static float sim_y = 0.0f;
static float sim_theta = 0.0f;

sim_x += sim_v * cosf(sim_theta) * dt;
sim_y += sim_v * sinf(sim_theta) * dt;
sim_theta += sim_omega * dt;

g_sensor_frame.encoders[0].speed_mps = sim_v;
```

**场景测试**:
- 模拟偏离轨道（不同的IR读数）
- 模拟转弯（IMU角速度变化）
- 模拟加减速（速度渐变）

### 8.3 性能分析

**添加时间测量**:
```c
uint32_t start_us = PlatformTime_GetUs64();
state_evaluator_update(&g_state_evaluator, &g_sensor_frame);
uint32_t elapsed_us = PlatformTime_GetUs64() - start_us;

if ((g_cycle_counter % 500) == 0) {
    printf("State update: %u us\n", elapsed_us);
}
```

---

## 9. 关键经验总结

### 9.1 调试技巧

1. **分步日志**: 在关键步骤添加printf，快速定位卡死位置
2. **错误码输出**: 打印具体的错误码，不要只输出"failed"
3. **条件编译**: 使用`#if`/#else分离硬件和软件逻辑
4. **有效性标志**: 模拟传感器数据时别忘了设置`*_valid`标志

### 9.2 常见陷阱

1. **microLIB的printf重定向**: 使用`fputc()`而非`_sys_write()`
2. **I2C/SPI超时**: 硬件未连接时会长时间阻塞
3. **结构体字段名**: 仔细检查实际定义，不要猜测
4. **时间戳递增**: EKF需要递增的时间戳，否则返回`SD_ERR_DATA_INVALID`

### 9.3 最佳实践

1. **先软件后硬件**: 算法验证不依赖硬件
2. **渐进式测试**: 一个模块一个模块验证
3. **保留测试代码**: 使用条件编译保留，方便回归测试
4. **文档同步**: 及时记录调试过程和解决方案

---

## 10. 参考资料

### 10.1 相关文档
- `README.md` - 工程入口和构建步骤
- `WIRING_AND_SYSCONFIG.md` - 引脚配置参考
- `Debug/ti_msp_dl_config.c` - UART0配置（自动生成）

### 10.2 数据结构定义
- `modules/Sens-Decision/inc/interface.h` - 传感器数据结构
- `modules/Sens-Decision/inc/preprocess.h` - sensor_frame_t定义
- `modules/Sens-Decision/inc/state_evaluate.h` - vehicle_state_t定义
- `modules/Sens-Decision/inc/config.h` - 错误码定义

### 10.3 工具链
- Arm Compiler 6 (`C:\Keil_v5\ARM\ARMCLANG`)
- EIDE (Embedded IDE)
- TI SysConfig 1.26.2

---

**结论**: UART0调试功能和软件测试模式已成功实现并验证。所有算法模块在纯软件环境下正常运行，为后续硬件集成测试奠定了基础。
