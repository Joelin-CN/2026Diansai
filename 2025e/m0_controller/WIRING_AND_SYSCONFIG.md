# MSPM0G3507 LQFP-48 最终引脚方案

`NewProject1.syscfg` 是硬件与外设配置的唯一权威来源；`Debug/ti_msp_dl_config.c` 和 `Debug/ti_msp_dl_config.h` 均为生成文件，不得手工编辑。固定工具版本为 SysConfig 1.26.2+4477 和仓库 SDK 2.10.00.04。已审查生成映射，但尚未完成目标板烧录、电气连接和硬件运行验证。构建、运行限制和模块扩展门槛见 [README.md](README.md)，配套引脚总览图见 [MSPM0G3507_PINOUT_SUMMARY.png](docs/MSPM0G3507_PINOUT_SUMMARY.png)。

## 系统时钟与定时器

- CPU / SYSOSC / BUSCLK：32 MHz；HFXT 和 SYSPLL 均禁用。该 32 MHz 时钟用于 CPU、PWM、TIMG0、UART、SPI 和 I2C。
- MFCLK：固定 4 MHz，仅为 TIMG12 启用。
- TIMG0 / CONTROL_TIMER：使用 32 MHz BUSCLK，周期 2 ms，装载值 63999，周期模式，在 ZERO 事件产生中断，对应 500 Hz 控制节拍。初始化后保持停止，由 `control` 任务启动。
- TIMG12 / ICM42688_TIMER：使用 MFCLK / 4，频率 1 MHz，装载值 4294967295，周期模式，无中断且不自动启动。它预留为微秒时基，但当前应用既未启动也未读取该定时器。

`PlatformTime_GetUs32()` 当前仍返回 `0`，因此 TIMG12 尚未集成为应用时间戳；尚未在目标板测量实际频率或周期。

## 通信接口

| 功能 | GPIO | 核心板位置 | IOMUX / 复用 | 说明 |
|---|---|---|---|---|
| ICM42688_CS | PB20 | H1-8 | PINCM48 / GPIO 输出 | 软件片选，空闲时保持高电平 |
| SPI1_POCI / ICM42688_SDO | PA16 | H2-13 | PINCM38 / SPI1_POCI | ICM42688 传感器输出、主控输入 |
| SPI1_PICO / ICM42688_SDI | PA18 | H2-11 | PINCM40 / SPI1_PICO | ICM42688 主控输出、传感器输入 |
| SPI1_SCLK / ICM42688_SCLK | PA17 | H2-12 | PINCM39 / SPI1_SCLK | 1 MHz，SPI Mode 0 |
| I2C0_SDA | PA0 | H1-1 | PINCM1 / I2C0_SDA | 100 kHz；MCP23017 使用，可在硬件上与 3.3 V OLED 共用；OLED 源码当前未链接 |
| I2C0_SCL | PA1 | H1-2 | PINCM2 / I2C0_SCL | 100 kHz；共享设备必须使用 3.3 V 并避免地址冲突 |
| UART0_TX | PA10 | 调试排针 U4 | PINCM21 / UART0_TX | 已接 CH340 |
| UART0_RX | PA11 | 调试排针 U4 | PINCM22 / UART0_RX | 已接 CH340 |
| UART1_TX | PA8 | H1-14 | PINCM19 / UART1_TX | 115200 |
| UART1_RX | PA9 | H1-15 | PINCM20 / UART1_RX | 115200 |
| UART3_TX | PB2 | H1-12 | PINCM15 / UART3_TX | 115200 |
| UART3_RX | PB3 | H1-13 | PINCM16 / UART3_RX | 115200 |

SPI0 未启用，因为 PA4/PA5/PA6 与核心板时钟网络共用；UART2 未启用，因为 PB15/PB16 与板载 Flash 网络共用。

### ICM42688 接线与片选

单个 ICM42688 使用 SPI1，连接如下：

| ICM42688 引脚 | MSPM0G3507 | 核心板位置 | 说明 |
|---|---|---|---|
| VCC | 3.3 V | 3.3 V | 不可接 5 V |
| GND | GND | GND | 与电机控制板共地 |
| SCLK | PA17 | H2-12 | SPI1 时钟 |
| SDI / MOSI | PA18 | H2-11 | SPI1 PICO |
| SDO / MISO | PA16 | H2-13 | SPI1 POCI |
| CS | PB20 | H1-8 | 普通 GPIO 软件片选，低电平有效 |

SPI1 在 SysConfig 中设置为 Controller、8 bit、MSB first、Mode 0、1 MHz，并使用不包含外设片选的 `MOTO3` 帧格式。这里的 `MOTO3` 表示 SPI 外设只管理 SCLK/PICO/POCI 三根信号线，完整接线仍是包含软件 CS 的标准四线 SPI。PB20 独立配置为 `ICM42688_CS` GPIO 输出，初始值为高。这样寄存器地址和后续数据可以保持在同一次 CS 低电平事务中，不会与电机、编码器、UART、I2C 或 SWD 引脚冲突。

## TB6612 四电机

