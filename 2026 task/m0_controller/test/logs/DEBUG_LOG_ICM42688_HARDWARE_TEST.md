# ICM42688陀螺仪硬件测试调试日志

**日期**: 2026-07-23  
**目标**: 验证ICM42688 IMU硬件连接和数据读取功能  
**结果**: ✅ 测试通过

---

## 1. 项目背景

### 1.1 初始状态
- 项目已完成软件测试模式验证（参考 `DEBUG_LOG_UART0_AND_SOFTWARE_TEST_MODE.md`）
- UART0调试功能正常工作
- 需要开始硬件外设验证，首选目标：ICM42688陀螺仪/加速度计

### 1.2 硬件配置
- **芯片**: ICM42688-P (6轴IMU)
- **接口**: SPI (SPI1)
- **引脚**:
  - SCK: SPI1_SCLK
  - MOSI: SPI1_PICO
  - MISO: SPI1_POCI
  - CS: ICM42688_CS_PIN (GPIO)
- **供电**: 3.3V
- **采样率**: 1000 Hz
- **量程**: 加速度计 ±4g, 陀螺仪 ±2000 dps

---

## 2. 开发过程

### 2.1 测试程序设计

#### 设计思路
创建一个**独立的硬件验证测试**，与完整控制应用分离：
- 使用裸机模式（不启动FreeRTOS）
- 分步骤验证（SPI通信 → 初始化 → 数据读取 → 校准 → 连续监测）
- 每步输出详细日志，便于定位问题

#### 文件结构
```
tests/test_icm42688_hardware.c    # 测试主程序
src/main.c                        # 测试模式切换
.eide/eide.yml                    # 编译配置
docs/ICM42688_HARDWARE_TEST_GUIDE.md      # 详细测试指南
docs/ICM42688_TEST_QUICKSTART.md          # 快速开始指南
```

### 2.2 测试程序实现

#### 测试流程（5步）

**步骤1: SPI通信测试**
```c
// 直接读取WHO_AM_I寄存器（0x75）
uint8_t tx_buf[2] = {ICM42688_WHO_AM_I | 0x80, 0x00};  // 读操作需要设置bit7
uint8_t rx_buf[2] = {0};

// CS拉低 → 发送命令 → 读取数据 → CS拉高
DL_GPIO_clearPins(ICM42688_PORT, ICM42688_CS_PIN);
// ... SPI传输 ...
DL_GPIO_setPins(ICM42688_PORT, ICM42688_CS_PIN);

// 预期值: 0x47
```

**步骤2: 传感器初始化**
```c
icm42688_config_t config = {
    .interface_type = ICM42688_INTERFACE_SPI,
    .acc_sample     = ICM42688_ACC_SAMPLE_SGN_4G,
    .gyro_sample    = ICM42688_GYRO_SAMPLE_SGN_2000DPS,
    .sample_rate    = ICM42688_SAMPLE_RATE_1000
};

icm42688_mspm0_bind(&config);
status = icm42688_init();
```

**步骤3: 原始数据读取**
```c
for (int i = 0; i < 5; i++) {
    icm42688_read(&g_imu_data);
    printf("Sample %d: Accel(%d,%d,%d) Gyro(%d,%d,%d)\n",
           i, acc_x, acc_y, acc_z, gyro_x, gyro_y, gyro_z);
}
```

**步骤4: 陀螺仪校准**
```c
// 静止状态下采样100次，计算零偏
icm42688_calibrate_gyro(100, 10);  // 100 samples, 10ms interval
icm42688_get_gyro_bias(&bias);
```

**步骤5: 连续监测**
```c
// 10Hz输出，应用校准后的数据
printf("[%d] Accel(g): %.3f %.3f %.3f | Gyro(dps): %.2f %.2f %.2f\n",
       counter, acc_x_g, acc_y_g, acc_z_g, gyro_x_dps, gyro_y_dps, gyro_z_dps);
```

### 2.3 主程序修改

#### 添加测试模式切换
`src/main.c`:
```c
#define TEST_MODE_CONTROL_APP    0  // 完整控制应用
#define TEST_MODE_ICM42688       1  // ICM42688硬件测试

#define ACTIVE_TEST_MODE  TEST_MODE_ICM42688  // <-- 切换测试模式
```

