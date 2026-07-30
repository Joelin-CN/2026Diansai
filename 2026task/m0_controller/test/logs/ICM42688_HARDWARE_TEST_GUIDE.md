# ICM42688陀螺仪硬件验证指南

**日期**: 2026-07-23  
**目标**: 验证ICM42688 IMU硬件连接和数据读取

---

## 1. 测试前准备

### 1.1 硬件连接检查

确认以下连接正确：

**ICM42688 SPI接口**（参考SysConfig配置）:
- **SCK** (SPI1_SCLK) → ICM42688 SCL
- **MOSI** (SPI1_PICO) → ICM42688 SDA/SDI
- **MISO** (SPI1_POCI) → ICM42688 SDO
- **CS** (ICM42688_CS_PIN) → ICM42688 CS
- **VDD** → 3.3V
- **GND** → GND

⚠️ **注意事项**:
1. ICM42688需要稳定的3.3V供电（推荐加去耦电容）
2. CS引脚需要上拉电阻（如果硬件设计中没有）
3. SPI时钟速度：建议≤8MHz（SysConfig中配置）
4. 确认SPI模式：CPOL=0, CPHA=0 (Mode 0)

### 1.2 查看当前引脚配置

在SysConfig中确认：
```
SPI1:
  - Mode: Controller (Master)
  - Clock: ≤8 MHz
  - Frame format: Motorola 3-wire
  - Data size: 8 bits
  
GPIO (ICM42688_CS_PIN):
  - Direction: Output
  - Initial value: High (CS默认禁用)
```

### 1.3 编译配置

**src/main.c**:
```c
#define ACTIVE_TEST_MODE  TEST_MODE_ICM42688  // ✓ 已配置
```

---

## 2. 编译和烧录

### 2.1 编译
```bash
# 在EIDE中按 F7
# 或使用命令行
eide build
```

输出文件: `build/Debug/<project_name>.hex`

### 2.2 烧录
1. 连接XDS110调试器到MSPM0G3507
2. 在EIDE中点击"下载"按钮
3. 或使用命令行工具

### 2.3 串口监控

**串口设置**:
- 端口: USB转串口设备号
- 波特率: 115200
- 数据位: 8
- 校验: None
- 停止位: 1

**连接方式**:
- UART0 TX (PA10) → USB转串口 RX
- GND → GND

---

## 3. 测试流程

测试程序会自动按以下顺序执行：

### 3.1 测试1：SPI通信测试

**测试内容**: 读取WHO_AM_I寄存器

**预期输出**:
```
=== TEST 1: SPI Communication ===
Reading WHO_AM_I register (should be 0x47)...
WHO_AM_I = 0x47 (expected 0x47)
✓ SPI communication OK
```

**如果失败**:
```
WHO_AM_I = 0x00 (expected 0x47)
✗ FAILED: Wrong device ID or SPI error
  Check: SPI wiring, CS pin, clock polarity/phase
```

**故障排查**:
- [ ] 检查SPI接线（特别是MOSI/MISO是否接反）
- [ ] 检查CS引脚定义是否正确
- [ ] 用示波器测量CS信号（应该有低脉冲）
- [ ] 检查ICM42688供电（3.3V）
- [ ] 确认SPI时钟模式（CPOL=0, CPHA=0）

---

### 3.2 测试2：传感器初始化

**测试内容**: 复位传感器并配置寄存器

**预期输出**:
```
=== TEST 2: Sensor Initialization ===
Binding MSPM0 adapter...
Calling icm42688_init()...
Init status: 0 (OK)
✓ Sensor initialized successfully
  Accel scale: 0.000122 g/LSB
  Gyro scale:  0.061035 dps/LSB
```

**配置参数**:
- 加速度计量程: ±4g
- 陀螺仪量程: ±2000 dps
- 采样率: 1000 Hz

**如果失败**:
- `ICM42688_STATUS_BAD_ID`: WHO_AM_I验证失败（硬件问题）
- `ICM42688_STATUS_NOT_READY`: 传感器无响应或超时

---

### 3.3 测试3：原始数据读取

**测试内容**: 连续读取5次原始ADC值

