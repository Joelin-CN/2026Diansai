# IR传感器UART端口更正

**日期**: 2026-07-30  
**问题**: IR传感器使用了错误的UART端口  
**状态**: ✅ 已更正  

---

## 🔴 关键错误

之前的所有代码和测试都使用了 **UART4 (PC10/PC11)**，但根据项目文档 `docs/STM32F407VGT6_SKYSTAR_PIN_ASSIGNMENT.md`，IR循迹模块应该使用：

### ✅ 正确配置

| 模块 | 外设 | MCU TX | MCU RX | 波特率 |
|------|------|--------|--------|--------|
| 8路循迹 | **USART2** | **PA2** | **PA3** | 115200 |

### ❌ 错误配置（已修正）

| 模块 | 外设 | MCU TX | MCU RX | 用途 |
|------|------|--------|--------|------|
| 蓝牙 | UART4 | PC10 | PC11 | ATK-BLE02 |

---

## 🔧 已修改的文件

### 1. `modules/IR-tracker/src/ir_uart_sensor.c`

**修改内容**：更改UART宏定义

```c
// 修改前（错误）
#define IR_UART_HANDLE  (&huart4)
#define IR_UART_IRQn    UART4_IRQn

// 修改后（正确）
#define IR_UART_HANDLE  (&huart2)
#define IR_UART_IRQn    USART2_IRQn
```

**影响**：所有IR传感器通信现在使用USART2

---

### 2. `Core/Src/stm32f4xx_it.c`

**修改内容A**：将IR接收处理从UART4移到USART2

```c
// USART2中断处理（新增IR处理）
void USART2_IRQHandler(void)
{
  /* USER CODE BEGIN USART2_IRQn 0 */
  // Handle IR sensor UART reception
  if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_RXNE) &&
      __HAL_UART_GET_IT_SOURCE(&huart2, UART_IT_RXNE)) {
    extern void IrUartSensor_RxByte(uint8_t byte);
    extern void IrUartDiag_CountRxByte(uint8_t byte);
    uint8_t received_byte = (uint8_t)(huart2.Instance->DR & 0xFF);
    IrUartSensor_RxByte(received_byte);
    IrUartDiag_CountRxByte(received_byte);
  }
  /* USER CODE END USART2_IRQn 0 */
  HAL_UART_IRQHandler(&huart2);
  /* USER CODE BEGIN USART2_IRQn 1 */

  /* USER CODE END USART2_IRQn 1 */
}
```

**修改内容B**：恢复UART4原始状态（用于蓝牙）

```c
// UART4中断处理（恢复为空，留给蓝牙模块）
void UART4_IRQHandler(void)
{
  /* USER CODE BEGIN UART4_IRQn 0 */

  /* USER CODE END UART4_IRQn 0 */
  HAL_UART_IRQHandler(&huart4);
  /* USER CODE BEGIN UART4_IRQn 1 */

  /* USER CODE END UART4_IRQn 1 */
}
```

---

### 3. `Core/Src/app/ir_uart_diagnostic.c`

**修改内容**：更新诊断工具使用USART2

**主要变更**：
- 所有 `huart4` → `huart2`
- GPIO检查：PC10/PC11 → PA2/PA3
- 中断号：UART4_IRQn → USART2_IRQn
- 标题：UART4 → USART2

---

### 4. `Core/Src/app/ir_sensor_test.c`

**修改内容**：更新测试工具说明

```c
// 修改前
printf("   IR Sensor Test (UART4 @ 115200)    \r\n");
printf("Hardware: PC10(TX), PC11(RX)\r\n");

// 修改后
printf("   IR Sensor Test (USART2 @ 115200)   \r\n");
printf("Hardware: PA2(TX), PA3(RX)\r\n");
```

---

## 📊 正确的硬件连接

### IR传感器接线（正确）

```
┌─────────────────────┐           ┌─────────────────────┐
│      STM32F407      │           │    IR Sensor        │
│                     │           │    (8路循迹)        │
│   PA2 (USART2 TX)  ├──────────►│ RX (接收)           │
│                     │           │                     │
│   PA3 (USART2 RX)  │◄──────────┤ TX (发送)           │
│                     │           │                     │
│   GND              ├───────────┤ GND                 │
│                     │           │                     │
│                     │           │ VCC ← 5V电源        │
└─────────────────────┘           └─────────────────────┘
```

### 其他UART分配（参考）

| 模块 | 外设 | TX | RX | 用途 |
|------|------|----|----|------|
| 调试串口 | UART5 | PC12 | PD2 | printf输出 |
| 蓝牙 | UART4 | PC10 | PC11 | ATK-BLE02 |
| **循迹** | **USART2** | **PA2** | **PA3** | **IR传感器** ✅ |
| K230 | USART3 | PD8 | PD9 | 视觉处理 |
| 舵机 | USART6 | PC6 | PC7 | 平衡舵机 |

