# IR传感器故障排查日志

**日期**: 2026-07-30  
**问题**: IR传感器无数据接收  
**状态**: 🔍 诊断中  

---

## 📋 问题描述

根据测试记录文件 `records-2026-07-30-02-20-40.json`，IR传感器测试运行了完整的10秒，但是：

❌ **没有收到任何有效数据帧**
- 所有输出都是 "Waiting for first valid frame..."
- 10秒后进入监控模式，持续显示 "IR: (no valid data)"
- 帧计数始终为0

---

## 🔍 可能的原因分析

### 1. UART4中断未触发（最可能）

**症状**：
- `IrUartSensor_RxByte()` 从未被调用
- `g_rx_receiving` 始终为 false
- 没有任何字节计数

**可能原因**：
- [ ] UART4 RX中断未使能
- [ ] NVIC中UART4中断未使能
- [ ] 中断优先级配置错误

### 2. 硬件连接问题

**症状**：
- 传感器没有响应请求命令
- TX/RX线路可能有问题

**可能原因**：
- [ ] IR传感器未上电
- [ ] TX/RX接线错误（接反或未连接）
- [ ] 公共地未连接
- [ ] 线路接触不良

### 3. 传感器协议不匹配

**症状**：
- 传感器收到命令但不响应

**可能原因**：
- [ ] 传感器不支持 `$0,1,0#` 协议
- [ ] 波特率不匹配（不是115200）
- [ ] 传感器需要特殊初始化序列
- [ ] 传感器固件版本不兼容

### 4. 帧格式解析问题

**症状**：
- 收到数据但无法解析

**可能原因**：
- [ ] 传感器返回的帧格式与预期不符
- [ ] 帧头不是 `$A`
- [ ] 分隔符不是 `:`（代码期望 `N:VALUE` 格式）

---

## 🛠️ 已创建的诊断工具

### 1. IR UART诊断工具

**文件**：
- `Core/Src/app/ir_uart_diagnostic.c`
- `Core/Inc/app/ir_uart_diagnostic.h`

**功能**：
- ✅ 打印UART4完整配置（波特率、数据位、GPIO、寄存器）
- ✅ 检查NVIC中断状态
- ✅ 发送测试命令并监控响应
- ✅ 统计接收字节数
- ✅ 显示接收到的原始数据（hex/ascii）

**使用方法**：
```c
IrUartDiag_RunAll();  // 在测试前调用
```

### 2. 增强的中断处理

**修改**：`Core/Src/stm32f4xx_it.c:406-414`

```c
void UART4_IRQHandler(void)
{
  if (__HAL_UART_GET_FLAG(&huart4, UART_FLAG_RXNE) &&
      __HAL_UART_GET_IT_SOURCE(&huart4, UART_IT_RXNE)) {
    extern void IrUartSensor_RxByte(uint8_t byte);
    extern void IrUartDiag_CountRxByte(uint8_t byte);  // ← 新增
    uint8_t received_byte = (uint8_t)(huart4.Instance->DR & 0xFF);
    IrUartSensor_RxByte(received_byte);
    IrUartDiag_CountRxByte(received_byte);  // ← 新增：统计接收字节
  }
  HAL_UART_IRQHandler(&huart4);
}
```

### 3. 诊断测试流程

**修改**：`Core/Src/freertos.c`

新的测试流程：
1. **Step 1**: 运行底层UART4诊断 (`IrUartDiag_RunAll()`)
   - 打印配置信息
   - 测试传感器通信
   - 显示接收字节数和原始数据
2. **Step 2**: 运行IR传感器测试 (`IrSensorTest_Run()`)
   - 10秒数据采集
   - 统计帧率和错误率
3. **Step 3**: 进入持续监控模式 (`IrSensorTest_Monitor()`)

---

## 📊 预期诊断输出

### 正常情况（传感器工作）

