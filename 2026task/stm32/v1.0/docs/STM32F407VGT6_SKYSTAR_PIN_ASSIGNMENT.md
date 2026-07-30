# 天空星 STM32F407VGT6 Pro 小车扩展板引脚配置

更新时间：2026-07-29

## 0. 芯片名称确认

本配置针对梁山派天空星高配版上实际安装的：

```text
STM32F407VGT6，LQFP100，1 MB Flash，168 MHz
```

不存在本项目所述的 `STM32F307VGT6`。CubeMX 中应选择
`STM32F407VGT6`，不能选择 F3 系列芯片。

## 1. 最终推荐分配

### 1.1 TB6612 双轮：只使用 B、C

| 功能 | STM32 引脚 | 复用 | CubeMX 名称 | 上电状态 |
|---|---|---|---|---|
| B/左轮 PWM | PE9 | AF1 | TIM1_CH1 / MOTOR_B_PWM | 0% |
| C/右轮 PWM | PE11 | AF1 | TIM1_CH2 / MOTOR_C_PWM | 0% |
| B_IN1 | PD14 | GPIO Output | MOTOR_B_IN1 | 低 |
| B_IN2 | PD15 | GPIO Output | MOTOR_B_IN2 | 低 |
| C_IN1 | PE2 | GPIO Output | MOTOR_C_IN1 | 低 |
| C_IN2 | PE3 | GPIO Output | MOTOR_C_IN2 | 低 |
| TB6612 STBY | PE0 | GPIO Output | MOTOR_STBY | 低 |
| 载板 ADC | PA1 | ADC1_IN1 | TB6612_ADC | 模拟输入 |

B、C通道暂按“车辆前进”为`IN1=1、IN2=0`，但改线后必须把车轮架空，
逐轮确认方向。PE9/PE11属于同一个TIM1，可同步更新PWM。

驱动载板实物接线改为：

```text
B/左轮：BO1、BO2；编码器 E2A、E2B
C/右轮：CO1、CO2；编码器 E3A、E3B
A、D 通道及 E1、E4 全部留空
```

建议：

- PWM 频率 20 kHz；
- 168 MHz TIM1 时钟下可用 `PSC=0、ARR=8399`；
- STBY 加 10 kΩ 下拉，确保 MCU 复位期间电机休眠；
- IN1/IN2 建议各串 33～100 Ω；
- TB6612 VCC=3.3 V，VM 使用独立电机电源，所有地共地。
- 照片中的 `ADC` 是电机驱动载板额外引出的模拟量，并非 TB6612 芯片原生引脚；
  接到扩展板左排母 `J_EXT_L-4 / PA1`。
- PA1 绝对不能超过 VDDA（正常约 3.3 V）。上电前先用万用表测载板 ADC
  在最高电机电源电压下的输出；未知分压比不得直接写成电池电压。
- MCU 端建议预留 `1 kΩ` 串联和 `100 nF` 对地滤波位；先不额外加分压，
  避免与载板已有分压叠加。

### 1.2 两路编码器预留

即使第一阶段不用闭环，扩展板仍建议保留B、C两个编码器接口。

| 编码器 | STM32 引脚 | 复用 | CubeMX |
|---|---|---|---|
| B/左轮 E2A | PB4 | AF2 | TIM3_CH1 / ENC_LEFT_A |
| B/左轮 E2B | PB5 | AF2 | TIM3_CH2 / ENC_LEFT_B |
| C/右轮 E3A | PD12 | AF2 | TIM4_CH1 / ENC_RIGHT_A |
| C/右轮 E3B | PD13 | AF2 | TIM4_CH2 / ENC_RIGHT_B |

TIM3、TIM4 均设置为 `Encoder Mode / TI1 and TI2`。编码器接口应同时提供
3V3、GND、A、B；如果编码器模块以 5 V 供电，必须先确认输出电平，必要时加
电平转换。

### 1.3 UART 分配