**预期输出**:
```
=== TEST 3: Raw Data Reading ===
Reading 5 samples...

Sample |   Accel (raw)    |    Gyro (raw)     | Temp
-------|------------------|-------------------|------
  1    |    123  -456  16384 |     12    -8     3 | 3250
  2    |    125  -454  16382 |     10    -5     2 | 3251
  3    |    124  -455  16383 |     11    -7     1 | 3250
  4    |    122  -457  16385 |     13    -6     4 | 3252
  5    |    123  -456  16384 |     12    -9     2 | 3251

✓ Raw data reading OK
  Note: Accel Z should be ~±16384 (1g @ ±4g range)
  Note: Gyro should be near zero if stationary
```

**数值检查**:

1. **加速度计Z轴** (垂直放置，Z轴朝上):
   - 预期值: ~16384 LSB (1g)
   - 如果倒置: ~-16384 LSB (-1g)
   - 容差: ±2000 LSB

2. **陀螺仪** (静止状态):
   - 预期值: 接近0（±50 LSB内）
   - 如果数值很大（>500）: 传感器可能在移动或有干扰

3. **温度**:
   - 原始值: ~3250 (对应约25°C)
   - 转换公式: `temp_c = (raw / 132.48) + 25.0`

---

### 3.4 测试4：陀螺仪校准

**测试内容**: 静止状态下测量陀螺仪零偏

⚠️ **重要**: 在此阶段保持设备**完全静止**！

**预期输出**:
```
=== TEST 4: Gyro Calibration ===
⚠ KEEP DEVICE STATIONARY!
Calibrating with 100 samples...
✓ Calibration complete
  Gyro bias (dps): X=0.125, Y=-0.342, Z=0.078
```

**零偏检查**:
- **正常范围**: 每轴 < 5 dps
- **可接受**: 每轴 < 10 dps
- **异常**: 任意轴 > 50 dps（可能在移动或硬件故障）

如果看到警告:
```
⚠ WARNING: Bias seems large, device may have been moving
```
→ 重新上电，确保静止后再次测试

---

### 3.5 测试5：连续监测

**测试内容**: 10 Hz数据输出，应用校准后的数据

**预期输出** (静止状态):
```
=== TEST 5: Continuous Monitoring ===
Reading at 10 Hz (press reset to stop)...

[   0] Accel(g):   0.015  -0.056   1.002 | Gyro(dps):     0.12    -0.08     0.03 | Temp: 25.3°C
[   1] Accel(g):   0.016  -0.055   1.001 | Gyro(dps):     0.10    -0.10     0.05 | Temp: 25.3°C
[   2] Accel(g):   0.015  -0.056   1.003 | Gyro(dps):     0.11    -0.09     0.02 | Temp: 25.4°C
...
[  50] Accel(g):   0.015  -0.055   1.002 | Gyro(dps):     0.09    -0.11     0.04 | Temp: 25.4°C

>>> Try rotating the device slowly to see gyro response <<<
```

**数值检查**:

1. **加速度计** (单位: g):
   - Z轴: ~1.0g (垂直放置)
   - X, Y轴: ~0.0g
   - 轻微晃动时应该看到变化

2. **陀螺仪** (单位: dps):
   - 静止时: 每轴 < 1 dps
   - 缓慢旋转时: 应该看到对应轴的角速度变化
   - 例如：绕Z轴旋转 → Gyro Z轴有明显数值

3. **温度**:
   - 室温: 20-30°C
   - 运行几分钟后会略微升高

---

## 4. 互动测试建议

在连续监测阶段（测试5），可以进行以下操作验证传感器响应：

### 4.1 加速度计测试
```
动作                    | 预期加速度变化
-----------------------|---------------------------
水平放置               | Z≈1.0, X≈0, Y≈0
垂直放置（X轴朝上）    | X≈1.0, Y≈0, Z≈0
倒置                   | Z≈-1.0, X≈0, Y≈0
轻轻晃动               | 三轴都有瞬时变化
```

### 4.2 陀螺仪测试
```
动作                    | 预期陀螺仪变化
-----------------------|---------------------------
静止                   | 三轴 ≈ 0
绕Z轴慢速旋转          | Z轴显示角速度（±30~100 dps）
绕X轴倾斜              | X轴显示角速度
绕Y轴倾斜              | Y轴显示角速度
快速旋转               | 对应轴显示大的角速度值
```

