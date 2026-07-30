# lateral_error 符号验证准备 - 会话修复日志

**日期**: 2026-07-30
**状态**: ⏳ 等待编译烧录验证

---

## 本次会话完成的修复

### 修复1: `Core/Src/app/sensor_adapter.c`

**问题**: `read_ir()` 直接调 `IrUartSensor_GetAnalog()`，但从未调 `IrUartSensor_Process()`。
`g_analog_valid` 永远是 false，GetAnalog 永远返回 false。

数据流必须是：
```
中断 → RxByte() → g_frame_ready=true
主循环 → Process() → 解析帧 → g_analog_valid=true   ← 这步之前缺失
         GetAnalog() → 返回数据
```

**修复**: 在 `GetAnalog()` 前加 `IrUartSensor_Process()` 调用（当走完整HAL链路时仍需要此修复）。

---

### 修复2: `Core/Src/freertos.c` — 测试任务重写（第三版）

**问题**: 前两版使用 preprocess/sensor HAL 完整链路，该链路有多个串联失败点
（`sd_config_validate()`、`sensors_init_all()` 等），任何一层失败都淹没为
"IR not ready"，无法诊断。

**修复**: 完全绕过传感器HAL链路，直接调用IR底层API：

```
IrUartSensor_Init()
IrUartSensor_RequestAnalogMode()
SET_BIT(huart2.Instance->CR1, USART_CR1_RXNEIE)  ← 防御性重置

循环:
  IrUartSensor_Process()    → 解析中断缓冲区中的帧
  IrUartSensor_GetAnalog()  → 获取8通道ADC值
  手动计算 lateral_error    → 与 perception.c 算法相同
  printf 输出
```

---

## lateral_error 计算公式（与 perception.c 一致）

```c
const float w[8] = {
    -0.5694f, -1.7083f, -2.8472f, -3.9861f,  // ch0-3: 右侧，负值
     0.5694f,  1.7083f,  2.8472f,  3.9861f   // ch4-7: 左侧，正值
};
float wsum = 0;
for (i=0; i<8; i++) wsum += w[i] * (float)ir_raw[i];
float lateral_error = wsum / 3.9861f;  // 除以 max_abs_weight
```

IR传感器值：高值=白色（无线），低值=黑色（检测到线）。
ch0 (index 0) 对应物理最右侧通道 x1。

---

## 串口输出格式

正常数据行：
```
[ms]  lat_err  x1   x2   x3   x4   x5   x6   x7   x8  dir
[  3450]   +0.0   261  263  265  260  258  264  262  259  CTR
[  3650]  -142.3  261  263  265  260   98  264  262  259  >>RIGHT
[  3850]  +138.7  261  263   91  260  258  264  262  259  << LEFT
```

无数据诊断行：
```
[  3450] nodata: proc=1(0=OK 1=NOFR 2=BAD 3=OVF) rdy=0
```

- `proc=1 rdy=0` → 中断未触发，RXNEIE问题或接线
- `proc=2` → 收到字节但帧解析失败，协议不匹配
- `proc=3` → 溢出

---

## 符号验证规则

| 操作 | 期望 | 若符号反了 |
|------|------|-----------|
| 向右推小车 | `lat < 0`，`>>RIGHT` | 将 `config.c` ir_weights 整体取反 |
| 向左推小车 | `lat > 0`，`<< LEFT` | 同上 |
| 中心 | `lat ≈ 0`，`CTR` | — |

若符号反了，修改 `modules/Sens-Decision/src/config.c` 第22-24行：
```c
// 当前（右负左正）
{ -0.5694f, -1.7083f, -2.8472f, -3.9861f, +0.5694f, +1.7083f, +2.8472f, +3.9861f }
// 取反后（右正左负）
{ +0.5694f, +1.7083f, +2.8472f, +3.9861f, -0.5694f, -1.7083f, -2.8472f, -3.9861f }
```

---

## 已修改文件

| 文件 | 说明 |
|------|------|
| `Core/Src/freertos.c` | 测试任务：直接IR路径，200ms循环输出 lateral_error + 原始8通道值 |
| `Core/Src/app/sensor_adapter.c` | `read_ir()` 加 `IrUartSensor_Process()` |

## 待完成（下一会话）

1. 编译烧录，观察串口
2. 验证 lateral_error 符号
3. 若符号正确 → 将 `freertos.c` 恢复为完整控制流程（`ControlApp_Init` + `ControlApp_RunFastCycle`）
4. 调试完整闭环运行

---

**执行者**: Claude (Opus 4.8) + Joelin