| 模块 | 外设 | MCU TX | MCU RX | 建议波特率 | 接收方式 |
|---|---|---|---|---:|---|
| 调试串口 | UART5 | PC12 | PD2 | 115200 | 中断或 DMA |
| ATK-BLE02 / MW579D 蓝牙 | UART4 | PC10 | PC11 | 115200 | DMA Circular + IDLE |
| 8 路循迹 | USART2 | PA2 | PA3 | 115200 | DMA Circular + IDLE |
| K230 小球位置 | USART3 | PD8 | PD9 | 115200 | DMA Circular + IDLE |
| 平衡舵机 | USART6 | PC6 | PC7 | 依舵机协议 | DMA/中断 |
| 舵机半双工方向预留 | GPIO | PE10 | - | - | 可选 |

接线方向：

```text
蓝牙模块 TX   -> PC11 / UART4_RX
蓝牙模块 RX   <- PC10 / UART4_TX

循迹模块 TX -> PA3 / USART2_RX
循迹模块 RX <- PA2 / USART2_TX

K230 TX       -> PD9 / USART3_RX
K230 RX       <- PD8 / USART3_TX（可暂时不接）

舵机 RX       <- PC6 / USART6_TX
舵机 TX       -> PC7 / USART6_RX
```

重要限制：蓝牙 UART4 的 PC10/PC11 与调试 UART5 的 PC12/PD2 都和板载
TF 卡 SDIO 引脚复用。当前方案不使用 TF 卡，因此两组串口可以同时使用，但：

- TF 卡槽必须保持空；
- CubeMX 不得开启 SDIO；
- 如果实测受板载上拉或卡座支路影响，可拆对应 0 Ω隔离电阻；
- 如果以后需要 TF 卡，调试串口改为核心板调试排针上的
  `USART1 PA9/PA10`，蓝牙也必须改换其它串口或引脚。

蓝牙按旧板所用 `ATK-BLE02 / ATK-MW579D` 配置为 `115200 8N1`。它是 BLE
模块，不是经典蓝牙 SPP，手机端应使用 BLE GATT 工具。底板虽可由 3.3～5 V
供电，UART 逻辑仍是 3.3 V。

固件中的 `atk_ble02.c/.h` 会在 FreeRTOS `defaultTask` 启动 UART4
DMA+IDLE 接收，并提供 256 字节环形缓冲、读取和透明发送 API。旧工程尚未完成
手机 BLE GATT 到 MCU 的端到端验收，因此新板仍应先做 UART 回环，再做手机
GATT 双向测试。

循迹模块若以 5 V 供电，必须测量模块 TX 空闲高电平。高于 3.3 V 时，在模块
TX 到 PA3 之间加入电平转换。K230 UART 是 3.3 V，可直接连接并共地。

舵机型号尚未明确，因此 PCB 应提供：

- 独立舵机电源输入；
- TX、RX、GND；
- PE10 方向控制焊盘；
- TX/RX 合并为单总线的 0 Ω/电阻焊位；
- 3.3 V/5 V 电平转换器可选焊位。

不要直接把大电流舵机挂到核心板 5 V 引脚。

### 1.4 OLED

| 功能 | STM32 引脚 | 复用 | CubeMX |
|---|---|---|---|
| OLED SCL | PB8 | AF4 | I2C1_SCL |
| OLED SDA | PB9 | AF4 | I2C1_SDA |

设置为开漏输出，先使用 100 kHz。扩展板放置两颗 4.7 kΩ 到 3.3 V 的可选
上拉电阻；若 OLED 模块已有上拉，可不装扩展板上拉。

### 1.5 ICM42688

| 功能 | STM32 引脚 | 复用 | CubeMX |
|---|---|---|---|
| SCLK | PB13 | AF5 | SPI2_SCK |
| MISO/SDO | PB14 | AF5 | SPI2_MISO |
| MOSI/SDI | PB15 | AF5 | SPI2_MOSI |
| CS | PE7 | GPIO Output | IMU_CS |
| INT1 | PE8 | GPIO EXTI8 | IMU_INT1 |

初始化参数：

```text
SPI2 Master
Full Duplex
8 bit
MSB first
CPOL Low
CPHA 1 Edge
Software NSS
初始分频 32：42 MHz / 32 = 1.3125 MHz
```

WHO_AM_I 成功并验证布线后再提高 SPI 时钟。CS 上电默认拉高，可增加 10 kΩ
上拉。IMU 接口必须使用 3.3 V。

### 1.6 五个按键

