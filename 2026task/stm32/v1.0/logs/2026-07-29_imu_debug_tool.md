# IMU调试工具开发日志

**日期**: 2026-07-29  
**模块**: ICM42688 IMU调试  
**状态**: ✅ 已完成  

---

## 📋 任务概述

开发IMU调试工具，用于验证ICM42688陀螺仪和加速度计的数据读取功能以及AHRS姿态解算算法是否正常工作。

---

## ✅ 已完成工作

### 1. 创建IMU调试模块（2个新文件）

#### `Core/Inc/app/imu_debug.h`
- 调试接口头文件
- 导出 `ImuDebug_Init()` 和 `ImuDebug_Run()` 函数

#### `Core/Src/app/imu_debug.c`
核心功能实现：

**初始化流程** (`ImuDebug_Init`):
1. 初始化平台时间（DWT_CYCCNT）
2. 绑定ICM42688 STM32 SPI适配器
3. 初始化ICM42688（包含WHO_AM_I=0x47验证）
4. 陀螺仪零偏自动标定（100样本×10ms）
5. 初始化AHRS Mahony算法

**运行循环** (`ImuDebug_Run`):
- 读取ICM42688 6轴数据（加速度+陀螺仪）
- 读取温度传感器
- 更新AHRS算法（输入：gyro rad/s, accel g）
- 计算姿态角（Roll/Pitch/Yaw）
- 格式化输出到debug串口

### 2. 修改FreeRTOS任务

#### `Core/Src/freertos.c`
- 在 `StartDefaultTask` 中启用IMU调试模式
- 跳过BLE初始化（避免干扰）
- 每500ms调用 `ImuDebug_Run()` 输出数据
- 添加10ms osDelay防止任务饿死

### 3. 更新构建配置

#### `CMakeLists.txt`
- 添加 `Core/Src/app/imu_debug.c` 到源文件列表
- AHRS模块（`ahrs_hal.c`）已在之前的迁移中添加

### 4. 修复编译错误

#### `Core/Inc/app/imu_debug.h`
- 添加 `#include <stdint.h>`（uint32_t类型定义）

---

## 📊 输出数据格式

通过UART5（debug串口）输出，格式如下：

```
========================================
Timestamp: xxxxx ms
----------------------------------------
ACC_RAW:  [  xxxx,   xxxx,   xxxx]       # 加速度原始ADC值（int16_t）
GYRO_RAW: [  xxxx,   xxxx,   xxxx]       # 陀螺仪原始ADC值（int16_t）
TEMP_RAW: xxxx                            # 温度原始ADC值
----------------------------------------
ACC_G:    [ x.xxx,  x.xxx,  x.xxx] g     # 加速度（重力加速度g）
GYRO_DPS: [ xx.xx,  xx.xx,  xx.xx] deg/s # 角速度（度/秒）
TEMP:     xx.xx °C                        # 温度（摄氏度）
----------------------------------------
EULER:    Roll=xx.xx  Pitch=xx.xx  Yaw=xx.xx deg  # 姿态角（度）
ACC_MAG:  x.xxx g (should be ~1.0 when still)     # 加速度模长
========================================
```

**数据更新率**: 每500ms一次（2Hz）

---

## 🔧 使用方法

### 1. 硬件连接
- **SPI2**: ICM42688通信接口（SCK/MISO/MOSI/CS）
- **UART5**: Debug串口输出
  - PC12 (TX) → USB转串口模块 RX
  - PD2 (RX) → USB转串口模块 TX（本工具未使用接收）
  - 波特率: 115200, 8N1

### 2. 编译和烧录
```bash
# 使用EIDE或命令行编译
cube-cmake --build build/Debug

# 通过ST-Link烧录
# （使用你的烧录工具）
```

### 3. 查看输出
打开串口助手：
- 端口：选择USB转串口设备
- 波特率：115200
- 数据位：8
- 停止位：1
- 校验：无

上电后会看到初始化信息，然后每500ms输出一次IMU数据。

---

## ✅ 验证要点

### 1. 初始化阶段
启动时应显示：
```
========================================
    ICM42688 IMU Debug Tool v1.0
========================================

[1/5] Initializing platform timer...
[2/5] Binding ICM42688 STM32 adapter...
[3/5] Initializing ICM42688 sensor...
  ICM42688 WHO_AM_I check: OK (0x47)
[4/5] Calibrating gyro bias (keep sensor still)...
  Gyro bias: X=x.xxx Y=x.xxx Z=x.xxx dps
[5/5] Initializing AHRS (Mahony 6-axis)...

========================================
  IMU Debug Ready!
========================================
```

**如果初始化失败**，会显示错误提示：
- `ICM42688 init failed`: 检查SPI连接、CS引脚、3.3V供电
- `Gyro calibration failed`: 可以继续，但零偏未校准

### 2. 静态测试（传感器静止）
- **ACC_G**: 三轴中有一轴接近±1.0g（取决于放置方向）
  - 例如：水平放置时 Z轴≈1.0g