---

## 5. 常见问题与解决

### 5.1 SPI通信失败 (WHO_AM_I = 0x00 或 0xFF)

**可能原因**:
1. **接线错误**
   - MOSI/MISO接反 → 交换后重试
   - CS未连接或一直为低 → 检查GPIO配置

2. **供电问题**
   - 电压不足 → 用万用表测量VDD引脚（应该是3.3V）
   - 瞬态电流不足 → 在VDD附近加100nF去耦电容

3. **SPI配置错误**
   - 时钟极性错误 → 确认CPOL=0, CPHA=0
   - 时钟频率太高 → 降低到1-2MHz重试

4. **硬件损坏**
   - 尝试更换ICM42688芯片

### 5.2 初始化超时 (ICM42688_STATUS_NOT_READY)

**检查步骤**:
1. 确认测试1的SPI通信成功
2. 检查复位逻辑（如果有硬件复位引脚）
3. 增加初始化延迟时间（修改`icm42688_hal.c`）

### 5.3 原始数据全零或不变

**可能原因**:
1. **传感器未启用** → 检查PWR_MGMT0寄存器配置
2. **FIFO模式错误** → 确认使用寄存器直接读取模式
3. **时钟未启动** → 检查内部时钟使能

### 5.4 陀螺仪零偏过大

**正常情况**:
- 每次上电零偏略有不同是正常的（±5 dps）

**异常情况**:
- 零偏 > 50 dps → 可能硬件故障或温度漂移严重
- 解决方法：
  1. 等待1-2分钟让温度稳定
  2. 重新校准
  3. 如果持续异常，更换传感器

### 5.5 数据噪声过大

**检查**:
- PCB走线是否靠近电机驱动或开关电源
- 是否有足够的滤波电容
- 数字地和模拟地是否正确分离

**软件滤波**:
- 可以在`state_evaluate.c`中启用卡尔曼滤波
- 或添加简单的移动平均滤波

---

## 6. 下一步工作

### 6.1 集成到控制应用

测试通过后，修改 `src/main.c`:
```c
#define ACTIVE_TEST_MODE  TEST_MODE_CONTROL_APP  // 切换回控制模式
```

然后修改 `src/control_app.c`:
```c
#define SOFTWARE_TEST_MODE 0  // 启用硬件模式
```

### 6.2 验证传感器融合

在完整控制应用中检查：
- [ ] `state_evaluator_update()` 使用IMU数据
- [ ] EKF融合陀螺仪和编码器数据
- [ ] 姿态估计精度（与实际旋转角度对比）

### 6.3 高级校准

如果需要更高精度：
1. **加速度计校准**: 6面法校准（每面静止采样）
2. **磁力计校准**: 如果使用磁力计（ICM42688不含）
3. **温度补偿**: 记录温度-零偏曲线

---

## 7. 测试记录模板

```
测试日期: ___________
测试人员: ___________
硬件版本: ___________

[ ] 测试1: SPI通信      - WHO_AM_I = 0x____
[ ] 测试2: 传感器初始化  - Status: ____
[ ] 测试3: 原始数据读取  - Accel Z = ______, Gyro静止 < 50
[ ] 测试4: 陀螺仪校准    - Bias: X=____ Y=____ Z=____
[ ] 测试5: 连续监测      - 旋转响应正常

问题记录:
__________________________________________________
__________________________________________________

结论: [ ] 通过  [ ] 失败  [ ] 需返工
```

---

## 8. 参考资料

### 8.1 相关文档
- `ICM42688数据手册`: 寄存器详细说明
- `WIRING_AND_SYSCONFIG.md`: 引脚配置参考
- `DEBUG_LOG_UART0_AND_SOFTWARE_TEST_MODE.md`: UART调试说明

### 8.2 源代码位置
- 测试程序: `tests/test_icm42688_hardware.c`
- HAL层: `modules/ICM42688/inc/icm42688_hal.h`
- 平台适配: `modules/ICM42688/src/icm42688_mspm0.c`
- 主程序: `src/main.c` (测试模式切换)

---

**测试完成后请填写测试记录并保存！**