| 按键 | STM32 引脚 | 模式 | 建议用途 |
|---|---|---|---|
| KEY1 | PC0 | GPIO/EXTI0 | 启动/停止 |
| KEY2 | PC1 | GPIO/EXTI1 | 模式切换 |
| KEY3 | PC2 | GPIO/EXTI2 | 目标位置减 |
| KEY4 | PC3 | GPIO/EXTI3 | 目标位置加 |
| KEY5 | PC4 | GPIO/EXTI4 | 确认/急停 |

推荐按键接地、引脚上拉到 3.3 V，按下为低。扩展板可放 10 kΩ 外部上拉和
100 nF 可选硬件消抖。五个按键分别使用 EXTI0～EXTI4，没有 EXTI 线冲突。

急停不能只依赖软件按键；建议另设硬件急停链路，直接拉低 TB6612 STBY 或切断
执行器使能。

## 2. CubeMX 外设清单

| 外设 | 配置 |
|---|---|
| SYS | Serial Wire；保留 PA13/PA14 |
| RCC | HSE Crystal；需要 RTC 时启用 LSE |
| TIM1 | PWM Generation CH1/CH2，20 kHz |
| TIM3 | Encoder Mode TI1/TI2，16 bit |
| TIM4 | Encoder Mode TI1/TI2，16 bit |
| UART4 | Async 115200 8N1，ATK-BLE02；RX DMA Circular |
| UART5 | Async 115200 8N1，调试 |
| USART2 | Async 115200 8N1，循迹 |
| USART3 | Async 115200 8N1，K230 |
| USART6 | Async，舵机波特率待定 |
| I2C1 | 100 kHz 起步 |
| SPI2 | Mode 0，1.3125 MHz 起步 |
| GPIO | 电机方向、STBY、IMU CS、5 个按键、可选 SERVO_DIR |

建议系统时钟：

```text
HSE = 8 MHz
PLL_M = 8
PLL_N = 336
PLL_P = 2
PLL_Q = 7
SYSCLK = 168 MHz
AHB = 168 MHz
APB1 = 42 MHz
APB2 = 84 MHz
```

APB1/APB2 分频不为 1 时，定时器时钟会自动乘 2，因此 TIM1 为 168 MHz，
TIM3/TIM4 为 84 MHz。

## 3. DMA 建议

优先级由高到低：

1. K230 USART3_RX；
2. 循迹 USART2_RX；
3. 舵机 USART6_RX/TX；
4. SPI2 RX/TX；
5. 蓝牙 UART4_RX；
6. 调试 UART5。

一种无主要 Stream 冲突的方案：

| 信号 | DMA |
|---|---|
| UART4_RX | DMA1 Stream2 Channel4 |
| UART5_RX | DMA1 Stream0 Channel4 |
| UART5_TX | DMA1 Stream7 Channel4 |
| USART3_RX | DMA1 Stream1 Channel4 |
| USART2_RX | DMA1 Stream5 Channel4 |
| USART2_TX | DMA1 Stream6 Channel4 |
| SPI2_RX | DMA1 Stream3 Channel0 |
| SPI2_TX | DMA1 Stream4 Channel0 |
| USART6 RX/TX | 由 CubeMX 在 DMA2 Channel5 的可用 Stream 中选择 |

USART3_TX 不必使用 DMA，因为 STM32 给 K230 的数据很少；使用阻塞短包或发送
中断即可。UART4_TX 使用普通阻塞短包或发送中断，不配置 DMA，因为可用的
DMA1 Stream4 已分配给 SPI2_TX。最终仍以当前 CubeMX 版本的冲突检查为准。

## 4. PCB 接口建议

### TB6612

```text
3V3, GND, STBY,
B_PWM, B_IN1, B_IN2,
C_PWM, C_IN1, C_IN2,
ADC
```

VM、电机输出和大电流地不要通过核心板排针。每颗驱动附近放置 100 nF 逻辑
去耦和足够的 VM 大电容，电机回流与 IMU/OLED 地在电源入口附近单点汇合。

### UART

每组 UART 接口至少放置：

```text
VCC, GND, MCU_TX, MCU_RX
```

GND 应靠近信号脚；TX/RX 可串 33～100 Ω；丝印必须从 MCU 视角标明
`MCU_TX/MCU_RX`，避免模块视角混淆。

