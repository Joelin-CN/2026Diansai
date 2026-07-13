# ICM42688 MSPM0G3507 驱动说明

本模块提供单个 ICM42688-P 的 SPI 底层驱动，目前独立存放，尚未加入 `controller/mspm0` 主控工程。

## 目录

- `inc/icm42688.h`：公开数据类型、状态码和函数接口。
- `src/icm42688.c`：MSPM0 DriverLib SPI/GPIO 通信、芯片初始化、数据读取、单位换算和陀螺仪零偏校准。

## 硬件配置

驱动使用主控 `NewProject1.syscfg` 中的 SPI1 和软件片选：

| 信号 | MSPM0G3507 | 核心板位置 |
|---|---|---|
| ICM42688 SCLK | PA17 | H2-12 |
| ICM42688 SDI / MOSI | PA18 | H2-11 |
| ICM42688 SDO / MISO | PA16 | H2-13 |
| ICM42688 CS | PB20 | H1-8 |

SPI1 参数为 Controller、Mode 0、8 bit、MSB first、1 MHz，并采用不包含硬件片选的 `MOTO3` 帧格式。这里的 `MOTO3` 仅表示外设管理 SCLK/PICO/POCI，PB20 仍作为普通 GPIO 软件片选，所以物理连接仍是标准四线 SPI。驱动依赖 SysConfig 生成的 `SPI1_INST`、`ICM42688_PORT`、`ICM42688_CS_PIN` 和 `CPUCLK_FREQ` 宏。

## 初始化配置

`ICM42688_Init()` 执行以下操作：

1. 软件复位传感器。
2. 读取 `WHO_AM_I`，必须等于 `0x47`。
3. 让加速度计和陀螺仪进入低噪声模式。
4. 配置加速度计为 `+/-8 g, 1 kHz`。
5. 配置陀螺仪为 `+/-1000 dps, 1 kHz`。
6. 回读关键寄存器验证配置。

这两个量程配置的寄存器值都是 `0x26`。不要使用旧 STM32 工程中“枚举值直接左移”的写法，旧枚举顺序与 ICM42688 的 FS_SEL 寄存器编码相反。

## 数据读取

`ICM42688_ReadRaw()` 从 `TEMP_DATA1` 开始一次连续读取 14 字节，得到同一次 SPI 事务中的温度、三轴加速度和三轴角速度原始值。

`ICM42688_Read()` 进一步转换成：

- 温度：摄氏度；
- 加速度：g；
- 角速度：dps，并自动扣除已保存的陀螺仪零偏。

在当前量程下：

```text
accel_g  = raw / 4096.0
gyro_dps = raw / 32.768 - bias
```

## 陀螺仪校准

`ICM42688_CalibrateGyro(sampleCount, sampleIntervalMs)` 会平均三轴静态角速度并保存零偏。校准期间车辆必须完全静止。推荐启动后使用约 1000 个样本、2 ms 间隔：

```c
ICM42688_Status status;

status = ICM42688_Init();
if (status == ICM42688_STATUS_OK) {
    status = ICM42688_CalibrateGyro(1000U, 2U);
}
```

## 将来接入主控的顺序

1. 将 `modules/ICM42688/inc` 加入头文件搜索路径。
2. 将 `modules/ICM42688/src/icm42688.c` 加入编译源文件。
3. 在 `SYSCFG_DL_init()` 之后调用 `ICM42688_Init()`。
4. 车辆保持静止，调用 `ICM42688_CalibrateGyro()`。
5. 在固定周期控制循环中调用 `ICM42688_Read()`。
6. 水平安装时通常使用 `gyroZDps` 作为车体转向角速度；实际轴和正负号必须根据传感器安装方向验证。

本模块只负责传感器驱动和数据校准，不包含循迹、TB6612 电机控制、AHRS 或转向控制算法。