---

## 🔍 为什么之前测试失败

### 原因分析

1. **使用了错误的UART端口**
   - 代码配置：UART4 (PC10/PC11)
   - 实际接线：USART2 (PA2/PA3)
   - 结果：发送和接收都在错误的引脚上

2. **UART4可能未正确初始化**
   - UART4主要用于蓝牙模块
   - 可能使用了DMA循环接收模式
   - 与IR传感器的中断接收模式冲突

3. **诊断输出误导**
   - 诊断显示"UART4配置正常"
   - 但IR传感器根本没连接到UART4
   - 所以当然收不到数据

---

## ✅ 现在的状态

### 已修复的问题

1. ✅ IR传感器驱动使用USART2
2. ✅ USART2中断处理已配置
3. ✅ 诊断工具检查USART2配置
4. ✅ RXNEIE强制使能（解决之前的Bug）
5. ✅ 测试工具显示正确的引脚说明

### 需要验证的项目

**请确认你的硬件连接：**

- [ ] IR传感器TX → STM32 PA3 (USART2_RX)
- [ ] IR传感器RX ← STM32 PA2 (USART2_TX)
- [ ] IR传感器GND ↔ STM32 GND
- [ ] IR传感器VCC ← 5V电源

---

## 🚀 下一步操作

### 1. 确认硬件连接

**检查当前接线：**
```
你的IR传感器目前连接到哪些引脚？
- 如果是 PA2/PA3 → ✅ 正确，直接测试
- 如果是 PC10/PC11 → ❌ 错误，需要改线到PA2/PA3
```

### 2. 重新编译和烧录

所有文件已更新为使用USART2，需要重新编译：

```
1. 清理构建
2. 重新编译（确保无错误）
3. 烧录到目标板
```

### 3. 运行新的诊断

烧录后查看输出，应该显示：

```
========================================
   USART2 Configuration Diagnostic     
========================================
Instance:     USART2 (0x40004400)
Baud Rate:    115200
...
PA2 (TX): Mode=2, AF=7
PA3 (RX): Mode=2, AF=7

USART2 Register Status:
CR1:  0x0000202C [UE=ON] [TE=ON] [RE=ON] [RXNEIE=ON]  ✅
```

### 4. 测试传感器响应

如果硬件连接正确，应该能看到：

```
IR Sensor Communication Test
Sending analog mode request: $0,1,0#
Waiting 2 seconds for sensor response...
  [100ms] RX: 15 bytes
  [200ms] RX: 45 bytes

Result: TX=7 bytes, RX=45 bytes
✅ Received 45 bytes
Raw data: $A0:1234,1:1456...
```

---

## 📝 经验教训

### 1. 始终参考项目文档

在开始实现之前，应该先查看：
- 引脚分配文档
- 硬件设计文档  
- 原理图

避免假设或记忆错误。

### 2. 早期硬件验证

在编写复杂的驱动代码之前：
1. 确认物理连接
2. 用示波器验证信号
3. 做简单的回环测试

### 3. 文档与代码一致性

发现引脚分配错误后：
- ✅ 立即更新所有相关代码
- ✅ 更新注释和说明
- ✅ 创建更正文档
- ✅ 通知团队成员

---

## 🔗 相关文档

| 文档 | 内容 |
|------|------|
| `docs/STM32F407VGT6_SKYSTAR_PIN_ASSIGNMENT.md` | **官方引脚分配文档** ⭐ |
| `logs/2026-07-30_ir_sensor_debug_summary.md` | IR传感器调试总结（UART4，已过时） |
| `logs/2026-07-30_ir_sensor_wiring_checklist.md` | 接线检查清单（需更新为USART2） |
| `logs/2026-07-29_ir_sensor_uart_port_fix.md` | 原始移植文档（错误使用了UART4） |

---

## ⚠️ 重要提醒

### UART端口分配（最终确认）

```
USART2 (PA2/PA3)  → IR循迹传感器  ✅ 正确
UART4 (PC10/PC11) → 蓝牙模块      ✅ 保留
UART5 (PC12/PD2)  → 调试串口      ✅ 正确
```

**现在所有代码都已更正为使用USART2。**

**请确认你的硬件接线也是USART2 (PA2/PA3)，然后重新编译测试！**

---

**文档创建时间**: 2026-07-30  
**执行者**: Claude (Opus 4.8) + 用户 Joelin  
**问题类型**: 引脚分配错误  
**修复状态**: ✅ 代码已全部更正为USART2  
**待确认**: 硬件接线是否为PA2/PA3

