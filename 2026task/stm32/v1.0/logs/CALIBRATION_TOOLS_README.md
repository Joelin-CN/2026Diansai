# 校准工具集 - 使用说明

**创建日期**: 2026-07-30  
**状态**: ✅ 已完成，待集成到主程序

---

## 📦 已创建的文件

### 1. 校准工具源码

| 文件 | 位置 | 功能 |
|------|------|------|
| `calibration_tool.c/h` | `Core/Src/app/`, `Core/Inc/app/` | 统一校准工具主程序 |
| `encoder_resolution_calibration.c/h` | `Core/Src/app/`, `Core/Inc/app/` | 编码器分辨率校准 |
| `ir_sensor_calibration.c/h` | `Core/Src/app/`, `Core/Inc/app/` | IR传感器符号验证 |
| `motor_direction_calibration.c/h` | `Core/Src/app/`, `Core/Inc/app/` | 电机方向校准（已存在） |

### 2. 文档

| 文件 | 位置 | 内容 |
|------|------|------|
| `CALIBRATION_QUICK_GUIDE.md` | `docs/` | 快速参考指南 |
| `CALIBRATION_RECORD_TEMPLATE.md` | `logs/` | 校准记录表模板 |

---

## 🚀 如何使用

### 方式1：运行完整校准工具（推荐）

在你的主程序或调试任务中调用：

```c
#include "app/calibration_tool.h"

void run_calibration_task(void) {
    CalibrationTool_Run();
}
```

这会提供一个交互式菜单，让你选择：
- [1] 编码器分辨率校准
- [2] IR传感器符号验证
- [3] 电机方向校准
- [4] 完整校准流程（推荐）

### 方式2：单独运行某个校准

```c
#include "app/encoder_resolution_calibration.h"

void run_encoder_calibration(void) {
    EncoderResolutionCalibration_Run();
}
```

```c
#include "app/ir_sensor_calibration.h"

void run_ir_calibration(void) {
    IrSensorCalibration_Run();
}
```

```c
#include "app/motor_direction_calibration.h"

void run_motor_calibration(void) {
    MotorDirectionCalibration_Run();
}
```

---

## 📋 集成步骤

### 第1步：确认依赖

校准工具需要以下模块：
- ✅ `encoder.c/h` - 编码器接口
- ✅ `motor.c/h` - 电机控制接口
- ✅ `platform_time.c/h` - 时间接口
- ⚠️ `IrUartSensor_GetAnalog()` - IR传感器接口（需要确认）

### 第2步：添加到构建系统

如果使用 Makefile：
```makefile
# 在 C_SOURCES 中添加
C_SOURCES += \
Core/Src/app/calibration_tool.c \
Core/Src/app/encoder_resolution_calibration.c \
Core/Src/app/ir_sensor_calibration.c
```

如果使用 CubeMX/IDE：
- 将新文件添加到项目中
- 确保包含路径正确

### 第3步：在主程序中调用

**选项A: 在启动时运行（推荐用于初次校准）**

```c
int main(void) {
    // ... 硬件初始化 ...
    
    // 运行校准工具
    CalibrationTool_Run();
    
    // 继续正常程序
    // ...
}
```

**选项B: 通过按钮触发**

```c
// 在主循环或任务中
if (user_button_pressed()) {
    CalibrationTool_Run();
}
```

**选项C: 通过串口命令触发**

```c
// 在串口命令处理中
if (strcmp(cmd, "calibrate") == 0) {
    CalibrationTool_Run();
} else if (strcmp(cmd, "cal_encoder") == 0) {
    EncoderResolutionCalibration_Run();
} else if (strcmp(cmd, "cal_ir") == 0) {
    IrSensorCalibration_Run();
} else if (strcmp(cmd, "cal_motor") == 0) {
    MotorDirectionCalibration_Run();
}
```

---

## 🔧 注意事项

### IR传感器接口

当前代码中使用了 `IrUartSensor_GetAnalog(uint16_t raw[8])`，需要确认：

1. **检查这个函数是否存在**
```bash
# 在项目中搜索
grep -r "IrUartSensor_GetAnalog" .
```

2. **如果不存在或接口不同**，需要修改 `ir_sensor_calibration.c` 中的调用：

```c
// 示例：如果你的接口是这样的
extern bool IR_GetRawData(uint8_t index, uint16_t* value);

// 修改为：
uint16_t ir_raw[8];
for (int i = 0; i < 8; i++) {
    IR_GetRawData(i, &ir_raw[i]);
}
```

### 串口输出

校准工具使用 `printf()` 输出，确保：
- `printf` 已重定向到UART
- UART波特率足够高（建议 115200）
- 终端支持ANSI转义码（用于特殊字符显示）

### 用户交互

当前简化版本使用延时代替用户输入。实际使用时可以改进：