### IMU

ICM42688 应远离：

- TB6612；
- 电机和电机线；
- 舵机电源；
- DCDC 电感；
- 板边振动最强位置。

放在车体刚性较好、接近旋转中心的位置。SPI 走线短、等长不是硬要求，但应有
连续参考地，CS 和 SCK 避免与电机 PWM 平行长距离走线。

## 5. 明确禁止或保留的核心板引脚

| 引脚 | 原因 |
|---|---|
| PA0 | 板载用户按键 |
| PB2 | BOOT1/板载用户 LED |
| PA4～PA7 | 高配版板载 SPI Flash / SPI1 |
| PA11、PA12 | 板载 USB Device |
| PA13、PA14 | SWDIO、SWCLK |
| PC8、PC9 | 板载 TF 卡 SDIO，本方案保持不用 |
| PC10、PC11 | 本方案仅在禁用 TF 时作为蓝牙 UART4 |
| PC12、PD2 | 本方案仅在禁用 TF 时作为调试 UART5 |
| 晶振相关引脚 | 板载高速/低速晶振，禁止复用 |

保留标准 SWD：3V3/VTref、GND、SWDIO、SWCLK、NRST；可选 SWO。NRST 不接
大电容，避免影响下载和复位。

## 6. 引脚占用总表

```text
PA2  USART2_TX        循迹
PA3  USART2_RX        循迹

PB4  TIM3_CH1         B轮/E2A/左轮编码器A
PB5  TIM3_CH2         B轮/E2B/左轮编码器B
PB8  I2C1_SCL         OLED
PB9  I2C1_SDA         OLED
PB13 SPI2_SCK         ICM42688
PB14 SPI2_MISO        ICM42688
PB15 SPI2_MOSI        ICM42688

PC0  KEY1
PC1  KEY2
PC2  KEY3
PC3  KEY4
PC4  KEY5
PC6  USART6_TX        舵机
PC7  USART6_RX        舵机
PC10 UART4_TX         蓝牙（禁用TF）
PC11 UART4_RX         蓝牙（禁用TF）
PC12 UART5_TX         调试（禁用TF）

PA1  ADC1_IN1         TB6612载板ADC

PD2  UART5_RX         调试（禁用TF）
PD8  USART3_TX        K230
PD9  USART3_RX        K230
PD12 TIM4_CH1         C轮/E3A/右轮编码器A
PD13 TIM4_CH2         C轮/E3B/右轮编码器B
PD14 GPIO             B_IN1
PD15 GPIO             B_IN2

PE0  GPIO             TB6612_STBY
PE2  GPIO             C_IN1
PE3  GPIO             C_IN2
PE7  GPIO             ICM42688_CS
PE8  EXTI8            ICM42688_INT1
PE9  TIM1_CH1         B_PWM
PE10 GPIO             SERVO_DIR（可选）
PE11 TIM1_CH2         C_PWM
```

## 7. 依据

扩展板两只 2x20 排母的完整针号、接口丝印、蓝牙 UART4、调试 UART5，以及
预留 I2C2、共享 SPI2 方案见：

`STM32F407_SKYSTAR_PCB_SILK_AND_RESERVED_BUSES.md`

- 本地核心板规格书：
  `C:\Users\35336\Desktop\diansai\C32710423_开发板_LCKFB-LSPI-SKYSTAR-STM32F407VGT6-PRO_规格书_WJ1510728.PDF`
- 旧硬件迁移总结：
  `C:\Users\35336\Desktop\diansai\OLD_PCB_MODULES_AND_STM32F407_MIGRATION.md`
- ST STM32F407VG 产品页：
  https://www.st.com/en/microcontrollers-microprocessors/stm32f407vg.html
- ST RM0090：
  https://www.st.com/resource/en/reference_manual/dm00031020-stm32f405415xx-stm32f407417xx-stm32f427437xx-and-stm32f429439xx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf
- 天空星核心板原理图说明：
  https://wiki.lckfb.com/zh-hans/tkx/hardware/schematic.html
- 天空星板载 SPI Flash 连接：
  https://wiki.lckfb.com/zh-hans/tkx/tkx-stm32f407vxt6/beginner/spi.html