| 功能 | GPIO | 核心板位置 | IOMUX / 复用 |
|---|---|---|---|
| M1_PWM | PA21 | H2-8 | PINCM46 / TIMA0_CCP0 |
| M2_PWM | PA22 | H2-7 | PINCM47 / TIMA0_CCP1 |
| M3_PWM | PA24 | H2-5 | PINCM54 / TIMA1_CCP1 |
| M4_PWM | PA25 | H2-4 | PINCM55 / TIMA0_CCP3 |
| M1_IN1 | PB6 | H1-16 | PINCM23 / GPIO |
| M1_IN2 | PB7 | H1-17 | PINCM24 / GPIO |
| M2_IN1 | PB8 | H2-10 | PINCM25 / GPIO |
| M2_IN2 | PA7 | H1-11 | PINCM14 / GPIO |
| M3_IN1 | PA15 | H2-14 | PINCM37 / GPIO |
| M3_IN2 | PB9 | H2-9 | PINCM26 / GPIO |
| M4_IN1 | PB19 | H1-9 | PINCM45 / GPIO |
| M4_IN2 | PB24 | H1-7 | PINCM52 / GPIO |
| TB6612_STBY | 硬件上拉 3.3 V | — | 不占用 MCU GPIO；PA23 保留空闲 |

四路 PWM 均为 32 MHz / 1000 = 32 kHz，初始占空比为 0%。

## 编码器输入

| 功能 | GPIO | 核心板位置 | IOMUX / 方式 |
|---|---|---|---|
| ENC1_A | PA12 | H2-17 | PINCM34 / GPIO 双边沿中断 |
| ENC1_B | PA13 | H2-16 | PINCM35 / GPIO 双边沿中断 |
| ENC2_A | PA2 | H1-6 | PINCM7 / GPIO 双边沿中断 |
| ENC2_B | PA26 | H2-3 | PINCM59 / GPIO 双边沿中断 |
| ENC3_A | PA27 | H2-2 | PINCM60 / GPIO 双边沿中断 |
| ENC3_B | PA28 | H1-3 | PINCM3 / GPIO 双边沿中断 |
| ENC4_A | PA31 | H1-4 | PINCM6 / GPIO 双边沿中断 |
| ENC4_B | PB18 | H1-10 | PINCM44 / GPIO 双边沿中断 |

`ENC2_A` 从 PA14 改到 PA2，避开板载 LED；`ENC4_A` 从 PB17 改到 PA31，因为 PB17 是板载 Flash 的片选脚且未接到 H1/H2。

## SPI DMA

| 方向 | 触发源 | DMA 通道 |
|---|---|---|
| SPI1 RX | DMA_SPI1_RX_TRIG | DMA_CH2 |
| SPI1 TX | DMA_SPI1_TX_TRIG | DMA_CH3 |

DMA CH2/CH3 由统一 SPI1 传输层独占管理。同一 SPI1 总线上的新设备可以使用该传输服务，但不得独立重配置、抢占、并发驱动这些通道，也不得建立其他 DMA 所有者。所有设备仍必须使用独立片选，并保持 ICM42688 的完整 CS 低电平事务。

## 调试保留

| 功能 | GPIO | 位置 |
|---|---|---|
| SWDIO | PA19 | 调试排针 U4 |
| SWCLK | PA20 | 调试排针 U4 |
| NRST | NRST | H1-5 / 复位按键 |

## 相对原图的必要修正

| 原图 | 最终配置 | 原因 |
|---|---|---|
| M3_IN2 = PB11 | PB9 | PB11 不存在于 LQFP-48 |
| M4_IN1 = PB12 | PB19 | PB12 不存在于 LQFP-48 |
| M4_IN2 = PB13 | PB24 | PB13 不存在于 LQFP-48 |
| SPI1_SCLK = PA15 | PA17 | PA15 实际不是 SPI1_SCLK |
| SPI1_PICO = PA17 | PA18 | PA17 实际是 SPI1_SCLK |
| M3_IN1 = PA18 | PA15 | 为 SPI1_PICO 让出 PA18 |
| ENC4_A = PB17 | PA31 | PB17 已接板载 Flash CS 且未引出 |
| ENC2_A = PA14 | PA2 | PA14 与板载 LED 共用 |
| SPI1_CS0 = PB20 | ICM42688_CS = PB20 | ICM42688 需要软件控制整个寄存器事务的片选时序 |

## 循迹模块说明

这套完整通信、四 PWM、八编码器方案没有剩余的 12 路独立 GPIO。循迹输入固定使用 MCP23017 I2C GPIO 扩展器，不新增 MCU，也不修改 SysConfig。

| MCP23017 | 连接 |
|---|---|
| VCC / GND | 3.3 V 外置电源 / 与 MSPM0 和传感器共地 |
| SDA / SCL | PA0 / PA1，使用 I2C0；硬件可与 3.3 V OLED 共用，但 OLED 当前未进入目标固件 |
| A0 / A1 / A2 | 全接 GND，7 位 I2C 地址 `0x20` |
| RESET | 上拉至 3.3 V |
| GPA0~GPA7 | 循迹 S1~S8 |
| GPB0~GPB3 | 循迹 S9~S12 |
| INTA / INTB | 默认不接，软件轮询读取；PA23 可留作后续中断输入 |

SDA/SCL 上拉和所有 MCP23017 GPIO 输入必须为 3.3 V 逻辑。若循迹模块输出 5 V，必须先做电平转换或分压，不能直接接 MCP23017。

## 当前验证边界

已验证：使用固定工具组合完成生成；生成的 TIMG0、TIMG12 以及本文记录的全部外设和 GPIO 映射已完成检查，映射审查未发现重映射；EIDE/AC6 全量编译和链接成功。

未验证：HEX 烧录、目标板启动/复位、实际时钟与定时器精度、外设与控制环硬件行为、接线、电平、EMI、电源完整性和最坏情况执行时间。

本文描述当前配置与接线要求，不构成硬件合格验证的证明。