#### ICM42688测试模式入口
```c
#elif (ACTIVE_TEST_MODE == TEST_MODE_ICM42688)
    /* 发送测试字符确认UART工作 */
    DL_UART_Main_transmitData(UART0_INST, 'S');
    
    /* 初始化平台定时器（用于校准延时） */
    PlatformTime_Init();
    
    printf("ICM42688 Test Starting...\n");
    
    /* 运行测试循环 */
    for (;;) {
        test_icm42688_main_loop();
    }
```

### 2.4 编译配置调整

#### 问题1: 测试文件未被编译
**现象**: `test_icm42688_hardware.c` 不在源文件列表中

**解决方案**: 修改 `.eide/eide.yml`:
```yaml
srcDirs:
  - src
  - Debug
  - tests        # ← 添加tests目录
  - modules/MCP23017/src
  - modules/ICM42688/src
  - modules/Motion Control/src
  - modules/Sens-Decision/src
```

#### 问题2: 其他测试文件导致编译错误
**现象**: `test_mcp23017.c`, `test_platform_time.c` 等文件有mock函数冲突

**解决方案**: 在 `excludeList` 中排除其他测试文件:
```yaml
excludeList:
  - "**/example_usage.c"
  - "**/temp/**"
  - "**/OLED*"
  - "tests/test_control_app.c"       # 排除
  - "tests/test_icm42688.c"          # 排除
  - "tests/test_mcp23017.c"          # 排除
  - "tests/test_motion_control.c"    # 排除
  - "tests/test_platform_time.c"     # 排除
  - "tests/test_square_path.c"       # 排除
  - "tests/test_target_adapters.c"   # 排除
  - "tests/test_uart0_debug.c"       # 排除
  - "tests/test_uart0_simple.c"      # 排除
  # 只保留 test_icm42688_hardware.c
```

**关键经验**: 
- 通配符模式 `**/tests/*.c` 在EIDE中不工作
- 必须使用相对路径 `tests/file.c` 格式

---

## 3. 调试过程

### 3.1 问题1: 串口无输出

**现象**: 编译烧录后，串口监控完全无输出

**原因**: ICM42688测试模式中未初始化UART

**定位过程**:
1. 检查 `main.c` 的 `TEST_MODE_ICM42688` 分支
2. 发现直接调用 `PlatformTime_Init()` 和 `test_icm42688_main_loop()`
3. 缺少UART初始化和启动信息

**解决方案**: 在测试模式入口添加UART测试输出
```c
/* 发送测试字符验证UART工作 */
while (DL_UART_isTXFIFOFull(UART0_INST));
DL_UART_Main_transmitData(UART0_INST, 'S');
while (DL_UART_isTXFIFOFull(UART0_INST));
DL_UART_Main_transmitData(UART0_INST, '\n');

DelayMs(100);  // 等待UART稳定

printf("ICM42688 Test Starting...\n");
```

**教训**: 
- 裸机模式下，UART虽然由`SYSCFG_DL_init()`初始化，但最好发送测试字符确认
- 添加启动横幅帮助确认程序正在运行

---

## 4. 测试结果

### 4.1 完整测试输出