```
************************************************
*   UART4 / IR SENSOR DIAGNOSTIC SUITE        *
************************************************

========================================
   UART4 Configuration Diagnostic      
========================================
Instance:     UART4 (0x40004C00)
Baud Rate:    115200
Word Length:  8 bits
Stop Bits:    1
Parity:       None
Mode:         TX + RX
Flow Control: None

GPIO Configuration:
PC10 (TX): Mode=2, AF=8
PC11 (RX): Mode=2, AF=8

UART4 Register Status:
CR1:  0x0000200C [UE=ON] [TE=ON] [RE=ON] [RXNEIE=ON] 
SR:   0x000000C0 [TC] [TXE] 

NVIC Configuration:
UART4_IRQn (52): ENABLED, NOT PENDING
========================================

IR Sensor Communication Test
Sending analog mode request: $0,1,0#
Waiting 2 seconds for sensor response...
  [100ms] RX: 3 bytes
  [200ms] RX: 15 bytes
  [300ms] RX: 27 bytes

Result: TX=7 bytes, RX=45 bytes
✅ Received 45 bytes
Raw data: $A0:1234,1:1456,2:1678...
```

### 异常情况：无数据接收

```
Result: TX=7 bytes, RX=0 bytes
❌ No response from sensor
Check:
  1. Sensor power (usually 5V)
  2. TX/RX not swapped (STM32 TX->Sensor RX)
  3. Common ground connected
  4. Sensor firmware supports this protocol
```

### 异常情况：中断未使能

```
UART4 Register Status:
CR1:  0x0000000C [UE=ON] [TE=ON] [RE=ON]   ← 缺少 [RXNEIE=ON]
SR:   0x000000C0 [TC] [TXE] 

NVIC Configuration:
UART4_IRQn (52): DISABLED, NOT PENDING      ← 问题所在
```

---

## 🔧 排查步骤

### Step 1: 编译并烧录新版本

```bash
# 使用VS Code或你习惯的IDE
# 1. 清理构建
# 2. 重新编译（包含诊断工具）
# 3. 烧录到目标板
```

### Step 2: 查看诊断输出

打开串口监视器（UART5, 115200），重启MCU，查看诊断信息：

**关键检查点**：

1. **UART4 CR1寄存器**
   - 必须包含 `[RXNEIE=ON]` 标志
   - 如果没有，说明RX中断未使能

2. **NVIC状态**
   - UART4_IRQn 必须是 `ENABLED`
   - 如果是 `DISABLED`，检查 `IrUartSensor_Init()`

3. **接收字节数**
   - 如果 RX=0，检查硬件连接
   - 如果 RX>0，说明中断正常，问题在协议解析

### Step 3: 硬件检查（如果RX=0）

#### 检查传感器供电
```
万用表测量传感器VCC引脚：
- 应该有5V（或3.3V，取决于传感器型号）
- 如果没有电压，检查电源连接
```

#### 检查UART连接
```
┌─────────────┐           ┌─────────────┐
│   STM32     │           │ IR Sensor   │
│             │           │             │
│  PC10 (TX) ├──────────►│ RX          │  ← STM32发送
│  PC11 (RX) │◄──────────┤ TX          │  ← 传感器发送
│        GND ├───────────┤ GND         │  ← 必须共地
└─────────────┘           └─────────────┘

使用万用表连续性测试：
- STM32 PC10 ←→ 传感器 RX
- STM32 PC11 ←→ 传感器 TX
- STM32 GND ←→ 传感器 GND
```

#### 使用示波器/逻辑分析仪
```
1. 探头接在STM32 PC10（TX）
   - 应该看到发送 "$0,1,0#" 的波形
   - 波特率115200，每字节约87μs

2. 探头接在STM32 PC11（RX）
   - 如果传感器正常，应该看到返回数据
   - 如果没有波形，传感器未响应
```

### Step 4: 协议调试（如果RX>0但无有效帧）

如果诊断显示接收到字节，但解析失败：

1. **查看原始数据**
   - 诊断工具会显示前16字节的hex/ascii
   - 检查帧格式是否为 `$A...#`

