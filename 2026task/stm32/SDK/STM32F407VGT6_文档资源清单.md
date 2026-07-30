# STM32F407VGT6/LQFP100 文档资源清单

本文档列出了STM32F407VGT6开发所需的关键手册和API文档的官方下载链接。

## 芯片核心文档

### 1. STM32F407VG 数据手册 (Datasheet)
- **文档编号**: DS8626
- **描述**: STM32F407VG芯片规格书，包含LQFP100封装的引脚定义、电气特性、时序参数等
- **官方下载链接**: https://www.st.com/resource/en/datasheet/stm32f407vg.pdf
- **页面链接**: https://www.st.com/en/microcontrollers-microprocessors/stm32f407vg.html

### 2. STM32F4xx 参考手册 (Reference Manual)
- **文档编号**: RM0090
- **描述**: STM32F405/415/407/417/427/437/429/439系列的完整参考手册，包含所有外设寄存器详细说明
- **官方下载链接**: https://www.st.com/resource/en/reference_manual/rm0090-stm32f405415-stm32f407417-stm32f427437-and-stm32f429439-advanced-armbased-32bit-mcus-stmicroelectronics.pdf
- **文档大小**: 约15-20 MB，1700+页

## HAL库开发文档

### 3. STM32F4 HAL和LL驱动说明 (HAL API Manual)
- **文档编号**: UM1725
- **描述**: STM32CubeF4 HAL（硬件抽象层）和LL（低层）驱动的完整API手册
- **包含模块**: GPIO, UART, SPI, I2C, ADC, DAC, Timer, DMA, USB, Ethernet等所有外设HAL API
- **官方下载**: 访问 https://www.st.com/en/embedded-software/stm32cubef4.html → Resources → Documentation → 查找 UM1725
- **直接搜索**: 在ST官网搜索 "UM1725" 或 "Description of STM32F4 HAL and Low-layer drivers"

### 4. STM32CubeF4入门指南
- **文档编号**: UM1730
- **描述**: STM32CubeF4 MCU固件包使用入门
- **官方下载**: 访问 https://www.st.com/en/embedded-software/stm32cubef4.html → Resources → Documentation → 查找 UM1730

## FreeRTOS (CMSIS-RTOS2) 文档

### 5. CMSIS-RTOS2 API 参考手册
- **版本**: CMSIS Version 5.x / CMSIS-RTOS2
- **描述**: ARM官方CMSIS-RTOS2 API规范，FreeRTOS的CMSIS封装接口
- **官方文档链接**: 
  - ARM官网: https://arm-software.github.io/CMSIS_5/RTOS2/html/index.html
  - 在线HTML文档: https://arm-software.github.io/CMSIS_5/RTOS2/html/group__CMSIS__RTOS.html
- **PDF下载**: 访问 https://github.com/ARM-software/CMSIS_5 → Releases → 下载完整文档包

### 6. FreeRTOS参考手册
- **描述**: FreeRTOS内核API参考（如果需要直接使用FreeRTOS原生API）
- **官方网站**: https://www.freertos.org/a00106.html
- **API参考**: https://www.freertos.org/a00106.html
- **官方书籍**: "Mastering the FreeRTOS Real Time Kernel" (免费PDF) - https://www.freertos.org/Documentation/RTOS_book.html

## 应用笔记和示例

### 7. STM32F4 + FreeRTOS 应用笔记
- **文档**: AN3983 - Running FreeRTOS on STM32 microcontrollers
- **下载**: 在ST官网搜索 "AN3983"

### 8. STM32CubeF4 固件包（包含示例代码）
- **下载页面**: https://www.st.com/en/embedded-software/stm32cubef4.html
- **包含内容**: 
  - HAL/LL驱动源码及注释
  - FreeRTOS（CMSIS-RTOS2封装）源码
  - 大量外设示例代码
  - 中间件（USB、TCP/IP、文件系统）

## 下载方法

### 方法1: ST官网直接下载
1. 访问 https://www.st.com
2. 在搜索框输入文档编号（如 RM0090, UM1725, DS8626）
3. 在搜索结果中找到对应文档并下载

### 方法2: 通过产品页面下载
1. 访问 STM32F407VG 产品页: https://www.st.com/en/microcontrollers-microprocessors/stm32f407vg.html
2. 点击 "Resources" 或 "Documentation" 标签
3. 下载所需的数据手册和参考手册

### 方法3: 使用STM32CubeMX
1. 安装STM32CubeMX工具
2. 在工具内可以直接访问和下载所有相关文档

### 方法4: GitHub镜像（部分文档）
某些文档在GitHub上有社区维护的镜像，搜索相关仓库。

## 备注

- 所有文档均为官方最新版本，建议定期检查更新
- 参考手册（RM0090）是最核心的文档，约1700页，涵盖所有寄存器细节
- HAL API手册（UM1725）是使用STM32Cube HAL库开发的必备文档
- CMSIS-RTOS2是推荐的FreeRTOS使用方式，提供了统一的RTOS抽象层

## 推荐阅读顺序

1. **快速入门**: STM32F407VG Datasheet（关注引脚定义和封装）
2. **外设开发**: RM0090 Reference Manual（根据需要查阅特定外设章节）
3. **HAL编程**: UM1725 HAL API Manual（学习HAL库函数）
4. **RTOS开发**: CMSIS-RTOS2 API + FreeRTOS Kernel Guide

---

*文档生成时间: 2026-07-29*
*STM32F407VGT6: 32-bit ARM Cortex-M4F, 168MHz, 1MB Flash, 192KB RAM, LQFP100*