```
ICM42688 Test Starting...

=====================================
 ICM42688 Hardware Validation Test  
=====================================
Board: MSPM0G3507
SPI: SPI1_INST
CS:  ICM42688_PORT/ICM42688_CS_PIN


=== TEST 1: SPI Communication ===
Reading WHO_AM_I register (should be 0x47)...
WHO_AM_I = 0x47 (expected 0x47)
✓ SPI communication OK

=== TEST 2: Sensor Initialization ===
Binding MSPM0 adapter...
Calling icm42688_init()...
Init status: 0 (OK)
✓ Sensor initialized successfully
  Accel scale: 0.000122 g/LSB
  Gyro scale:  0.061035 dps/LSB

=== TEST 3: Raw Data Reading ===
Reading 5 samples...

Sample |   Accel (raw)    |    Gyro (raw)     | Temp
-------|------------------|-------------------|------
  1    |  -109  5771  5938 |     21    -5     14 | (raw)
  2    |   -98  5761  5927 |     21    -5     14 | (raw)
  3    |  -104  5766  5926 |     22    -4     14 | (raw)
  4    |   -84  5760  5926 |     24    -5     15 | (raw)
  5    |  -100  5746  5928 |     22    -6     14 | (raw)

✓ Raw data reading OK
  Note: Accel Z should be ~±16384 (1g @ ±4g range)
  Note: Gyro should be near zero if stationary

=== TEST 4: Gyro Calibration ===
⚠ KEEP DEVICE STATIONARY!
Calibrating with 100 samples...
✓ Calibration complete
  Gyro bias (dps): X=1.299, Y=-0.298, Z=0.876

=== TEST 5: Continuous Monitoring ===
Reading at 10 Hz (press reset to stop)...

[   0] Accel(g):  -0.012   0.705   0.725 | Gyro(dps):    -0.02     0.05    -0.02 | Temp: 24.2°C
[   1] Accel(g):  -0.013   0.704   0.726 | Gyro(dps):    -0.01     0.04    -0.03 | Temp: 24.2°C
[   2] Accel(g):  -0.012   0.705   0.725 | Gyro(dps):    -0.02     0.06    -0.01 | Temp: 24.2°C
...
```

### 4.2 数据分析

#### 测试1: SPI通信 ✅
- **WHO_AM_I**: 0x47 (正确)
- **结论**: SPI接线正确，通信协议正确

#### 测试2: 传感器初始化 ✅
- **状态**: ICM42688_STATUS_OK (0)
- **加速度计比例因子**: 0.000122 g/LSB
  - 验证: 32768 LSB / 4g = 8192 LSB/g → 1/8192 ≈ 0.000122 ✓
- **陀螺仪比例因子**: 0.061035 dps/LSB
  - 验证: 32768 LSB / 2000 dps ≈ 16.384 LSB/dps → 1/16.384 ≈ 0.061 ✓

#### 测试3: 原始数据读取 ✅
```
平均值:
  Accel X:  -99  LSB  →  -0.012 g
  Accel Y: 5761  LSB  →   0.703 g
  Accel Z: 5929  LSB  →   0.723 g
  
  Gyro  X:   22  LSB  →   1.34 dps
  Gyro  Y:   -5  LSB  →  -0.31 dps
  Gyro  Z:   14  LSB  →   0.85 dps
```

**设备姿态分析**:
- 总加速度: √(0.012² + 0.703² + 0.723²) ≈ **1.01 g** ✓
- 设备处于**倾斜状态**（Y、Z方向各承受约0.7g）
- 陀螺仪数值较小（< 2 dps），设备基本静止 ✓

#### 测试4: 陀螺仪校准 ✅
```
零偏 (dps):
  X: +1.299  (良好)
  Y: -0.298  (优秀)
  Z: +0.876  (良好)
```

**评估标准**:
- **< 1 dps**: 优秀
- **1-5 dps**: 良好（可接受）
- **5-10 dps**: 一般（可用，但建议重新校准）
- **> 10 dps**: 异常（检查硬件或温度稳定性）

**结论**: 所有轴零偏都在良好范围内 ✓

#### 测试5: 连续监测 ✅
```
静止状态:
  Accel:  -0.012   0.705   0.725 g  ← 总和 ≈ 1.0g
  Gyro:   -0.02    0.05   -0.02 dps ← 接近零
  Temp:   24.2°C                    ← 室温正常
```

**传感器性能**:
- 加速度计噪声: ±0.001 g (低噪声) ✓
- 陀螺仪噪声: ±0.05 dps (低噪声) ✓
- 温度读数: 24.2°C (符合室温)

---

## 5. 关键技术要点

### 5.1 ICM42688寄存器配置

#### WHO_AM_I读取（SPI）
```c
// SPI读操作需要在地址byte设置bit7
uint8_t read_cmd = ICM42688_WHO_AM_I | 0x80;  // 0x75 | 0x80 = 0xF5

// SPI时序：
// 1. CS拉低
// 2. 发送地址字节（0xF5）
// 3. 发送dummy字节，同时接收数据
// 4. CS拉高
```

