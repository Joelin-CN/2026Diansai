# ICM42688测试快速开始

## 当前状态

✅ **已配置**: ICM42688硬件测试模式  
✅ **文件已创建**: `tests/test_icm42688_hardware.c`  
✅ **主程序已配置**: `src/main.c` - `TEST_MODE_ICM42688`

---

## 快速测试步骤

### 1. 确认配置

**src/main.c** (第14行):
```c
#define ACTIVE_TEST_MODE  TEST_MODE_ICM42688  // ✓ 已设置
```

### 2. 硬件连接

```
ICM42688 → MSPM0G3507
------------------------
SCL/SCK  → SPI1_SCLK
SDA/SDI  → SPI1_PICO (MOSI)
SDO      → SPI1_POCI (MISO)
CS       → ICM42688_CS_PIN (GPIO)
VDD      → 3.3V
GND      → GND
```

⚠️ **检查事项**:
- [ ] 3.3V供电稳定（推荐加100nF去耦电容）
- [ ] MOSI/MISO不要接反
- [ ] CS默认高电平（空闲）

### 3. 编译

在EIDE中按 **F7** 或点击"构建"按钮

### 4. 串口连接

```
UART0 TX (PA10) → USB转串口 RX
GND             → GND

串口设置: 115200-8-N-1
```

### 5. 烧录并运行

点击EIDE的"下载"按钮，打开串口监控软件

---

## 预期输出

### 成功场景
```
=====================================
 ICM42688 Hardware Validation Test  
=====================================

=== TEST 1: SPI Communication ===
WHO_AM_I = 0x47 (expected 0x47)
✓ SPI communication OK

=== TEST 2: Sensor Initialization ===
✓ Sensor initialized successfully
  Accel scale: 0.000122 g/LSB
  Gyro scale:  0.061035 dps/LSB

=== TEST 3: Raw Data Reading ===
✓ Raw data reading OK

=== TEST 4: Gyro Calibration ===
⚠ KEEP DEVICE STATIONARY!
✓ Calibration complete
  Gyro bias (dps): X=0.125, Y=-0.342, Z=0.078

=== TEST 5: Continuous Monitoring ===
[   0] Accel(g):   0.015  -0.056   1.002 | Gyro(dps):     0.12    -0.08     0.03
[   1] Accel(g):   0.016  -0.055   1.001 | Gyro(dps):     0.10    -0.10     0.05
...
```

### 失败场景示例

**SPI通信失败**:
```
WHO_AM_I = 0x00 (expected 0x47)
✗ FAILED: Wrong device ID or SPI error
```
→ 检查接线和供电

**传感器初始化失败**:
```
Init status: 1 (NOT_READY)
✗ FAILED: Sensor not responding or timeout
```
→ 检查SPI配置和电源

---

## 快速故障排查

| 问题 | 检查项 |
|------|--------|
| WHO_AM_I = 0x00 | MISO是否连接？CS是否工作？ |
| WHO_AM_I = 0xFF | MOSI是否连接？供电是否正常？ |
| 初始化超时 | SPI时钟频率是否过高（应≤8MHz）？ |
| 数据全零 | 传感器是否正确配置为寄存器读取模式？ |
| 陀螺仪零偏>50dps | 设备是否静止？是否需要等待温度稳定？ |

---

## 测试完成后

### 切换回完整控制应用

1. **修改 src/main.c**:
   ```c
   #define ACTIVE_TEST_MODE  TEST_MODE_CONTROL_APP
   ```

2. **修改 src/control_app.c**:
   ```c
   #define SOFTWARE_TEST_MODE 0  // 启用硬件模式
   ```

3. **重新编译和烧录**

### 验证集成

在完整应用中检查：
- [ ] `state_evaluator_update()` 返回 `SD_OK`
- [ ] IMU数据被正确读取和融合
- [ ] 姿态估计随设备旋转变化

---

## 测试模式说明

程序中定义了两种测试模式：

```c
#define TEST_MODE_CONTROL_APP    0  // 完整控制应用 (FreeRTOS)
#define TEST_MODE_ICM42688       1  // ICM42688测试 (裸机)
```

- **TEST_MODE_ICM42688**: 裸机测试，不启动FreeRTOS，只测试ICM42688
- **TEST_MODE_CONTROL_APP**: 完整控制应用，包含所有传感器和算法

---

## 相关文件

| 文件 | 说明 |
|------|------|
| `tests/test_icm42688_hardware.c` | 测试程序主体 |
| `src/main.c` | 测试模式选择 |
| `modules/ICM42688/inc/icm42688_hal.h` | HAL接口 |
| `modules/ICM42688/src/icm42688_mspm0.c` | MSPM0适配层 |
| `docs/ICM42688_HARDWARE_TEST_GUIDE.md` | 详细测试指南 |

---

## 测试记录

```
日期: 2026-07-23
状态: [ ] 待测试  [ ] 进行中  [ ] 通过  [ ] 失败

测试结果:
[ ] TEST 1: SPI通信 - WHO_AM_I = 0x____
[ ] TEST 2: 初始化
[ ] TEST 3: 原始数据读取
[ ] TEST 4: 陀螺仪校准 - 零偏 X=____ Y=____ Z=____
[ ] TEST 5: 连续监测

问题:
____________________________________________________

```
