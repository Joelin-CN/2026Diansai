# TI MSPM0G3507 → STM32F407VGT6 迁移说明

> 文档日期：2026-07-29  
> 源工程：`E:\B306\2026\diansai\2026task\m0_controller\test`  
> 目标工程：`E:\B306\2026\diansai\2026task\stm32\v1.0`  
> **重大架构变化：4轮差速底盘 → 2轮差速底盘**

---

## 目录

1. [迁移总体策略](#1-迁移总体策略)
2. [文件迁移清单](#2-文件迁移清单)
3. [4轮→2轮架构改造](#3-4轮2轮架构改造)
4. [平台驱动层重写详解](#4-平台驱动层重写详解)
5. [FreeRTOS任务架构设计](#5-freertos任务架构设计)
6. [CMake配置更新](#6-cmake配置更新)
7. [各阶段验证方法](#7-各阶段验证方法)
8. [需要重新标定的参数](#8-需要重新标定的参数)

---

## 1. 迁移总体策略

### 1.1 分层迁移顺序（从底层到上层）

```
阶段1  电机驱动 + 编码器（最小硬件回路）
阶段2  platform_time + uart_debug（调试基础设施）
阶段3  ICM42688 SPI 适配器
阶段4  IR 循迹 UART 适配
阶段5  Motion Control 层（含4→2改造）
阶段6  Sens-Decision 层（含4→2改造）
阶段7  control_app + main.c（整体联调）
```

### 1.2 核心原则

- **平台无关模块**直接复制，不修改任何算法逻辑
- **平台相关模块**全部重写，只保留接口签名不变
- 4轮→2轮的修改**集中在2个文件**：`motion_feedback.h/c`（EncoderId枚举）和 `config.h`（SD_ENCODER_COUNT）
- 新建目录 `Core/Src/app/` 和 `Core/Inc/app/` 存放所有用户代码

---

## 2. 文件迁移清单

### 2.1 直接复制（平台无关，无需修改）

以下模块全部复制到 `modules/`（目标工程已有该目录，当前为空）：

| 源路径 | 目标路径 | 说明 |
|---|---|---|
| `modules/Motion Control/src/motion_kinematics.c` | `modules/Motion Control/src/` | 差速运动学，纯数学 |
| `modules/Motion Control/inc/motion_kinematics.h` | `modules/Motion Control/inc/` | |
| `modules/Motion Control/src/motion_feedforward.c` | `modules/Motion Control/src/` | 前馈控制，纯数学 |
| `modules/Motion Control/inc/motion_feedforward.h` | `modules/Motion Control/inc/` | |
| `modules/ICM42688/src/ahrs_hal.c` | `modules/ICM42688/src/` | Mahony AHRS，纯数学 |
| `modules/ICM42688/inc/ahrs_hal.h` | `modules/ICM42688/inc/` | |
| `modules/ICM42688/src/icm42688_hal.c` | `modules/ICM42688/src/` | 回调注入式HAL，平台无关 |
| `modules/ICM42688/inc/icm42688_hal.h` | `modules/ICM42688/inc/` | |
| `modules/IR-tracker/src/ir_uart_sensor.c` | `modules/IR-tracker/src/` | 纯字节解析，无硬件依赖 |
| `modules/IR-tracker/inc/ir_uart_sensor.h` | `modules/IR-tracker/inc/` | |
| `modules/MCP23017/src/mcp23017.c` | `modules/MCP23017/src/` | I2C逻辑，HAL接口替换 |
| `modules/MCP23017/inc/mcp23017.h` | `modules/MCP23017/inc/` | |
| `modules/Sens-Decision/` | `modules/Sens-Decision/` | 全部复制（纯C算法） |
| `src/square_path.c` | `Core/Src/app/` | 路径生成，纯数学 |
| `inc/square_path.h` | `Core/Inc/app/` | |
| `src/line_sensor.c` | `Core/Src/app/` | 可选 |
| `inc/line_sensor.h` | `Core/Inc/app/` | |

> **注意**：`modules/Sens-Decision/` 中有2个文件在4→2改造中需要修改，见第3节。
> `modules/Motion Control/src/motion_feedback.c` 和 `motion_feedback.h` 也需要改造。

### 2.2 需重写的平台相关文件

新建目录 `Core/Src/app/` 和 `Core/Inc/app/`，在其中创建以下文件：

| 新文件 | 对应TI原文件 | 主要改动 |
|---|---|---|
| `Core/Src/app/motor.c` | `src/motor.c` | 4轮→2轮，TI DriverLib→STM32 HAL TIM1 PWM |
| `Core/Inc/app/motor.h` | `inc/motor.h` | 去掉 `Motor_SetFour()`，接口保持 `Motor_SetSpeed(l,r)` |
| `Core/Src/app/encoder.c` | `src/encoder.c` | GPIO软件解码→TIM3/TIM4硬件编码器，4轮→2轮 |
| `Core/Inc/app/encoder.h` | `inc/encoder.h` | `Encoder_Id` 枚举从4项缩减为2项 |
| `Core/Src/app/encoder_hw_bridge.c` | `src/encoder_hw_bridge.c` | 直接读TIM计数器 |
| `Core/Inc/app/encoder_hw_bridge.h` | `inc/encoder_hw_bridge.h` | 接口不变（参数0=左,1=右） |
| `Core/Src/app/encoder_adapter.c` | `src/encoder_adapter.c` | EncoderId 2项映射 |
| `Core/Inc/app/encoder_adapter.h` | `inc/encoder_adapter.h` | 不变 |
| `Core/Src/app/motor_adapter.c` | `src/motor_adapter.c` | 绑定到新 Motor_SetSpeed/Stop/Init |
| `Core/Inc/app/motor_adapter.h` | `inc/motor_adapter.h` | 不变 |
| `Core/Src/app/sensor_adapter.c` | `src/sensor_adapter.c` | 2个编码器，STM32 IMU/IR HAL |
| `Core/Inc/app/sensor_adapter.h` | `inc/sensor_adapter.h` | 不变 |
| `Core/Src/app/uart_debug.c` | `src/uart_debug.c` | fputc重定向到UART5 |
| `Core/Inc/app/uart_debug.h` | `inc/uart_debug.h` | 不变 |
| `Core/Src/app/platform_time.c` | `src/platform_time.c` | TIMG12→DWT_CYCCNT（168MHz） |
| `Core/Inc/app/platform_time.h` | `inc/platform_time.h` | 接口完全不变 |
| `Core/Src/app/control_app.c` | `src/control_app.c` | 去掉SOFTWARE_TEST_MODE，更新硬件初始化 |
| `Core/Inc/app/control_app.h` | `inc/control_app.h` | 不变 |
| `modules/ICM42688/src/icm42688_stm32.c` | `modules/ICM42688/src/icm42688_mspm0.c` | 绑定SPI2 HAL（PE7 CS） |
| `modules/ICM42688/inc/icm42688_stm32.h` | `modules/ICM42688/inc/icm42688_mspm0.h` | 改名，入口函数改为 `icm42688_stm32_bind()` |

### 2.3 不迁移的文件

| 文件 | 原因 |
|---|---|
| `Debug/ti_msp_dl_config.h/c` | TI SysConfig生成，STM32用CubeMX替代 |
| `modules/ICM42688/src/icm42688_mspm0.c/h` | 替换为 `icm42688_stm32.c/h` |
| `FreeRTOS/` | STM32工程已有 `Middlewares/Third_Party/FreeRTOS/` |
| `src/main.c`（原TI版） | 用STM32 `Core/Src/main.c`（CubeMX生成）替代 |
| `tests/` | 测试程序不迁移，按需重新编写 |
| `src/encoder_hw_bridge.c`（中断版本） | STM32用硬件编码器，不需要GPIO中断解码 |

---

## 3. 4轮→2轮架构改造

这是本次迁移的**最大架构变化**，以下列出所有受影响的位置。

### 3.1 `modules/Motion Control/inc/motion_feedback.h`

**EncoderId_t 枚举（直接修改复制过来的文件）**：

```c
// 原来（4轮）
typedef enum {
    ENCODER_LEFT_FRONT = 0,
    ENCODER_LEFT_REAR  = 1,
    ENCODER_RIGHT_FRONT = 2,
    ENCODER_RIGHT_REAR  = 3,
    ENCODER_COUNT       // = 4
} EncoderId_t;

// 改为（2轮）
typedef enum {
    ENCODER_LEFT  = 0,
    ENCODER_RIGHT = 1,
    ENCODER_COUNT // = 2
} EncoderId_t;
```

**StateEstimator_t 结构体**中的数组会自动缩小（因为用了 `ENCODER_COUNT`）：

```c
int32_t prev_count[ENCODER_COUNT];         // 4→2
float   wheel_speed[ENCODER_COUNT];        // 4→2
float   wheel_speed_filtered[ENCODER_COUNT]; // 4→2
```

### 3.2 `modules/Motion Control/src/motion_feedback.c`

`StateEst_GetLeftSpeed` / `StateEst_GetRightSpeed` 的实现需要修改，原来取前后轮平均：

```c
// 原来（4轮）
float StateEst_GetLeftSpeed(StateEstimator_t *est) {
    return (est->wheel_speed_filtered[ENCODER_LEFT_FRONT] +
            est->wheel_speed_filtered[ENCODER_LEFT_REAR]) * 0.5f;
}
float StateEst_GetRightSpeed(StateEstimator_t *est) {
    return (est->wheel_speed_filtered[ENCODER_RIGHT_FRONT] +
            est->wheel_speed_filtered[ENCODER_RIGHT_REAR]) * 0.5f;
}

// 改为（2轮）
float StateEst_GetLeftSpeed(StateEstimator_t *est) {
    return est->wheel_speed_filtered[ENCODER_LEFT];
}
float StateEst_GetRightSpeed(StateEstimator_t *est) {
    return est->wheel_speed_filtered[ENCODER_RIGHT];
}
```

`StateEst_Update` 中循环上限自动变为2（因为用了 `ENCODER_COUNT`），无需额外改动。
`v_left` / `v_right` 的计算需对应修改，同上。

### 3.3 `modules/Sens-Decision/inc/config.h`

```c
// 原来
#define SD_ENCODER_COUNT 4U

// 改为
#define SD_ENCODER_COUNT 2U
```

> 这一改动会同步缩小 `sensor_frame_t.encoders[]`、`encoder_data_t` 数组等所有使用该宏的地方。

### 3.4 `modules/Sens-Decision/src/config.c` — `sd_config_reset_defaults()`

找到 `sd_vehicle_config_t` 的默认初始化，修改编码器索引：

```c
// 原来（4轮，左前+左后 / 右前+右后）
cfg->vehicle.left_encoder_indices[0]  = 0;  // ENCODER_LEFT_FRONT
cfg->vehicle.left_encoder_indices[1]  = 1;  // ENCODER_LEFT_REAR
cfg->vehicle.right_encoder_indices[0] = 2;  // ENCODER_RIGHT_FRONT
cfg->vehicle.right_encoder_indices[1] = 3;  // ENCODER_RIGHT_REAR

// 改为（2轮）
cfg->vehicle.left_encoder_indices[0]  = 0;  // ENCODER_LEFT
cfg->vehicle.left_encoder_indices[1]  = 0;  // 重复，或Sens-Decision层只读[0]
cfg->vehicle.right_encoder_indices[0] = 1;  // ENCODER_RIGHT
cfg->vehicle.right_encoder_indices[1] = 1;  // 重复
```

> 如果 Sens-Decision 内部用 `[0]` 取左侧编码器，`[1]` 取右侧编码器，以上设置即可。
> 如果用两个索引做平均，两个设成同一值等效于直接使用该值。

### 3.5 `Core/Src/app/encoder_adapter.c`（新建）

```c
#include "encoder_adapter.h"
#include "encoder_hw_bridge.h"
#include "motion_feedback.h"  // EncoderId_t 定义

static int32_t adapter_getCount(EncoderId_t id) {
    if (id == ENCODER_LEFT)  return EncoderHwBridge_GetCount(0);
    if (id == ENCODER_RIGHT) return EncoderHwBridge_GetCount(1);
    return 0;
}

static void adapter_resetCount(EncoderId_t id) {
    if (id == ENCODER_LEFT)  { EncoderHwBridge_ResetCount(0); return; }
    if (id == ENCODER_RIGHT) { EncoderHwBridge_ResetCount(1); return; }
}

static EncoderInterface_t s_iface = {
    .getCount    = adapter_getCount,
    .resetCount  = adapter_resetCount,
};

EncoderInterface_t *EncoderAdapter_GetInterface(void) { return &s_iface; }
```

### 3.6 `Core/Src/app/sensor_adapter.c`（新建）

`sensor_hal_t` 的 `read_encoder_count` 回调只有2个索引：

```c
static sd_status_t read_encoder_count(uint8_t index, int32_t *count) {
    if (index >= 2U || count == NULL) return SD_ERR_INVALID_ARGUMENT;
    *count = EncoderHwBridge_GetCount(index);
    return SD_OK;
}
// read_imu_raw / read_ir 与原TI版相似，替换底层HAL调用即可（见第4节）
```

---

## 4. 平台驱动层重写详解

### 4.1 `platform_time.c` — 用 DWT 计数器替代 TIMG12

STM32F407 的 DWT（Data Watchpoint and Trace）有32位硬件自由运行计数器，频率等于CPU时钟（168MHz）。

```c
// Core/Src/app/platform_time.c
#include "platform_time.h"
#include "stm32f4xx.h"

static uint32_t s_overflow_count = 0;
static uint32_t s_last_cyccnt    = 0;

void PlatformTime_Init(void) {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
    s_overflow_count = 0;
    s_last_cyccnt    = 0;
}

uint32_t PlatformTime_GetUs32(void) {
    return DWT->CYCCNT / 168U;   // 168 MHz → 微秒
}

uint64_t PlatformTime_GetUs64(void) {
    uint32_t now = DWT->CYCCNT;
    if (now < s_last_cyccnt) { s_overflow_count++; }  // 溢出检测
    s_last_cyccnt = now;
    uint64_t cycles = ((uint64_t)s_overflow_count << 32) | (uint64_t)now;
    return cycles / 168ULL;
}
```

> `GetUs64` 需在单任务或临界区中调用，否则 `s_overflow_count` 有竞态风险。
> 32位计数器在168MHz下约25.6秒溢出一次；如只用 `GetUs32`，溢出即归零，
> 差值计算仍然正确（`(uint32_t)(now - last)` 对溢出透明）。

### 4.2 `uart_debug.c` — printf 重定向到 UART5

```c
// Core/Src/app/uart_debug.c
#include "uart_debug.h"
#include "usart.h"
#include <stdio.h>

void UartDebug_Init(void) { /* CubeMX已初始化，NOP */ }

// 重定向 printf → UART5（阻塞发送）
int fputc(int ch, FILE *f) {
    (void)f;
    uint8_t c = (uint8_t)ch;
    HAL_UART_Transmit(&huart5, &c, 1, HAL_MAX_DELAY);
    return ch;
}

// 如需保留原有非printf接口，直接用 HAL_UART_Transmit 实现即可
void UartDebug_PutChar(char c) {
    HAL_UART_Transmit(&huart5, (uint8_t *)&c, 1, HAL_MAX_DELAY);
}
```

> 注意：`huart5` 在 `usart.h` 中声明为 `extern UART_HandleTypeDef huart5;`。
> 如在FreeRTOS任务中使用，`HAL_MAX_DELAY` 会阻塞任务；调试阶段可以接受。

### 4.3 `motor.c` — STM32 TB6612 两轮驱动

硬件对应关系：左轮=TB6612 B通道（TIM1_CH1），右轮=TB6612 C通道（TIM1_CH2）。
软件PWM范围 `[-1000, +1000]` 映射到 TIM1 比较值 `[0, 8399]`。

```c
// Core/Src/app/motor.c
#include "motor.h"
#include "tim.h"
#include "gpio.h"
#include "main.h"   // 引脚宏定义
#include <stdlib.h>

#define MOTOR_PWM_ARR  8399U   // TIM1 ARR，对应20kHz、100%占空比
#define MOTOR_SPEED_MAX 1000

static void _set_wheel(uint32_t ch,
                       GPIO_TypeDef *in1p, uint16_t in1,
                       GPIO_TypeDef *in2p, uint16_t in2,
                       int16_t speed) {
    if (speed >  MOTOR_SPEED_MAX) speed =  MOTOR_SPEED_MAX;
    if (speed < -MOTOR_SPEED_MAX) speed = -MOTOR_SPEED_MAX;
    uint32_t pulse = (uint32_t)(abs(speed) * (int32_t)MOTOR_PWM_ARR / MOTOR_SPEED_MAX);
    if (speed > 0) {
        HAL_GPIO_WritePin(in1p, in1, GPIO_PIN_SET);
        HAL_GPIO_WritePin(in2p, in2, GPIO_PIN_RESET);
    } else if (speed < 0) {
        HAL_GPIO_WritePin(in1p, in1, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(in2p, in2, GPIO_PIN_SET);
    } else {
        HAL_GPIO_WritePin(in1p, in1, GPIO_PIN_RESET);  // 刹车：IN1=IN2=0
        HAL_GPIO_WritePin(in2p, in2, GPIO_PIN_RESET);
    }
    __HAL_TIM_SET_COMPARE(&htim1, ch, pulse);
}

void Motor_Init(void) {
    HAL_GPIO_WritePin(MOTOR_STBY_GPIO_Port, MOTOR_STBY_Pin, GPIO_PIN_SET); // 使能TB6612
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
    Motor_Stop();
}

void Motor_SetSpeed(int16_t left, int16_t right) {
    _set_wheel(TIM_CHANNEL_1,
               MOTOR_B_IN1_GPIO_Port, MOTOR_B_IN1_Pin,
               MOTOR_B_IN2_GPIO_Port, MOTOR_B_IN2_Pin, left);
    _set_wheel(TIM_CHANNEL_2,
               MOTOR_C_IN1_GPIO_Port, MOTOR_C_IN1_Pin,
               MOTOR_C_IN2_GPIO_Port, MOTOR_C_IN2_Pin, right);
}

void Motor_Stop(void) { Motor_SetSpeed(0, 0); }
```

> `MOTOR_B_IN1_Pin` 等宏均已在 `Core/Inc/main.h` 由 CubeMX 定义好。
> STBY 上电初始为低（待机），`Motor_Init()` 必须先将其拉高，否则 TB6612 无输出。

### 4.4 `encoder.c` — TIM3/TIM4 硬件编码器（含溢出扩展）

TIM3/TIM4 已被 CubeMX 配置为 `TIM_ENCODERMODE_TI12`（双沿计数，4倍频），16位。
需要在 `Encoder_Poll()` 中做16位→32位累积计数的溢出补偿。

```c
// Core/Src/app/encoder.c
#include "encoder.h"
#include "tim.h"

static int32_t  s_count[2]    = {0, 0};
static uint16_t s_last[2]     = {0, 0};
static TIM_HandleTypeDef *const s_htim[2] = {&htim3, &htim4};

void Encoder_Init(void) {
    HAL_TIM_Encoder_Start(&htim3, TIM_CHANNEL_ALL);
    HAL_TIM_Encoder_Start(&htim4, TIM_CHANNEL_ALL);
    s_last[0] = (uint16_t)__HAL_TIM_GET_COUNTER(&htim3);
    s_last[1] = (uint16_t)__HAL_TIM_GET_COUNTER(&htim4);
    s_count[0] = 0;
    s_count[1] = 0;
}

/* 在 500Hz 控制任务中调用，更新累积计数 */
void Encoder_Poll(void) {
    for (int i = 0; i < 2; i++) {
        uint16_t now   = (uint16_t)__HAL_TIM_GET_COUNTER(s_htim[i]);
        int16_t  delta = (int16_t)(now - s_last[i]);  // 有符号强制转换自动处理溢出
        s_count[i]    += (int32_t)delta;
        s_last[i]      = now;
    }
}

int32_t Encoder_GetCount(uint8_t id) {
    if (id >= 2U) return 0;
    return s_count[id];
}

void Encoder_ResetCount(uint8_t id) {
    if (id >= 2U) return;
    s_count[id] = 0;
    s_last[id]  = (uint16_t)__HAL_TIM_GET_COUNTER(s_htim[id]);
}
```

**溢出原理**：`int16_t delta = (int16_t)(now - last)` — 两个 uint16_t 相减后
强制转换为 int16_t，当差值在 [-32768, +32767] 范围内时结果始终正确，天然处理计数器绕回。
在 500Hz 下，单周期计数变化远小于 32768 pulses，不会出错。

### 4.5 `encoder_hw_bridge.c` — 硬件桥接层

```c
// Core/Src/app/encoder_hw_bridge.c
#include "encoder_hw_bridge.h"
#include "encoder.h"
#include "cmsis_os.h"

int32_t EncoderHwBridge_GetCount(uint8_t physical_id) {
    return Encoder_GetCount(physical_id);
}

void EncoderHwBridge_ResetCount(uint8_t physical_id) {
    taskENTER_CRITICAL();       // ISR 安全（与原TI版保持一致）
    Encoder_ResetCount(physical_id);
    taskEXIT_CRITICAL();
}
```

### 4.6 `icm42688_stm32.c` — SPI2 适配器

```c
// modules/ICM42688/src/icm42688_stm32.c
#include "icm42688_stm32.h"
#include "icm42688_hal.h"
#include "spi.h"
#include "gpio.h"
#include "main.h"
#include "platform_time.h"
#include "cmsis_os.h"

/* SPI 全双工读写（CS由本函数控制） */
static void _spi_write_read(const uint8_t *tx, uint8_t *rx, uint16_t len) {
    HAL_GPIO_WritePin(IMU_CS_GPIO_Port, IMU_CS_Pin, GPIO_PIN_RESET);
    HAL_SPI_TransmitReceive(&hspi2, (uint8_t *)tx, rx, len, 10);
    HAL_GPIO_WritePin(IMU_CS_GPIO_Port, IMU_CS_Pin, GPIO_PIN_SET);
}

/* 延时（FreeRTOS 任务内用 osDelay；初始化时在任务外可用 HAL_Delay） */
static void _delay_ms(uint32_t ms) {
    if (osKernelGetState() == osKernelRunning) {
        osDelay(ms);
    } else {
        HAL_Delay(ms);
    }
}

static uint32_t _get_us(void) { return PlatformTime_GetUs32(); }

static const icm42688_comm_t   s_comm   = { .write_read = _spi_write_read };
static const icm42688_system_t s_system = { .delay_ms   = _delay_ms };
static const icm42688_timer_t  s_timer  = { .get_time_us = _get_us };

void icm42688_stm32_bind(const icm42688_config_t *config) {
    icm42688_hal_init(&s_comm, &s_system, config);
}
```

在 `control_app.c` 中将所有 `icm42688_mspm0_bind(...)` 替换为 `icm42688_stm32_bind(...)`。

### 4.7 IR 循迹模块 — USART2 DMA+IDLE 集成

`ir_uart_sensor.c` 的逻辑不变，只需为其提供正确的字节流入口。

**启动 DMA 接收**（在 `IrUartSensor_Init()` 中调用，或在任务初始化时调用）：

```c
// 定义接收缓冲区（建议256字节）
static uint8_t g_ir_dma_buf[256];

// 启动 DMA + IDLE 接收
HAL_UARTEx_ReceiveToIdle_DMA(&huart2, g_ir_dma_buf, sizeof(g_ir_dma_buf));
__HAL_DMA_DISABLE_IT(huart2.hdmarx, DMA_IT_HT);  // 关闭半传输中断，减少无用回调
```

**IDLE 回调**（在 `stm32f4xx_it.c` 或 `Core/Src/` 用户区中实现）：

```c
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size) {
    if (huart->Instance == USART2) {
        // 将新接收的字节逐一送入协议解析器
        // 注意 DMA Circular 模式下 Size 是本次到达的字节数
        for (uint16_t i = 0; i < Size; i++) {
            IrUartSensor_RxByte(g_ir_dma_buf[i]);
        }
        // Circular 模式无需重新启动
    }
}
```

> DMA 配置为 Circular 模式，不需要每次回调后重新调用 `HAL_UARTEx_ReceiveToIdle_DMA`。
> `IrUartSensor_RequestAnalogMode()` 通过 `HAL_UART_Transmit(&huart2, ...)` 发送命令帧。

### 4.8 MCP23017 — I2C1 HAL 适配

`mcp23017.c` 内部使用 TI 的 `DL_I2C_*` API，替换为 STM32 HAL I2C 调用：

| TI DriverLib | STM32 HAL 等效 |
|---|---|
| `DL_I2C_fillControllerTXFIFO(I2C0, &reg, 1)` + `DL_I2C_startControllerTransfer(...)` | `HAL_I2C_Mem_Write(&hi2c1, addr, reg, 1, data, len, 10)` |
| `DL_I2C_fillControllerTXFIFO` + Read | `HAL_I2C_Mem_Read(&hi2c1, addr, reg, 1, data, len, 10)` |

> `mcp23017.c` 中 I2C 操作集中在 `MCP23017_Init()` 和 `MCP23017_ReadInputs()` 两处，
> 替换工作量极小。I2C 地址为 `0x20 << 1 = 0x40`（HAL 使用8位地址，已左移）。

---

## 5. FreeRTOS 任务架构设计

### 5.1 500Hz 定时器：使用 TIM7

TIM6 已被 CubeMX 占用作 HAL 时基。使用 **TIM7**（空闲的16位通用定时器）实现500Hz中断。

**CubeMX 配置（或手动配置）**：
- Prescaler = 167（168MHz ÷ 168 = 1MHz 计数时钟）
- Period = 1999（1MHz ÷ 2000 = 500Hz）
- 使能 TIM7 global interrupt，Priority = 5（与其他外设中断一致）

**`stm32f4xx_it.c` 中的ISR**（直接操作寄存器，绕开HAL回调冲突）：

```c
// 在文件顶部声明（或在专门的任务头文件中）
extern void ControlTask_NotifyFromISR(void);

void TIM7_IRQHandler(void) {
    if (__HAL_TIM_GET_FLAG(&htim7, TIM_FLAG_UPDATE) &&
        __HAL_TIM_GET_IT_SOURCE(&htim7, TIM_IT_UPDATE)) {
        __HAL_TIM_CLEAR_IT(&htim7, TIM_IT_UPDATE);
        ControlTask_NotifyFromISR();
    }
}
```

### 5.2 FreeRTOS 任务：`freertos.c`

```c
// 在 freertos.c 的 USER CODE 区域中添加

static TaskHandle_t xControlTaskHandle = NULL;

/* ISR 端通知函数（在 stm32f4xx_it.c 中被 TIM7_IRQHandler 调用） */
void ControlTask_NotifyFromISR(void) {
    BaseType_t xWoken = pdFALSE;
    vTaskNotifyGiveFromISR(xControlTaskHandle, &xWoken);
    portYIELD_FROM_ISR(xWoken);
}

/* 500Hz 控制任务 */
static void ControlTask(void *arg) {
    (void)arg;
    ControlApp_Init(3);                   // 初始化（含电机、编码器、IMU、循迹）
    HAL_TIM_Base_Start_IT(&htim7);        // 启动500Hz定时器
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        ControlApp_RunFastCycle();        // 500Hz执行，内部每10次执行一次50Hz感知决策
    }
}

void MX_FREERTOS_Init(void) {
    /* 创建控制任务：优先级4，栈1024字（STM32 RAM充足，比MSPM0的512更大） */
    xTaskCreate(ControlTask, "CtrlTask", 1024, NULL, 4, &xControlTaskHandle);

    /* CubeMX 生成的 defaultTask（蓝牙初始化）保持不变 */
    defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);
}
```

> `ControlApp_Init()` 内部调用 `icm42688_calibrate_gyro(100, 10)` 会产生约1秒阻塞，
> 在FreeRTOS任务内通过 `osDelay()` 阻塞时其他任务正常运行，没有问题。
> `osDelay()` 依赖 SysTick（FreeRTOS tick），不受 HAL 时基切到 TIM6 的影响。

---

## 6. CMake 配置更新

编辑 `E:\B306\2026\diansai\2026task\stm32\v1.0\CMakeLists.txt`，在 `target_sources` 和
`target_include_directories` 的 `# Add user sources here` 注释下添加：

> **注意**：`modules/Motion Control/` 目录名含空格，CMake路径需用引号包裹。
> 建议在复制时将目录改名为 `modules/MotionControl/`，避免构建系统隐患。

```cmake
# ── 用户应用层 ──────────────────────────────────────────────────────────────
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    Core/Src/app/motor.c
    Core/Src/app/encoder.c
    Core/Src/app/encoder_hw_bridge.c
    Core/Src/app/encoder_adapter.c
    Core/Src/app/motor_adapter.c
    Core/Src/app/sensor_adapter.c
    Core/Src/app/uart_debug.c
    Core/Src/app/platform_time.c
    Core/Src/app/control_app.c
    Core/Src/app/square_path.c

    # Motion Control 模块（目录名中的空格须用引号）
    "modules/MotionControl/src/motion_control.c"
    "modules/MotionControl/src/motion_feedback.c"
    "modules/MotionControl/src/motion_feedforward.c"
    "modules/MotionControl/src/motion_kinematics.c"

    # Sens-Decision 模块
    modules/Sens-Decision/src/config.c
    modules/Sens-Decision/src/interface.c
    modules/Sens-Decision/src/utils.c
    modules/Sens-Decision/src/preprocess.c
    modules/Sens-Decision/src/EKF.c
    modules/Sens-Decision/src/state_evaluate.c
    modules/Sens-Decision/src/perception.c
    modules/Sens-Decision/src/behavior_planner.c
    modules/Sens-Decision/src/trajectory_generate.c

    # ICM42688 模块
    modules/ICM42688/src/icm42688_hal.c
    modules/ICM42688/src/icm42688_stm32.c
    modules/ICM42688/src/ahrs_hal.c

    # IR 循迹模块
    modules/IR-tracker/src/ir_uart_sensor.c

    # MCP23017 模块（如需使用）
    modules/MCP23017/src/mcp23017.c
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    Core/Inc/app
    "modules/MotionControl/inc"
    modules/Sens-Decision/inc
    modules/ICM42688/inc
    modules/IR-tracker/inc
    modules/MCP23017/inc
)
```

---

## 7. 各阶段验证方法

### 阶段1：电机 + 编码器基础回路

在 `defaultTask` 中替换为以下测试代码，**不启用控制任务**：

```c
Motor_Init();
Encoder_Init();
for (;;) {
    Motor_SetSpeed(300, 300);      // 正转，约 30% 功率
    osDelay(20);
    Encoder_Poll();
    int32_t l = Encoder_GetCount(0);
    int32_t r = Encoder_GetCount(1);
    printf("ENC L=%ld R=%ld\r\n", l, r);
}
```

**预期**：两个编码器计数均单调递增；方向一致；速度比值接近1（左右对称）。

验证项目：
- [ ] STBY 使能后电机有输出（听到转动声）
- [ ] 正转时两编码器都增大，反转时都减小
- [ ] `Motor_SetSpeed(0,0)` 后电机制动，编码器停止

### 阶段2：Motion Control 闭环

```c
MotionControl_Init(&g_mc, EncoderAdapter_GetInterface(), MotorAdapter_GetInterface());
MotionControl_Start(&g_mc);
// 在500Hz任务中：
MotionControl_SetVelocityCommand(&g_mc, 0.3f, 0.0f);  // 0.3 m/s 直行
MotionControl_Update(&g_mc);
// 每500ms打印一次轮速
float vl, vr;
MotionControl_GetWheelSpeed(&g_mc, &vl, &vr);
printf("v_l=%.3f v_r=%.3f\r\n", vl, vr);
```

**预期**：1~2秒内左右轮速收敛到 0.3 m/s，无持续振荡。

### 阶段3：IMU 验证

```c
icm42688_stm32_bind(&g_icm_config);
if (icm42688_init() == ICM42688_STATUS_OK) {
    printf("ICM OK\r\n");
    icm42688_data_t data;
    icm42688_read(&data);
    printf("accel: %d %d %d\r\n", data.accel[0], data.accel[1], data.accel[2]);
}
```

**预期**：`WHO_AM_I = 0x47`；静止时加速度 Z 轴接近满量程（±8g 时约 ±4096 LSB 对应 1g）。

### 阶段4：IR 循迹验证

将小车放置在白色地面 + 黑色线条的赛道上：

```c
IrUartSensor_Init();
IrUartSensor_RequestAnalogMode();
// 等待20s暖机，然后：
uint16_t vals[8];
if (IrUartSensor_GetAnalog(vals)) {
    printf("IR: %d %d %d %d %d %d %d %d\r\n",
           vals[0],vals[1],vals[2],vals[3],vals[4],vals[5],vals[6],vals[7]);
}
```

**预期**：在黑线处读数较低，白色区域读数较高（反射率差异 > 1000）。

### 阶段5：完整控制联调

取消所有测试代码，恢复 `freertos.c` 中的 `ControlTask` 和 `defaultTask`，完整运行后：
- 小车能沿1m×1m正方形路径行驶
- 3圈后自动停止

---

## 8. 需要重新标定的参数

以下参数来自原 TI 工程的 `motion_config.h`，在 STM32 新硬件上必须重新测量：

| 参数 | 文件 | 当前值 | 标定方法 |
|---|---|---|---|
| `WHEEL_BASE` | `motion_config.h` | 0.150m | 用直尺量左右轮**中心线**间距 |
| `WHEEL_RADIUS` | `motion_config.h` | 0.033m | 轮子在已知长度地面滚动，量实际半径 |
| `ENCODER_PPR` | `motion_config.h` | 334 | 复位计数，手推轮子精确转1圈，读计数差值 |
| `SPEED_KP` | `motion_config.h` | 200.0 | 先置0再逐渐增大至速度响应无振荡 |
| `SPEED_KI` | `motion_config.h` | 50.0 | KP调好后增大KI消除稳态误差 |
| `FF_K_STATIC` | `motion_config.h` | 80.0 | 从0逐渐增大PWM，记录轮子刚开始转动的临界值 |
| `FF_K_FRICTION` | `motion_config.h` | 300.0 | 恒速直行时记录平均PWM，除以速度 |
| `FF_K_ACCEL` | `motion_config.h` | 50.0 | 阶跃加速测试，记录超调所需补偿 |

### Sens-Decision 配置（`config.c` 中的 `sd_config_reset_defaults()`）

| 参数 | 说明 |
|---|---|
| `vehicle.wheel_track_m` | 与 `WHEEL_BASE` 保持一致 |
| `encoders[i].wheel_radius_m` | 与 `WHEEL_RADIUS` 保持一致 |
| `encoders[i].pulses_per_revolution` | 与 `ENCODER_PPR` 保持一致 |
| `encoders[i].direction` | 正转时计数增大为 +1，否则为 -1（需实测）|
| `perception.weights[8]` | 8路IR传感器加权系数，根据传感器间距重新设置 |
| `imu.accel_scale_mps2_per_lsb` | 由 `icm42688_get_scale_factors()` 自动获取 |
| `imu.gyro_bias_radps[3]` | 由 `icm42688_calibrate_gyro()` 自动标定 |

---

## 附录：引脚速查表（STM32F407VGT6 天空星）

| 功能 | 引脚 | HAL 宏（main.h） |
|---|---|---|
| 左轮 PWM | PE9 TIM1_CH1 | `htim1 / TIM_CHANNEL_1` |
| 右轮 PWM | PE11 TIM1_CH2 | `htim1 / TIM_CHANNEL_2` |
| 左轮 IN1/IN2 | PD14/PD15 | `MOTOR_B_IN1/2_Pin` |
| 右轮 IN1/IN2 | PE2/PE3 | `MOTOR_C_IN1/2_Pin` |
| TB6612 STBY | PE0 | `MOTOR_STBY_Pin` |
| 左轮编码器 A/B | PB4/PB5 TIM3 | `htim3` |
| 右轮编码器 A/B | PD12/PD13 TIM4 | `htim4` |
| 调试串口 TX/RX | PC12/PD2 UART5 | `huart5` |
| 循迹模块 | PA2/PA3 USART2 | `huart2` |
| K230 视觉 | PD8/PD9 USART3 | `huart3` |
| 平衡舵机 | PC6/PC7 USART6 | `huart6` |
| IMU SPI | PB13/14/15 SPI2 | `hspi2` |
| IMU CS | PE7 | `IMU_CS_Pin` |
| IMU INT1 | PE8 EXTI8 | `IMU_INT1_Pin` |
| OLED I2C | PB8/PB9 I2C1 | `hi2c1` |
| 按键 1~5 | PC0~PC4 EXTI | `KEY1_Pin ~ KEY5_Pin` |


