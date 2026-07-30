# UART0 调试测试指南

## 概述

本目录包含UART0串口调试功能的测试程序和工具模块。UART0已配置为115200波特率，用于输出运行日志。

## 硬件连接

### UART0引脚定义
- **TX (发送)**: PA10 - 连接到USB转串口模块的RX
- **RX (接收)**: PA11 - 连接到USB转串口模块的TX（本测试未使用）
- **GND**: 必须与USB转串口模块共地

### 接线示意
```
MSPM0G3507          USB-to-Serial
-----------         -------------
PA10 (TX)   ------>  RX
PA11 (RX)   <------  TX (可选)
GND         -------  GND
```

## 快速测试步骤

### 1. 准备硬件
1. 连接USB转串口模块到UART0引脚（见上述接线）
2. 将USB转串口模块插入电脑
3. 确认串口号（Windows: COM口，Linux: /dev/ttyUSB*）

### 2. 打开串口终端
使用任意串口工具，配置如下：
- **波特率**: 115200
- **数据位**: 8
- **校验位**: None
- **停止位**: 1
- **流控**: None

推荐工具：
- Windows: PuTTY, Tera Term, SecureCRT
- Linux: minicom, screen
- 跨平台: Arduino Serial Monitor

### 3. 运行测试脚本

**Windows PowerShell:**
```powershell
.\test_uart0.ps1
```

**Linux/Git Bash:**
```bash
bash test_uart0.sh
```

脚本会自动：
- 备份原始 `src/main.c` 到 `src/main.c.backup`
- 将测试程序复制到 `src/main.c`

### 4. 编译和烧录

在EIDE中：
1. 按 `F7` 编译项目
2. 点击 "下载" 或按快捷键烧录到板子
3. 复位MCU或重新上电

### 5. 查看输出

串口终端应该显示：
```
=================================
UART0 Test Starting...
=================================

Device: MSPM0G3507
UART0: 115200 baud, 8N1
TX Pin: PA10
RX Pin: PA11
=================================

Counter: 0
Counter: 1
Counter: 2
Counter: 3
...
```

计数器每秒递增一次。

### 6. 恢复原始代码

测试完成后，恢复原始main.c：

**Windows PowerShell:**
```powershell
Copy-Item src\main.c.backup src\main.c -Force
```

**Linux/Git Bash:**
```bash
cp src/main.c.backup src/main.c
```

## 文件说明

| 文件 | 用途 |
|------|------|
| `inc/uart_debug.h` | UART调试接口头文件 |
| `src/uart_debug.c` | UART调试功能实现 |
| `tests/test_uart0_simple.c` | 简单的UART0测试程序（无FreeRTOS） |
| `tests/test_uart0_debug.c` | 完整的UART0测试程序（较早版本） |
| `test_uart0.ps1` | Windows测试脚本 |
| `test_uart0.sh` | Linux测试脚本 |

## UART调试API使用

测试通过后，你可以在其他模块中使用这些函数输出调试信息：

```c
#include "uart_debug.h"

// 初始化（可选，UART0已由SYSCFG_DL_init初始化）
UartDebug_Init();

// 输出字符串
UartDebug_Print("Hello, ");
UartDebug_Println("World!");  // 带换行

// 输出数字
uint32_t value = 12345;
UartDebug_Print("Value: ");
UartDebug_PrintU32(value);
UartDebug_Println("");

// 输出有符号整数
int32_t temp = -273;
UartDebug_Print("Temperature: ");
UartDebug_PrintI32(temp);
UartDebug_Println(" C");

// 输出十六进制
uint32_t addr = 0x20000000;
UartDebug_Print("Address: ");
UartDebug_PrintHex(addr, true);  // true = 显示"0x"前缀
UartDebug_Println("");

// 输出浮点数（3位小数）
float voltage = 3.14159f;
UartDebug_Print("Voltage: ");
UartDebug_PrintFloat(voltage);
UartDebug_Println(" V");
```

## 在FreeRTOS中使用

在控制任务中使用UART调试时要注意：

1. **避免在快速循环中频繁打印** - 串口输出会阻塞，影响实时性
2. **考虑使用日志缓冲区** - 在低优先级任务中统一输出
3. **注意栈空间** - 字符串格式化会消耗栈

示例：
```c
// 在ControlApp初始化时输出一次
void ControlApp_Init(uint32_t targetLaps) {
    UartDebug_Println("=== Control App Init ===");
    UartDebug_Print("Target laps: ");
    UartDebug_PrintU32(targetLaps);
    UartDebug_Println("");
    // ... 初始化代码 ...
}

// 在关键事件时输出
void ControlApp_OnLapComplete(uint32_t lap) {
    UartDebug_Print("Lap ");
    UartDebug_PrintU32(lap);
    UartDebug_Println(" completed");
}
```

## 故障排查

### 看不到任何输出
1. 检查硬件连接，特别是GND
2. 确认串口终端配置正确（115200, 8N1）
3. 确认USB转串口驱动已安装
4. 检查TX/RX是否接反（TX接RX，RX接TX）
5. 用万用表测量PA10是否有电平变化

### 输出乱码
1. 检查波特率是否为115200
2. 确认MCU时钟配置正确（32 MHz）
3. 检查接线是否松动

### 输出不连续
1. 检查供电是否稳定
2. 确认没有其他任务占用UART0
3. 检查FreeRTOS任务优先级配置

## 后续集成

测试成功后，你可以：

1. **在各个模块中添加日志输出**
   - Motion Control: 输出速度、位置等
   - Sens-Decision: 输出传感器数据、决策状态
   - 电机控制: 输出PWM值、编码器计数

2. **创建统一的日志系统**
   - 添加日志级别（DEBUG, INFO, WARN, ERROR）
   - 添加时间戳
   - 可选择性开关日志

3. **性能分析**
   - 输出循环时间
   - 输出任务栈使用情况
   - 输出CPU负载

## 参考资料

- UART0配置: `Debug/ti_msp_dl_config.c` 第515-535行
- 引脚定义: `Debug/ti_msp_dl_config.h` 第148-163行
- SysConfig配置: `NewProject1.syscfg`

---

**注意**: UART0当前未被占用，可以安全使用作为调试输出。
