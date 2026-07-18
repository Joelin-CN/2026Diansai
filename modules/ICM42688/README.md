# ICM42688 MSPM0G3507 驱动模块

本模块将原 STM32 平台的 ICM42688 六轴 IMU 驱动移植到 TI MSPM0G3507，提供：

- 平台无关的 ICM42688 核心 HAL（`icm42688_hal`）
- 平台无关的六轴 Mahony AHRS 姿态解算（`ahrs_hal`）
- 绑定 MSPM0 DriverLib 的适配层（`icm42688_mspm0`）

核心层通过回调接口注入硬件依赖，因此可在主机上用 GCC 编译并运行单元测试；适配层将 SysConfig 生成的外设符号绑定到核心层。

## 目录结构

```
ICM42688/
├── inc/
│   ├── icm42688_hal.h       平台无关核心 HAL 接口
│   ├── ahrs_hal.h           Mahony AHRS 接口
│   └── icm42688_mspm0.h     MSPM0 适配层接口
├── src/
│   ├── icm42688_hal.c       核心实现（无硬件依赖）
│   ├── ahrs_hal.c           AHRS 实现（无硬件依赖）
│   └── icm42688_mspm0.c     适配层实现（DriverLib + SysConfig）
├── README.md
└── temp/
    ├── tests/
    │   ├── run_tests.ps1                一键主机测试脚本
    │   ├── test_icm42688.c              核心层测试
    │   ├── test_ahrs.c                  AHRS 测试
    │   ├── test_mspm0_adapter.c         适配层测试（含 fake DriverLib）
    │   └── fakes/
    │       └── ti_msp_dl_config.h       主机测试用的 SysConfig 符号桩
    └── task-4-brief.md
```

## 引脚连接

| Signal | MSPM0 Pin | Direction |
|---|---|---|
| SPI1_POCI | PA16 | ICM42688 to MSPM0 |
| SPI1_SCLK | PA17 | MSPM0 to ICM42688 |
| SPI1_PICO | PA18 | MSPM0 to ICM42688 |
| ICM42688_CS | PB20 | MSPM0 to ICM42688, active low |

## SysConfig 配置

### SPI1

- 模式：Controller（主模式）
- 时钟极性/相位：MOTO3，POL0 / PHA0（CPOL=0, CPHA=0）
- 数据位宽：8-bit
- 位序：MSB first
- 速率：1 MHz（`SCR = 15`，BUSCLK 32 MHz）
- CS：软件控制（不用硬件 CS）

SysConfig 会为该外设生成符号 `SPI1_INST`，适配层通过该符号访问 SPI。

### GPIO（CS）

- `PB20`：push-pull 输出
- 初始电平：高（idle，CS 释放态）
- 通过 `DL_GPIO_setPins` / `DL_GPIO_clearPins` 软件拉低/拉高
- 适配层需要 SysConfig 提供 `ICM42688_PORT` 与 `ICM42688_CS_PIN` 两个符号

### TIMG0（AHRS 计时源）

- 实例名（Instance name）：`ICM42688_TIMER`
- 时钟源：BUSCLK 32 MHz
- 预分频：31（即 32 MHz / (31+1) = 1 MHz 计数）
- 计数频率：1 MHz（每个 tick = 1 µs）
- 模式：32-bit periodic / free-running
- 周期：max period（满量程，避免频繁重载）
- 上电启动：Start on power-up
- 中断：不使能

> 上述配置会让 SysConfig 生成符号 `ICM42688_TIMER_INST`。适配层通过 `DL_TimerG_getTimerCount(ICM42688_TIMER_INST)` 直接读取自由运行的计数值作为微秒时间戳，用于 AHRS 的 `dt` 计算。因为定时器自由运行且满量程周期，读取操作不会干扰计数，也无需中断服务。

> SysConfig 还需定义 `CPUCLK_FREQ`（默认 32000000），适配层的 `delay_ms` 据此计算每毫秒的 `delay_cycles` 数。

## EIDE 集成

在 controller 工程中（**不要修改本模块内的 `.eide/eide.yml`**，由使用者在 controller 工程操作）：

1. 将 `../../modules/ICM42688/src` 添加到 srcDirs。
2. 将 `../../modules/ICM42688/inc` 添加到 include paths。
3. 确保 SysConfig 已生成 `ti_msp_dl_config.h`（通常在 controller 工程的生成目录中，已在 include path 内）。

模块本身不修改任何 `.syscfg` 文件，所有外设符号契约由 controller 工程的 SysConfig 提供。

## 初始化序列

控制器 `main()` 中按以下顺序调用：

```c
#include "icm42688_mspm0.h"

static const icm42688_config_t imu_config = {
    .interface_type = ICM42688_INTERFACE_SPI,
    .acc_sample = ICM42688_ACC_SAMPLE_SGN_8G,
    .gyro_sample = ICM42688_GYRO_SAMPLE_SGN_1000DPS,
    .sample_rate = ICM42688_SAMPLE_RATE_1000,
};

SYSCFG_DL_init();
icm42688_mspm0_bind(&imu_config);   /* 内部已调用 ahrs_hal_init(&icm42688_mspm0_timer) */
if (icm42688_init() != ICM42688_STATUS_OK) {
    for (;;) { __WFI(); }
}
ahrs_init();
```

