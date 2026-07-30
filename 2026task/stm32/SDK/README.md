# STM32F407VGT6 SDK 文档下载指南

由于网络原因，从ST官网直接下载可能较慢。以下提供多种下载方法：

## 🚀 快速下载方法

### 方法1: 使用浏览器下载（推荐）

打开浏览器，直接访问以下链接下载（右键另存为）：

1. **STM32F407VG 数据手册** (3-4 MB)
   ```
   https://www.st.com/resource/en/datasheet/stm32f407vg.pdf
   ```
   保存为: `STM32F407VG_Datasheet.pdf`

2. **STM32F4xx 参考手册 RM0090** (15-20 MB, 1700+页)
   ```
   https://www.st.com/resource/en/reference_manual/rm0090-stm32f405415-stm32f407417-stm32f427437-and-stm32f429439-advanced-armbased-32bit-mcus-stmicroelectronics.pdf
   ```
   保存为: `RM0090_STM32F407_Reference_Manual.pdf`

### 方法2: 通过产品页面下载

访问STM32F407VG产品页面，从Resources标签下载：
```
https://www.st.com/en/microcontrollers-microprocessors/stm32f407vg.html
```

点击页面中的 **"Resources"** → **"Technical Documentation"**

### 方法3: 使用下载工具

如果浏览器下载慢，可以使用以下工具：
- **IDM (Internet Download Manager)** - Windows推荐
- **迅雷** - 支持多线程下载
- **wget** 或 **aria2c** - 命令行工具

### 方法4: 国内镜像站（可选）

搜索以下关键词可能找到国内镜像：
- "STM32F407 参考手册 下载"
- "RM0090 中文版"
- "STM32F4 HAL手册 下载"

## 📚 必需文档清单

### 核心文档（优先级：⭐⭐⭐⭐⭐）

- [ ] **STM32F407VG_Datasheet.pdf** - 芯片数据手册
- [ ] **RM0090_STM32F407_Reference_Manual.pdf** - 参考手册（最重要！）
- [ ] **UM1725_STM32F4_HAL_Driver_Manual.pdf** - HAL库API手册

### RTOS文档（优先级：⭐⭐⭐⭐）

- [ ] **CMSIS-RTOS2_API_Reference.pdf** - CMSIS-RTOS2 API手册

### 补充文档（优先级：⭐⭐⭐）

- [ ] **UM1730_STM32CubeF4_Getting_Started.pdf** - 入门指南
- [ ] **AN3983_FreeRTOS_on_STM32.pdf** - FreeRTOS应用笔记

## 🔍 详细下载步骤

### HAL库文档 (UM1725)

1. 访问: https://www.st.com/en/embedded-software/stm32cubef4.html
2. 滚动到页面下方找到 **"Documentation"** 部分
3. 查找并下载 **UM1725** - "Description of STM32F4 HAL and Low-layer drivers"

或者直接搜索：
```
site:st.com UM1725 STM32F4 HAL
```

### CMSIS-RTOS2 文档

**在线查看**（推荐）:
```
https://arm-software.github.io/CMSIS_5/RTOS2/html/index.html
```

**下载PDF版本**:
1. 访问: https://github.com/ARM-software/CMSIS_5
2. 点击 **Releases** 或 **Documentation**
3. 下载完整文档包

**CMSIS_5完整包下载**:
```
git clone https://github.com/ARM-software/CMSIS_5.git
```
文档在 `CMSIS_5/CMSIS/Documentation/` 目录

### FreeRTOS官方文档

**FreeRTOS官方书籍**（免费PDF）:
```
https://www.freertos.org/fr-content-src/uploads/2018/07/161204_Mastering_the_FreeRTOS_Real_Time_Kernel-A_Hands-On_Tutorial_Guide.pdf
```

**FreeRTOS API参考**（在线）:
```
https://www.freertos.org/a00106.html
```

## 🛠️ STM32CubeMX方式（最简单）

