# MSPM0G3507 LQFP-48 最终引脚方案

本表与 `NewProject1.syscfg` 及 SysConfig 生成的 `Debug/ti_msp_dl_config.h` 一致。SysConfig 1.24 / SDK 2.05 已验证通过。

## 通信接口

| 功能 | GPIO | 核心板位置 | IOMUX / 复用 | 说明 |
|---|---|---|---|---|
| SPI1_POCI | PA16 | H2-13 | PINCM38 / SPI1_POCI | 可用于扩展板 |
| SPI1_PICO | PA18 | H2-11 | PINCM40 / SPI1_PICO | 修正原图错误 |
| SPI1_SCLK | PA17 | H2-12 | PINCM39 / SPI1_SCLK | 修正原图错误 |
| I2C0_SDA | PA0 | H1-1 | PINCM1 / I2C0_SDA | 100 kHz，外部需要上拉 |
| I2C0_SCL | PA1 | H1-2 | PINCM2 / I2C0_SCL | 100 kHz，板上已有上拉配置 |
| UART0_TX | PA10 | 调试排针 U4 | PINCM21 / UART0_TX | 已接 CH340 |
| UART0_RX | PA11 | 调试排针 U4 | PINCM22 / UART0_RX | 已接 CH340 |
| UART1_TX | PA8 | H1-14 | PINCM19 / UART1_TX | 115200 |
| UART1_RX | PA9 | H1-15 | PINCM20 / UART1_RX | 115200 |
| UART2_TX | PB15 | 未引出 | PINCM32 / UART2_TX | 已接板载 Flash 数据线 |
| UART2_RX | PB16 | 未引出 | PINCM33 / UART2_RX | 已接板载 Flash 时钟线 |
| UART3_TX | PB2 | H1-12 | PINCM15 / UART3_TX | 115200 |
| UART3_RX | PB3 | H1-13 | PINCM16 / UART3_RX | 115200 |

SPI0 与 UART2 虽已按要求写入 SysConfig，但在现有核心板上不能直接从 H1/H2 扩展。扩展板应优先使用 SPI1、I2C0、UART1 和 UART3。

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
| TB6612_STBY | PA23 | H2-6 | PINCM53 / GPIO |

四路 PWM 均为 32 MHz / 1000 = 32 kHz，初始占空比为 0%。

## 编码器输入

| 功能 | GPIO | 核心板位置 | IOMUX / 方式 |
|---|---|---|---|
| ENC1_A | PA12 | H2-17 | PINCM34 / GPIO 双边沿中断 |
| ENC1_B | PA13 | H2-16 | PINCM35 / GPIO 双边沿中断 |
| ENC2_A | PA14 | H2-15 | PINCM36 / GPIO 双边沿中断；板载 LED 同脚 |
| ENC2_B | PA26 | H2-3 | PINCM59 / GPIO 双边沿中断 |
| ENC3_A | PA27 | H2-2 | PINCM60 / GPIO 双边沿中断 |
| ENC3_B | PA28 | H1-3 | PINCM3 / GPIO 双边沿中断 |
| ENC4_A | PA31 | H1-4 | PINCM6 / GPIO 双边沿中断 |
| ENC4_B | PB18 | H1-10 | PINCM44 / GPIO 双边沿中断 |

`ENC4_A` 从原图的 PB17 改到 PA31，因为 PB17 是板载 Flash 的片选脚且未接到 H1/H2。

## SPI DMA

| 方向 | 触发源 | DMA 通道 |
|---|---|---|
| SPI0 RX | DMA_SPI0_RX_TRIG | DMA_CH0 |
| SPI0 TX | DMA_SPI0_TX_TRIG | DMA_CH1 |
| SPI1 RX | DMA_SPI1_RX_TRIG | DMA_CH2 |
| SPI1 TX | DMA_SPI1_TX_TRIG | DMA_CH3 |

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

## 循迹模块说明

这套完整通信、四 PWM、八编码器方案没有剩余的 12 路独立 GPIO 给原来的 12 路数字循迹模块。当前 `line_sensor.c` 在没有 `LINE_SENSOR` SysConfig 实例时会编译为安全空实现，小车不会因悬空输入启动。

扩展 PCB 若仍需 12 路循迹，建议增加 `74HC165`、MCP23017 等输入扩展器，通过 SPI1 或 I2C0读取，不要再直接占用 12 个 MCU GPIO。