```c
// 在 calibration_tool.c 的 wait_for_input() 中
static int wait_for_input(void) {
    printf("请输入选项 (0-4): ");
    fflush(stdout);
    
    // 等待串口输入
    char input = 0;
    while (!uart_has_data()) {
        HAL_Delay(10);
    }
    input = uart_read_char();
    
    return input - '0';  // 转换 '0'-'4' 为 0-4
}
```

---

## 📊 校准流程

### 推荐顺序

```
1. 编码器分辨率校准 (5分钟)  🔴 最重要
   ↓
2. IR传感器符号验证 (3分钟)   🟡 很重要
   ↓
3. 电机方向校准 (2分钟)       🟢 推荐
   ↓
4. 记录结果到模板
   ↓
5. 根据结果更新配置
   ↓
6. 重新编译和下载
   ↓
7. 重新验证
   ↓
8. 开始循迹测试
```

### 时间规划

- **首次校准**: 15-20分钟（包括记录和修改）
- **验证校准**: 5-10分钟（修改后重新验证）
- **例行检查**: 5分钟（赛前快速检查）

---

## ✅ 验证清单

完成校准后，检查：

```
□ 编码器分辨率
  □ 左轮: 1560 ± 5% (1482-1638 counts)
  □ 右轮: 1560 ± 5% (1482-1638 counts)
  
□ IR传感器符号
  □ 向右偏 → lateral_error < 0
  □ 向左偏 → lateral_error > 0
  
□ 电机方向
  □ 左电机正转 → 编码器增加
  □ 右电机正转 → 编码器增加
  
□ 配置文件更新
  □ motion_config.h (如需要)
  □ config.c (如需要)
  
□ 重新验证
  □ 重新编译成功
  □ 下载到STM32成功
  □ 所有测试通过
```

---

## 🎯 预期成果

校准完成后，你应该能够：

1. ✅ **准确的速度估计**
   - 速度反馈误差 < 5%
   - 编码器计数准确

2. ✅ **正确的循迹方向**
   - 小车偏右会向左纠正
   - 小车偏左会向右纠正

3. ✅ **一致的电机行为**
   - 正向指令 → 正向运动
   - 反向指令 → 反向运动

4. ✅ **可靠的控制性能**
   - PID控制稳定
   - 轨迹跟踪准确

---

## 📝 记录和文档

### 使用记录模板

1. 复制 `logs/CALIBRATION_RECORD_TEMPLATE.md`
2. 重命名为 `logs/CALIBRATION_RECORD_<日期>.md`
3. 填写所有测量值和结果
4. 保存以备查

### 示例文件名
```
logs/CALIBRATION_RECORD_2026-07-30_初次校准.md
logs/CALIBRATION_RECORD_2026-07-30_修正后验证.md
logs/CALIBRATION_RECORD_2026-08-01_赛前检查.md
```

---

## 🐛 故障排查

### 编译错误

**错误**: `undefined reference to IrUartSensor_GetAnalog`

**解决**: 
1. 查找实际的IR传感器接口函数名
2. 修改 `ir_sensor_calibration.c` 中的调用
3. 或者添加一个适配函数

**错误**: `undefined reference to platform_time.h`

**解决**:
1. 确认 `platform_time.c/h` 存在
2. 或者用 `HAL_GetTick()` 替代

### 运行时问题

**问题**: 串口无输出

**检查**:
- `printf` 是否重定向
- UART是否初始化
- 波特率是否匹配

**问题**: 编码器计数始终为0

**检查**:
- TIM是否启动
- 编码器是否连接
- `Encoder_Poll()` 是否被调用

**问题**: IR传感器全是0

**检查**:
- UART是否工作
- 传感器是否供电
- 通信协议是否正确

---

## 📞 下一步

完成校准后：

1. **低速循迹测试**
   ```c
   // 设置低速参数
   line_speed_mps = 0.3f;      // 0.3 m/s
   lateral_gain = 0.5f;
   heading_gain = 0.0f;
   ```

2. **参数调优**
   - 调整PID增益
   - 调整前馈控制
   - 优化轨迹跟踪

3. **性能提升**
   - 逐步提高速度
   - 测试极限性能
   - 准备比赛

---

## 📚 相关文档

- 📖 [快速指南](../docs/CALIBRATION_QUICK_GUIDE.md)
- 📋 [记录模板](CALIBRATION_RECORD_TEMPLATE.md)
- 📊 [参数更新总结](PARAMETER_UPDATE_SUMMARY_2026-07-30.md)
- 🔧 [迁移指南](../docs/MIGRATION_GUIDE.md)

---

**创建者**: Kiro  
**版本**: 1.0  
**最后更新**: 2026-07-30  
**状态**: ✅ 待集成测试