如果上述方法都不方便，推荐安装 **STM32CubeMX** 工具：

1. 下载 STM32CubeMX: https://www.st.com/en/development-tools/stm32cubemx.html
2. 安装后，工具内置文档查看器
3. 可以直接在工具内访问所有文档
4. 自动下载 STM32CubeF4 固件包（包含HAL源码和示例）

## 📖 文档说明

### 1. STM32F407VG Datasheet (数据手册)
- **页数**: 约120-150页
- **内容**: 
  - LQFP100引脚定义（重要！）
  - 电气特性参数
  - 时序图
  - 封装尺寸
- **用途**: 硬件设计、引脚配置

### 2. RM0090 Reference Manual (参考手册)
- **页数**: 约1700页
- **内容**: 
  - 所有外设详细说明
  - 寄存器定义（每个bit的含义）
  - 工作原理和配置流程
- **用途**: 深入理解外设、寄存器级编程
- **章节导航**:
  - Ch2: Memory mapping
  - Ch6: GPIO
  - Ch9-14: Timers
  - Ch24-26: USART/SPI/I2C
  - Ch13: ADC/DAC

### 3. UM1725 HAL Driver Manual (HAL库手册)
- **页数**: 约2000页
- **内容**: 
  - 所有HAL API函数说明
  - 数据结构定义
  - 使用示例
- **用途**: HAL库开发必备
- **模块**: 
  - HAL_GPIO_*
  - HAL_UART_*
  - HAL_TIM_*
  - HAL_ADC_*
  - 等等

### 4. CMSIS-RTOS2 API Reference
- **页数**: 约100-200页
- **内容**:
  - osThreadNew / osThreadTerminate
  - osMutex* / osSemaphore*
  - osMessageQueue*
  - osTimer*
  - osEventFlags*
- **用途**: FreeRTOS标准化API编程

## 💡 使用建议

### 日常开发优先级

1. **快速查阅**: RM0090参考手册（PDF书签导航）
2. **API查询**: UM1725 HAL手册（搜索函数名）
3. **RTOS编程**: CMSIS-RTOS2在线文档（网页搜索快）
4. **引脚确认**: Datasheet LQFP100引脚表

### 推荐工具

- **PDF阅读器**: 
  - Adobe Acrobat Reader DC（支持大文档）
  - Foxit Reader（速度快）
  - SumatraPDF（轻量级）
  
- **快速查找**: 
  - PDF书签功能
  - Ctrl+F 全文搜索
  - 多标签页对照阅读

## 📦 STM32CubeF4固件包

建议下载完整固件包，包含：
- HAL/LL驱动源码
- CMSIS-RTOS2（FreeRTOS封装）
- 中间件（USB, TCP/IP, FAT文件系统）
- 大量示例代码

**下载**: https://www.st.com/en/embedded-software/stm32cubef4.html
**文件**: `STM32Cube_FW_F4_VX.XX.X.zip` (约500MB)

或使用STM32CubeMX自动下载。

## ❓ 常见问题

**Q: 下载速度很慢怎么办？**
A: 使用浏览器下载，分多次下载，或使用下载工具（IDM/迅雷）

**Q: 有没有中文版文档？**
A: ST官方主要提供英文版。部分中文翻译版可在社区找到，但建议以英文版为准。

**Q: 文档太大，打不开？**
A: RM0090约20MB，需要64位PDF阅读器。推荐Adobe Acrobat或Foxit。

**Q: 只需要部分内容，不想下载全部？**
A: 在线查看或使用STM32CubeMX内置文档浏览器。

## 🔗 有用链接

- ST官方社区: https://community.st.com/
- STM32中文社区: https://www.stmcu.org.cn/
- ARM CMSIS文档: https://arm-software.github.io/CMSIS_5/
- FreeRTOS官网: https://www.freertos.org/

---

**提示**: 文档下载完成后，请将文件放在本目录下，方便查阅。建议的文件名在上方"必需文档清单"中已列出。