说明：
- `SYSCFG_DL_init()` 必须先调用，完成 SPI1 / GPIO / TIMG0 的硬件初始化。
- `icm42688_mspm0_bind()` 只绑定回调接口指针，不触碰硬件；传入的 `imu_config` 决定量程与 ODR。内部已调用 `ahrs_hal_init(&icm42688_mspm0_timer)` 绑定 TIMG0 计时器，调用方无需重复调用。
- `icm42688_init()` 执行 WHO_AM_I 校验、软复位、唤醒、量程与 ODR 配置；失败则进入死循环。
- `ahrs_init()` 复位四元数并建立时间基线。

## 主循环示例

```c
icm42688_data_t imu;
ahrs_euler_angle_t attitude;
const float dps_to_rad = AHRS_PI / 180.0f;

for (;;) {
    if (icm42688_read(&imu) == ICM42688_STATUS_OK) {
        /* gyro: dps -> rad/s，AHRS 需要弧度制 */
        ahrs_update_6axis(
            imu.gyro_dps.x * dps_to_rad,
            imu.gyro_dps.y * dps_to_rad,
            imu.gyro_dps.z * dps_to_rad,
            imu.acc_g.x,
            imu.acc_g.y,
            imu.acc_g.z);

        ahrs_get_attitude(&attitude);
        /* attitude.roll / pitch / yaw 单位为度 */
    }
}
```

> 建议在静止时调用一次 `icm42688_calibrate_gyro(samples, interval_ms)` 计算陀螺零偏，再进入主循环。

## FreeRTOS 注意事项

如果控制器运行 FreeRTOS，可以将上述主循环放到一个周期任务中，用 `vTaskDelayUntil()` 调度任务节拍。但是：

- **AHRS 的 `dt` 必须来自 TIMG0**（`icm42688_mspm0_timer.get_time_us()`），不要用 `xTaskGetTickCount()`。
- `xTaskGetTickCount()` 分辨率只有 1 ms，对 1000 Hz 采样的 AHRS 远远不够，会导致姿态发散或 NaN。
- SysTick 被 FreeRTOS 占用；TIMG0 是独立外设，不受调度影响，因此用 TIMG0 作为 AHRS 微秒计时源是正确做法。
- 确保其它代码不会重新配置 TIMG0，否则 `ICM42688_TIMER_INST` 的计数语义会改变。

## 六轴局限

本模块只使用加速度计 + 陀螺仪（六轴），**yaw 会随时间漂移**，不是绝对航向。如需绝对航向，需要加入磁力计（九轴）。roll / pitch 在加速度计辅助下可保持长期稳定。

## 上板验证清单

- [ ] `WHO_AM_I`（寄存器 0x75）读取值为 `0x47`
- [ ] 静止时加速度模长约为 1 g
- [ ] 校准后陀螺三轴接近 0 dps
- [ ] roll / pitch 方向与安装朝向一致（不一致则调整轴向映射）

## 故障排查

| 现象 | 排查方向 |
|---|---|
| 所有寄存器读到 `0x00` 或 `0xFF` | 检查 SPI 接线（POCI/SCLK/PICO/CS）、电平、上电 |
| ID 读取错误 | 检查 CS 接线与 SPI 模式（必须 POL0/PHA0） |
| 姿态出现 NaN | 检查 `dt` 范围（1..100000 µs），校准期间传感器必须静止 |
| 姿态方向反了 | 根据安装朝向调整轴向映射 |
| FreeRTOS 下时间异常 | 确认 TIMG0 未被其它代码重新配置；不要用 `xTaskGetTickCount()` 当 `dt` |

## 主机测试

本模块附带主机端单元测试，无需任何硬件即可验证核心层与适配层逻辑：

```powershell
powershell -ExecutionPolicy Bypass -File .\temp\tests\run_tests.ps1
```

脚本依次构建并运行三个测试程序（`test_icm42688`、`test_ahrs`、`test_mspm0_adapter`），任一编译或测试失败立即退出。全部通过时最后一行为：

```text
ICM42688 host tests: PASS
```

测试覆盖：
- `test_icm42688`：核心 HAL 的量程换算、ODR 编码、ID 校验、复位/唤醒流程、12 字节突发读、陀螺校准等。
- `test_ahrs`：Mahony 六轴更新的时间基线、`dt` 边界、零加速度纯陀螺积分、四元数归一化、欧拉角输出、复位等。
- `test_mspm0_adapter`：适配层 SPI 单读/单写/突发读的 CS 时序与字节流、NULL 安全、CS 空闲高、TIMG0 时间戳直读、`delay_ms` 的 `delay_cycles` 计算等。