#### 初始化序列（简化）
```c
1. 软复位 (DEVICE_CONFIG寄存器, bit0)
2. 等待复位完成 (~1ms)
3. 验证WHO_AM_I
4. 配置电源管理 (PWR_MGMT0寄存器)
   - 加速度计使能 (bit1:0 = 0b11)
   - 陀螺仪使能 (bit3:2 = 0b11)
5. 配置加速度计量程 (ACCEL_CONFIG0)
6. 配置陀螺仪量程 (GYRO_CONFIG0)
7. 配置采样率 (ACCEL_CONFIG0/GYRO_CONFIG0的ODR字段)
```

### 5.2 数据转换公式

#### 加速度计 (±4g量程)
```c
// LSB → g
float accel_g = (float)raw_value * 0.000122f;

// 或使用配置的比例因子
float accel_scale;
icm42688_get_scale_factors(&accel_scale, NULL);
float accel_g = (float)raw_value * accel_scale;
```

#### 陀螺仪 (±2000dps量程)
```c
// LSB → dps
float gyro_dps = (float)raw_value * 0.061035f;

// 应用零偏校准
float gyro_calibrated = gyro_dps - gyro_bias;
```

#### 温度
```c
// LSB → °C
float temp_c = ((float)raw_value / 132.48f) + 25.0f;
```

### 5.3 校准算法

#### 陀螺仪零偏校准
```c
// 静止状态下采样N次，计算平均值
float sum_x = 0, sum_y = 0, sum_z = 0;

for (int i = 0; i < N; i++) {
    icm42688_read(&data);
    sum_x += data.gyro_dps.x;
    sum_y += data.gyro_dps.y;
    sum_z += data.gyro_dps.z;
    delay_ms(interval);
}

bias.x = sum_x / N;
bias.y = sum_y / N;
bias.z = sum_z / N;
```

**注意事项**:
- 校准期间设备必须**完全静止**
- 建议等待1-2分钟让温度稳定后再校准
- 每次上电后应重新校准

---

## 6. 故障排查指南

### 6.1 SPI通信问题

| WHO_AM_I值 | 可能原因 | 解决方案 |
|-----------|---------|---------|
| 0x00 | MISO未连接或浮空 | 检查MISO接线 |
| 0xFF | MOSI未连接，或CS一直为低 | 检查MOSI和CS |
| 其他值 | SPI时钟模式错误 | 确认CPOL=0, CPHA=0 |
| 随机值 | 时钟频率过高 | 降低到1-2 MHz重试 |

### 6.2 初始化失败

**ICM42688_STATUS_NOT_READY**:
- 检查供电是否稳定（3.3V）
- 增加初始化后的延时
- 尝试多次复位

**ICM42688_STATUS_BAD_ID**:
- 重新检查SPI通信
- 确认芯片型号（ICM42688 vs ICM42605等）

### 6.3 数据异常

**加速度计数据全零**:
- 检查PWR_MGMT0寄存器是否正确配置
- 确认读取的是数据寄存器而非FIFO

**陀螺仪数据漂移**:
- 等待温度稳定（1-2分钟）
- 重新校准
- 检查是否有机械振动干扰

**温度读数异常**:
- 转换公式可能不适用当前ICM42688版本
- 参考最新数据手册

---

## 7. 后续工作

### 7.1 集成到控制应用

✅ **硬件验证完成**，可以切换到完整控制模式：

#### 步骤1: 切换测试模式
`src/main.c`:
```c
#define ACTIVE_TEST_MODE  TEST_MODE_CONTROL_APP  // 改为0
```

#### 步骤2: 启用硬件模式
`src/control_app.c`:
```c
#define SOFTWARE_TEST_MODE 0  // 启用硬件模式
```

#### 步骤3: 验证集成
在完整应用中检查：
- [ ] `icm42688_mspm0_bind()` 在初始化时被调用
- [ ] `icm42688_read()` 在传感器读取循环中被调用
- [ ] `state_evaluator_update()` 接收IMU数据并返回 `SD_OK`
- [ ] 姿态估计（theta）随设备旋转变化
- [ ] EKF融合陀螺仪和编码器数据

### 7.2 其他硬件验证

建议按以下顺序验证其他硬件：

1. **编码器** (Encoder)
   - 验证计数方向
   - 校准速度计算
   - 检查噪声和抖动