- **ACC_MAG**: 应接近1.0g（地球重力）
- **GYRO_DPS**: 三轴接近0 deg/s（标定后零偏<1 deg/s）
- **EULER**: 
  - 水平放置：Roll≈0°, Pitch≈0°
  - Yaw会缓慢漂移（仅6轴无磁力计）

### 3. 动态测试（旋转传感器）
- **绕X轴旋转90°**: Roll应变化±90°
- **绕Y轴旋转90°**: Pitch应变化±90°
- **绕Z轴旋转**: Yaw应跟随旋转（会有累积漂移）
- **GYRO_DPS**: 旋转时对应轴应有明显非零读数

### 4. 数据合理性检查
- **ACC_RAW**: 典型范围 -4000 ~ +4000（±8g量程，16位ADC）
- **GYRO_RAW**: 典型范围 -2000 ~ +2000（±1000dps量程）
- **TEMP**: 应在室温附近（20-30°C）
- **无明显噪声尖峰**: 数据应平滑变化

---

## 🐛 故障排查

### 问题1: 初始化失败 - WHO_AM_I不匹配
**可能原因**:
- SPI接线错误（SCK/MISO/MOSI/CS）
- CS引脚配置错误（检查gpio.c中IMU_CS的配置）
- SPI时钟极性/相位不匹配（CubeMX配置）
- 供电不足或接地不良

**解决方法**:
1. 用万用表/逻辑分析仪检查SPI信号
2. 确认CS在读写时正确拉低/拉高
3. 检查SPI2配置：模式3（CPOL=1, CPHA=1）
4. 确认3.3V供电稳定

### 问题2: 陀螺仪零偏很大（>10 deg/s）
**可能原因**:
- 标定时传感器在移动
- 温度漂移严重（刚上电时）

**解决方法**:
1. 标定时保持传感器绝对静止
2. 上电后等待30秒再标定
3. 在代码中增加标定样本数（100→200）

### 问题3: ACC_MAG不是1.0g
**可能原因**:
- 量程配置错误
- 传感器损坏

**解决方法**:
1. 检查imu_config中acc_sample配置（当前±8g）
2. 计算误差：如果ACC_MAG=0.5或2.0，可能是量程配置翻倍/减半
3. 更换传感器

### 问题4: 姿态角不响应或乱跳
**可能原因**:
- AHRS时间戳异常
- 加速度/陀螺仪数据单位错误
- 坐标系不匹配

**解决方法**:
1. 检查 `PlatformTime_GetUs64()` 是否递增
2. 确认gyro单位转换：dps → rad/s（×0.017453）
3. 检查传感器安装方向与软件坐标系一致

---

## 📝 技术细节

### ICM42688配置
```c
.interface_type = ICM42688_INTERFACE_SPI,
.acc_sample     = ICM42688_ACC_SAMPLE_SGN_8G,      // ±8g
.gyro_sample    = ICM42688_GYRO_SAMPLE_SGN_1000DPS, // ±1000dps
.sample_rate    = ICM42688_SAMPLE_RATE_1000         // 1kHz
```

### AHRS算法
- **类型**: Mahony 6-axis（无磁力计）
- **输入**: 陀螺仪（rad/s）+ 加速度计（g）
- **输出**: 四元数 → 欧拉角（Roll/Pitch/Yaw）
- **局限性**: Yaw会漂移（需要磁力计校正）

### 温度转换公式
根据ICM42688数据手册：
```c
temp_c = (float)temperature_raw / 132.48 + 25.0
```

---

## 🔗 相关文件

| 文件 | 功能 |
|------|------|
| `Core/Inc/app/imu_debug.h` | 调试接口头文件 |
| `Core/Src/app/imu_debug.c` | 调试功能实现 |
| `modules/ICM42688/src/icm42688_hal.c` | ICM42688 HAL层（平台无关） |
| `modules/ICM42688/src/icm42688_stm32.c` | STM32 SPI适配器 |
| `modules/ICM42688/src/ahrs_hal.c` | Mahony AHRS算法 |
| `Core/Src/app/platform_time.c` | 微秒级时间戳（DWT） |
| `Core/Src/freertos.c` | FreeRTOS任务（调用调试循环） |

---

## 📌 后续工作

### 1. 集成到主控制循环
当IMU硬件验证通过后：
- 移除freertos.c中的调试代码
- 恢复ControlApp_Init和ControlApp_RunFastCycle
- IMU数据将通过sensor_adapter.c自动采集

### 2. 可选优化
- [ ] 添加磁力计（如果硬件支持）消除Yaw漂移
- [ ] 增加数据记录功能（SD卡/Flash）
- [ ] 添加FFT频谱分析（振动诊断）
- [ ] 实现陀螺仪温度补偿

---

**日志撰写时间**: 2026-07-29 23:30  
**任务执行者**: Claude (Opus 4.8)  
**影响范围**: 调试工具，不影响主业务逻辑  
**风险评估**: 低（独立调试模式）