2. **可能的帧格式问题**
   ```
   预期格式：$A0:1234,1:1456,2:1678,3:1890,4:2012,5:2234,6:2456,7:2678#
   
   问题1：帧头不是$A
   实际：  $D0,1234,1456,1678,1890,2012,2234,2456,2678#
   
   问题2：分隔符不是冒号
   实际：  $A0=1234,1=1456,2=1678...#
   
   问题3：没有通道号
   实际：  $A1234,1456,1678,1890,2012,2234,2456,2678#
   ```

3. **修改解析代码**
   - 根据实际帧格式修改 `ir_uart_sensor.c:IrUartSensor_ParseAnalogFrame()`

---

## 🎯 当前代码的帧格式假设

查看 `modules/IR-tracker/src/ir_uart_sensor.c:140-199`：

```c
// 帧格式：$A<channel>:<value>,<channel>:<value>,...#
// 例如：  $A0:1234,1:1456,2:1678,3:1890,4:2012,5:2234,6:2456,7:2678#

if ((frame[0] != '$') || (frame[1] != 'A') || (frame[length - 1U] != '#')) {
    return false;  // 帧头/帧尾检查
}

// 解析每个字段，寻找 ':' 分隔符
while (field_end < frame_end) {
    if (*field_end == ':') {
        field_mid = field_end;  // 找到通道号和数值的分隔符
    } else if (*field_end == ',') {
        break;
    }
    field_end++;
}
```

**如果传感器使用不同格式，需要修改解析逻辑！**

---

## 🚀 下一步操作

### 立即执行：

1. **编译新版本**（包含诊断工具）
2. **烧录到目标板**
3. **运行并查看诊断输出**
4. **根据诊断结果决定下一步**

### 根据诊断结果：

| 诊断结果 | 问题定位 | 下一步 |
|---------|---------|-------|
| RXNEIE未使能 | 软件配置 | 检查 `IrUartSensor_Init()` |
| NVIC未使能 | 软件配置 | 检查 `HAL_NVIC_EnableIRQ()` |
| RX=0字节 | 硬件连接 | 检查供电、接线、示波器 |
| RX>0但无有效帧 | 协议不匹配 | 查看原始数据，修改解析 |
| 收到有效帧 | 正常 | 继续功能测试 |

---

## 📝 已修改文件清单

| 文件 | 修改内容 |
|------|---------|
| `Core/Src/app/ir_uart_diagnostic.c` | 新增：UART4诊断工具实现 |
| `Core/Inc/app/ir_uart_diagnostic.h` | 新增：UART4诊断工具接口 |
| `Core/Src/stm32f4xx_it.c` | 修改：UART4中断添加字节计数 |
| `Core/Src/freertos.c` | 修改：添加诊断步骤到测试流程 |
| `CMakeLists.txt` | 修改：添加诊断工具编译 |

---

## 💡 关键调试技巧

### 1. 使用诊断工具确认中断触发

在 `IrUartDiag_CountRxByte()` 中添加LED闪烁：
```c
void IrUartDiag_CountRxByte(uint8_t byte)
{
    rx_byte_count++;
    HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);  // 每收到一个字节闪烁一次
    // ...
}
```

### 2. 回环测试

物理短接 PC10 和 PC11，运行回环测试：
- 如果能收到自己发送的数据 → UART4硬件正常
- 如果收不到 → UART4配置或硬件问题

### 3. 降低波特率

如果怀疑波特率问题，临时改为9600测试：
```c
// usart.c 中修改
huart4.Init.BaudRate = 9600;  // 原115200
```

---

## 🔗 相关文档

- `logs/2026-07-30_ir_sensor_test_preparation.md` - IR测试准备日志
- `logs/2026-07-29_ir_sensor_uart_port_fix.md` - IR传感器移植日志
- `modules/IR-tracker/src/ir_uart_sensor.c` - IR传感器驱动
- `Core/Src/stm32f4xx_it.c` - UART4中断处理

---

**日志撰写时间**: 2026-07-30  
**执行者**: Claude (Opus 4.8) + 用户 Joelin  
**当前状态**: 🔍 诊断工具已创建，等待编译测试  
**下一步**: 编译、烧录、查看诊断输出