2. **红外传感器阵列** (MCP23017 + ADC)
   - MCP23017 I2C通信
   - GPIO扩展功能
   - ADC采样精度

3. **电机驱动**
   - PWM输出波形
   - 方向控制
   - 开环速度测试

4. **完整闭环控制**
   - 传感器融合
   - 轨迹跟踪
   - PID调参

### 7.3 高级功能

如果需要更高性能：

#### 动态校准
```c
// 运行时检测静止状态，自动更新零偏
if (vehicle_speed < 0.01f && variance < threshold) {
    update_gyro_bias();
}
```

#### 温度补偿
```c
// 建立温度-零偏曲线
float bias_corrected = bias_nominal + k_temp * (temp - 25.0f);
```

#### FIFO模式
```c
// 使用FIFO批量读取，减少CPU中断频率
// 适用于高采样率场景（1kHz+）
```

---

## 8. 经验总结

### 8.1 调试技巧

1. **分步验证**: 从最底层（SPI通信）开始，逐层向上
2. **测试字符**: 裸机模式下先发送简单字符确认UART
3. **详细日志**: 每个关键步骤输出状态和数值
4. **硬件优先**: 先排除硬件问题，再调试软件
5. **对比参考**: 将实测值与数据手册理论值对比

### 8.2 常见陷阱

1. **SPI读操作**: 地址字节必须设置bit7（读标志位）
2. **UART初始化**: 裸机模式下容易忘记UART测试输出
3. **零偏校准**: 必须在静止且温度稳定后进行
4. **编译配置**: EIDE的excludeList不支持复杂通配符
5. **数据解读**: 倾斜放置时，加速度计读数不是(0,0,1g)

### 8.3 最佳实践

1. **独立测试**: 每个硬件模块单独验证，再集成
2. **文档同步**: 边调试边记录，包括失败的尝试
3. **版本控制**: 测试通过后提交代码和文档
4. **测试记录**: 保存实际测试输出，便于对比
5. **可复现性**: 提供完整的测试步骤和预期结果

---

## 9. 参考资料

### 9.1 相关文档
- `ICM42688数据手册`: 寄存器定义和电气特性
- `DEBUG_LOG_UART0_AND_SOFTWARE_TEST_MODE.md`: UART调试和软件测试模式
- `ICM42688_HARDWARE_TEST_GUIDE.md`: 详细测试指南
- `ICM42688_TEST_QUICKSTART.md`: 快速开始指南

### 9.2 源代码
- `tests/test_icm42688_hardware.c`: 硬件测试主程序
- `modules/ICM42688/inc/icm42688_hal.h`: HAL接口定义
- `modules/ICM42688/src/icm42688_hal.c`: HAL实现
- `modules/ICM42688/inc/icm42688_mspm0.h`: MSPM0平台适配
- `modules/ICM42688/src/icm42688_mspm0.c`: SPI驱动实现

### 9.3 工具链
- EIDE (Embedded IDE)
- Arm Compiler 6 (armclang)
- TI SysConfig 1.26.2
- XDS110 调试器

---

## 10. 测试记录表

```
=====================================
测试日期: 2026-07-23
测试人员: [用户名]
硬件版本: MSPM0G3507 + ICM42688-P
=====================================

[ ✓ ] TEST 1: SPI通信
      WHO_AM_I = 0x47

[ ✓ ] TEST 2: 传感器初始化
      Status: OK (0)
      Accel scale: 0.000122 g/LSB
      Gyro scale: 0.061035 dps/LSB

[ ✓ ] TEST 3: 原始数据读取
      Accel: (-99, 5761, 5929) LSB
      Gyro: (22, -5, 14) LSB

[ ✓ ] TEST 4: 陀螺仪校准
      Bias: X=1.299, Y=-0.298, Z=0.876 dps
      评价: 良好

[ ✓ ] TEST 5: 连续监测
      静止状态: Accel≈1.0g, Gyro≈0dps
      温度: 24.2°C

问题记录: 无

结论: ✅ 测试通过，硬件工作正常
```

---

**结论**: ICM42688陀螺仪硬件验证成功完成。所有测试通过，传感器工作正常，数据质量良好。可以继续进行其他硬件验证和完整系统集成。
