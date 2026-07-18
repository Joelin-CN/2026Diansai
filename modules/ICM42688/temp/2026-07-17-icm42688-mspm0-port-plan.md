# ICM42688 MSPM0G3507 Driver Port Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将现有 STM32 ICM42688 驱动和 Mahony 六轴 AHRS 迁移为可在 MSPM0G3507 LQFP-48 上使用的独立模块，并提供完整的控制器工程集成 README。

**Architecture:** 保留通信、系统服务函数指针，使 `icm42688_hal` 和 `ahrs_hal` 不依赖 TI DriverLib；`icm42688_mspm0` 只负责连接 SysConfig 生成的 SPI1、PB20 和 TIMG0 符号。平台无关代码通过 `temp/tests` 中的伪硬件接口在主机上测试，目标工程配置只记录在 README，不修改 `controller/`。

**Tech Stack:** C99、TI MSPM0 DriverLib、MSPM0 SysConfig、ARM Compiler 6、MinGW GCC 主机测试、Mahony quaternion AHRS、FreeRTOS SysTick/TIMG0 分离计时。

## Global Constraints

- 目标器件固定为 MSPM0G3507 LQFP-48。
- SPI1 引脚固定为 POCI PA16、SCLK PA17、PICO PA18；ICM42688_CS 固定为 PB20，低有效。
- SPI 固定为 Mode 0、8 位、MSB first，初始时钟 1 MHz。
- 不修改 `controller/`、其 `.syscfg` 文件或自动生成文件；所有控制器工程改动写入 `modules/ICM42688/README.md`。
- 中间产物和主机测试代码只放在 `modules/ICM42688/temp/`。
- 最终模块文件只放在 `modules/ICM42688/inc/`、`modules/ICM42688/src/` 和 `modules/ICM42688/README.md`。
- 不修改 `pictures/`；不添加 DMA、FIFO、数据就绪中断、I2C、磁力计或九轴融合。
- TIMG0 配置为 1 MHz 自由运行计数器；FreeRTOS 保留 SysTick 作为系统节拍。
- 不执行 git commit，除非用户另行明确要求。

## File Map

- Create `inc/icm42688_hal.h`: 寄存器、配置、状态、原始数据和物理量数据接口。
- Create `src/icm42688_hal.c`: 平台无关初始化、连续读取、换算和静止零偏校准。
- Create `inc/ahrs_hal.h`: 六轴 AHRS 的定时器接口、姿态结构和公共 API。
- Create `src/ahrs_hal.c`: 安全的 Mahony 更新、时间回绕处理和欧拉角输出。
- Create `inc/icm42688_mspm0.h`: SysConfig 符号契约、默认配置和接口对象声明。
- Create `src/icm42688_mspm0.c`: DriverLib SPI/GPIO/延时/TIMG0 适配。
- Create `temp/tests/test_icm42688.c`: 传感器初始化、解析、换算、校准测试。
- Create `temp/tests/test_ahrs.c`: 首帧、静止、时间回绕、无效输入和转动趋势测试。
- Create `temp/tests/fakes/ti_msp_dl_config.h`: MSPM0 适配层的主机编译桩。
- Create `temp/tests/test_mspm0_adapter.c`: 验证 SPI 事务、片选和时间读取。
- Create `temp/tests/run_tests.ps1`: 使用 GCC 构建并运行三个主机测试程序。
- Create `README.md`: 硬件、SysConfig、EIDE、API、FreeRTOS 和上板验证说明。

---

### Task 1: Platform-Independent ICM42688 Core

**Files:**
- Create: `inc/icm42688_hal.h`
- Create: `src/icm42688_hal.c`
- Create: `temp/tests/test_icm42688.c`

**Interfaces:**
- Consumes: `icm42688_comm_t` 的 `init/read_reg/write_reg/read_regs` 回调和 `icm42688_system_t.delay_ms`。
- Produces: `icm42688_hal_init(...)`、`icm42688_init()`、`icm42688_read(...)`、`icm42688_calibrate_gyro(...)`、`icm42688_acc_transition(...)`、`icm42688_gyro_transition(...)`。

- [ ] **Step 1: Write the failing core-driver tests**

创建 `temp/tests/test_icm42688.c`，实现伪寄存器、写操作日志和延时日志，并至少覆盖以下断言：

```c
static uint8_t registers[256];
static uint8_t read_buffer[14];
static uint8_t write_reg_log[8];
static uint8_t write_value_log[8];
static size_t write_count;

static uint8_t fake_read_reg(uint8_t reg) { return registers[reg]; }
static void fake_write_reg(uint8_t reg, uint8_t value)
{
    write_reg_log[write_count] = reg;
    write_value_log[write_count++] = value;
}

static void test_init_rejects_wrong_id(void)
{
    registers[ICM42688_WHO_AM_I] = 0x00U;
    bind_default_fakes();
    assert(icm42688_init() == ICM42688_STATUS_BAD_ID);
}

static void test_big_endian_sample_and_scale(void)
{
    const uint8_t sample[] = {
        0x40, 0x00, 0xC0, 0x00, 0x20, 0x00,
        0x10, 0x00, 0xF0, 0x00, 0x08, 0x00
    };
    icm42688_data_t data;

    memcpy(read_buffer, sample, sizeof(sample));
    bind_default_fakes();
    registers[ICM42688_WHO_AM_I] = ICM42688_ID;
    assert(icm42688_init() == ICM42688_STATUS_OK);
    assert(icm42688_read(&data) == ICM42688_STATUS_OK);
    assert(fabsf(data.acc_g.x - 4.0f) < 0.001f);
    assert(fabsf(data.acc_g.y + 4.0f) < 0.001f);
    assert(fabsf(data.gyro_dps.x - 125.0f) < 0.001f);
}
```

测试配置使用 ±8 g、±1000 dps、1000 Hz，并验证初始化写入 `DEVICE_CONFIG=0x01`、`PWR_MGMT0=0x0F`、量程/ODR 配置和滤波配置。再添加空指针、未绑定接口和静止零偏校准后输出归零测试。

- [ ] **Step 2: Run the test to verify it fails**

Run from `modules/ICM42688`:

```powershell
gcc -std=c99 -Wall -Wextra -Werror -Iinc temp/tests/test_icm42688.c src/icm42688_hal.c -lm -o temp/tests/test_icm42688.exe
```

Expected: FAIL because `inc/icm42688_hal.h` and `src/icm42688_hal.c` do not exist yet.

- [ ] **Step 3: Define the public sensor API**

在 `inc/icm42688_hal.h` 中保留原寄存器和枚举值，并定义：

```c
typedef enum {
    ICM42688_STATUS_OK = 0,
    ICM42688_STATUS_NOT_READY,
    ICM42688_STATUS_BAD_ID,
    ICM42688_STATUS_INVALID_ARGUMENT
} icm42688_status_t;

typedef struct { float x, y, z; } icm42688_vector3f_t;

typedef struct {
    icm42688_axis_data_t acc_raw;
    icm42688_axis_data_t gyro_raw;
    icm42688_vector3f_t acc_g;
    icm42688_vector3f_t gyro_dps;
} icm42688_data_t;

typedef struct {
    void (*init)(void);
    uint8_t (*read_reg)(uint8_t reg);
    void (*write_reg)(uint8_t reg, uint8_t value);
    void (*read_regs)(uint8_t reg, uint8_t *data, uint8_t len);
} icm42688_comm_t;

typedef struct { void (*delay_ms)(uint16_t ms); } icm42688_system_t;

void icm42688_hal_init(const icm42688_comm_t *comm,
                       const icm42688_system_t *system,
                       const icm42688_config_t *config);
icm42688_status_t icm42688_init(void);
icm42688_status_t icm42688_read(icm42688_data_t *data);
icm42688_status_t icm42688_calibrate_gyro(uint16_t samples,
                                         uint16_t interval_ms);
float icm42688_acc_transition(int16_t raw);
float icm42688_gyro_transition(int16_t raw);
```

- [ ] **Step 4: Implement the minimal core driver**

在 `src/icm42688_hal.c` 中：

- 移除原 STM32 `main.h` 和 `myInterface.h` 依赖。
- 保存接口和配置的只读指针。
- 按原寄存器序列初始化并返回明确状态。
- 从 `ACCEL_DATA_X1` 一次连续读取 12 字节，避免加速度和陀螺仪来自不同采样时刻。
- 使用显式大端转换：

```c
static int16_t decode_be16(const uint8_t *bytes)
{
    return (int16_t)(((uint16_t)bytes[0] << 8) | (uint16_t)bytes[1]);
}
```

- 根据配置保存换算系数；将校准得到的三轴 `gyro_bias_dps` 从后续物理量中减去。
- `icm42688_calibrate_gyro` 在 `samples == 0` 时返回 `INVALID_ARGUMENT`，否则读取指定次数并求平均；README 要求校准期间保持静止。

- [ ] **Step 5: Run the core-driver tests**

```powershell
gcc -std=c99 -Wall -Wextra -Werror -Iinc temp/tests/test_icm42688.c src/icm42688_hal.c -lm -o temp/tests/test_icm42688.exe
& .\temp\tests\test_icm42688.exe
```

Expected: compiler exits 0 and program prints `test_icm42688: PASS`.

---

### Task 2: Portable Six-Axis AHRS

**Files:**
- Create: `inc/ahrs_hal.h`
- Create: `src/ahrs_hal.c`
- Create: `temp/tests/test_ahrs.c`

**Interfaces:**
- Consumes: `ahrs_timer_t.get_time_us/init/start` and gyroscope values in radians per second.
- Produces: `ahrs_hal_init(...)`、`ahrs_init()`、`ahrs_update_6axis(...)`、`ahrs_get_attitude(...)`、`ahrs_reset_parameters()`。

- [ ] **Step 1: Write failing AHRS tests**

创建 `temp/tests/test_ahrs.c`，用可控制的 `fake_time_us` 测试：

```c
static uint32_t fake_time_us;
static uint32_t get_time_us(void) { return fake_time_us; }

static void test_level_sensor_stays_level(void)
{
    ahrs_euler_angle_t angle;
    bind_fake_timer();
    ahrs_init();
    fake_time_us = 1000U;
    ahrs_update_6axis(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
    fake_time_us = 11000U;
    ahrs_update_6axis(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
    ahrs_get_attitude(&angle);
    assert(fabsf(angle.roll) < 0.05f);
    assert(fabsf(angle.pitch) < 0.05f);
}

static void test_unsigned_timestamp_wrap_is_one_tick(void)
{
    assert(ahrs_elapsed_us(UINT32_MAX, 0U) == 1U);
}
```

再测试：首帧只建立时间基准、零加速度不产生 NaN、`dt == 0` 和大于 100 ms 的异常间隔被忽略、持续正 X 轴角速度使 roll 朝正方向变化、空输出指针安全返回。

- [ ] **Step 2: Run the test to verify it fails**

```powershell
gcc -std=c99 -Wall -Wextra -Werror -Iinc temp/tests/test_ahrs.c src/ahrs_hal.c -lm -o temp/tests/test_ahrs.exe
```

Expected: FAIL because AHRS files and `ahrs_elapsed_us` do not exist.

- [ ] **Step 3: Define the AHRS API**

在 `inc/ahrs_hal.h` 定义：

```c
typedef struct { float yaw, pitch, roll; } ahrs_euler_angle_t;

typedef struct {
    uint32_t (*get_time_us)(void);
    void (*init)(void);
    void (*start)(void);
} ahrs_timer_t;

void ahrs_hal_init(const ahrs_timer_t *timer);
void ahrs_init(void);
void ahrs_update_6axis(float gx_rad_s, float gy_rad_s, float gz_rad_s,
                       float ax_g, float ay_g, float az_g);
void ahrs_get_attitude(ahrs_euler_angle_t *angle);
void ahrs_reset_parameters(void);
uint32_t ahrs_elapsed_us(uint32_t previous, uint32_t current);
```

不保留未实际使用的磁力计参数和 `ahrs_system_t`，避免将六轴接口误解为九轴接口。姿态安装偏移由上层在持续更新 AHRS 时计算，不复制原先“只延时但不更新姿态”的无效校准函数。

- [ ] **Step 4: Port and harden the Mahony implementation**

在 `src/ahrs_hal.c` 保留原 KP/KI、死区和积分限幅参数，同时完成：

- 移除 `main.h`、`myInterface.h` 和 `icm42688_hal.h` 依赖。
- 将 `invSqrt` 中的 `long *` 类型穿透改为 C99 `memcpy` 和 `uint32_t`：

```c
static float inv_sqrt(float value)
{
    float result;
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    bits = 0x5F3759DFU - (bits >> 1U);
    memcpy(&result, &bits, sizeof(result));
    return result * (1.5f - (0.5f * value * result * result));
}
```

- 用模 `2^32` 无符号减法处理计数回绕：

```c
uint32_t ahrs_elapsed_us(uint32_t previous, uint32_t current)
{
    return current - previous;
}
```

- 先更新时间戳，再检查 `elapsed_us`；只接受 `1..100000 us`。
- 加速度模长为零时跳过重力反馈，但允许有效 `dt` 下的纯陀螺仪积分。
- 四元数归一化前检查平方和为正值。
- 将 `asinf` 输入限制到 `[-1, 1]`，避免浮点舍入产生 NaN。

- [ ] **Step 5: Run the AHRS tests**

```powershell
gcc -std=c99 -Wall -Wextra -Werror -Iinc temp/tests/test_ahrs.c src/ahrs_hal.c -lm -o temp/tests/test_ahrs.exe
& .\temp\tests\test_ahrs.exe
```

Expected: compiler exits 0 and program prints `test_ahrs: PASS`.

---

### Task 3: MSPM0G3507 DriverLib Adapter

**Files:**
- Create: `inc/icm42688_mspm0.h`
- Create: `src/icm42688_mspm0.c`
- Create: `temp/tests/fakes/ti_msp_dl_config.h`
- Create: `temp/tests/test_mspm0_adapter.c`

**Interfaces:**
- Consumes: SysConfig symbols `SPI1_INST`、`ICM42688_PORT`、`ICM42688_CS_PIN`、`ICM42688_TIMER_INST`、`CPUCLK_FREQ` and DriverLib functions declared by `ti_msp_dl_config.h`.
- Produces: `icm42688_mspm0_comm`、`icm42688_mspm0_system`、`icm42688_mspm0_timer` and `icm42688_mspm0_bind(...)`.

- [ ] **Step 1: Write the failing adapter test and DriverLib fake**

`temp/tests/fakes/ti_msp_dl_config.h` 定义主机可编译的假实例、GPIO/SPI/TIMG 函数声明和 `delay_cycles`。`test_mspm0_adapter.c` 记录每次发送、接收和 CS 电平变化，并验证：

```c
static void test_single_register_read_transaction(void)
{
    reset_fake_driverlib();
    fake_rx_byte = 0x47U;
    assert(icm42688_mspm0_comm.read_reg(0x75U) == 0x47U);
    assert(cs_events[0] == CS_LOW);
    assert(tx_bytes[0] == 0xF5U);
    assert(cs_events[1] == CS_HIGH);
}

static void test_timer_returns_sysconfig_counter(void)
{
    fake_timer_count = 123456U;
    assert(icm42688_mspm0_timer.get_time_us() == 123456U);
}
```

连续读取还要验证只产生一对 CS 事件，写寄存器验证地址 bit 7 被清除，初始化验证 CS 被拉高。

- [ ] **Step 2: Run the adapter test to verify it fails**

```powershell
gcc -std=c99 -Wall -Wextra -Werror -Itemp/tests/fakes -Iinc temp/tests/test_mspm0_adapter.c src/icm42688_mspm0.c -o temp/tests/test_mspm0_adapter.exe
```

Expected: FAIL because adapter files do not exist.

- [ ] **Step 3: Define the SysConfig contract and exported bindings**

`inc/icm42688_mspm0.h` 包含核心和 AHRS 头文件，并声明：

```c
extern const icm42688_comm_t icm42688_mspm0_comm;
extern const icm42688_system_t icm42688_mspm0_system;
extern const ahrs_timer_t icm42688_mspm0_timer;

void icm42688_mspm0_bind(const icm42688_config_t *config);
```

`icm42688_mspm0_bind` 只绑定接口，不调用 `SYSCFG_DL_init()`；控制器必须先完成系统初始化。

- [ ] **Step 4: Implement blocking SPI, GPIO, delay, and timer bindings**

`src/icm42688_mspm0.c` 使用以下事务模式：

```c
static uint8_t spi_exchange(uint8_t value)
{
    DL_SPI_transmitDataBlocking8(SPI1_INST, value);
    return DL_SPI_receiveDataBlocking8(SPI1_INST);
}

static uint8_t read_reg(uint8_t reg)
{
    uint8_t value;
    DL_GPIO_clearPins(ICM42688_PORT, ICM42688_CS_PIN);
    (void)spi_exchange((uint8_t)(reg | 0x80U));
    value = spi_exchange(0xFFU);
    DL_GPIO_setPins(ICM42688_PORT, ICM42688_CS_PIN);
    return value;
}
```

连续读取在一次 CS 低电平窗口中循环 `spi_exchange(0xFFU)`。毫秒延时分块调用 `delay_cycles(CPUCLK_FREQ / 1000U)`，避免 `ms * cycles` 乘法溢出。TIMG0 已由 SysConfig 配置和启动，因此 timer `init/start` 回调为空操作，`get_time_us` 返回 `DL_TimerG_getTimerCount(ICM42688_TIMER_INST)`。

- [ ] **Step 5: Run the adapter test**

```powershell
gcc -std=c99 -Wall -Wextra -Werror -Itemp/tests/fakes -Iinc temp/tests/test_mspm0_adapter.c src/icm42688_mspm0.c -o temp/tests/test_mspm0_adapter.exe
& .\temp\tests\test_mspm0_adapter.exe
```

Expected: compiler exits 0 and program prints `test_mspm0_adapter: PASS`.

---

### Task 4: README, Test Runner, and End-to-End Verification

**Files:**
- Create: `temp/tests/run_tests.ps1`
- Create: `README.md`
- Verify only: `inc/*.h`, `src/*.c`, `temp/tests/*`

**Interfaces:**
- Consumes: all public APIs and SysConfig symbol contract from Tasks 1-3.
- Produces: repeatable host verification and complete instructions for integrating into `controller/mspm0` without modifying it in this task.

- [ ] **Step 1: Add one-command host verification**

创建 `temp/tests/run_tests.ps1`，使用 `$ErrorActionPreference = 'Stop'`，依次执行三个 GCC 命令和三个测试程序。任何编译或测试返回非零时立即退出；全部成功时输出：

```text
ICM42688 host tests: PASS
```

- [ ] **Step 2: Write the final README**

`README.md` 必须明确包含以下可操作内容：

- 引脚表：PA16 POCI、PA17 SCLK、PA18 PICO、PB20 CS。
- SPI1 SysConfig：Controller、MOTO3 POL0/PHA0、8-bit、MSB、1 MHz、软件 CS。
- GPIO：PB20 推挽输出，初始化为高。
- TIMG0：建议 SysConfig 实例名 `ICM42688_TIMER`，BUSCLK 32 MHz、prescale 31、1 MHz、32 位 periodic/free-running、最大周期、上电启动、无需中断。
- 解释该名称必须生成 `ICM42688_TIMER_INST`；如果工程使用不同实例名，需要在构建设置或适配头中映射。
- EIDE 增加 `../../modules/ICM42688/src` 到 `srcDirs`，增加 `../../modules/ICM42688/inc` 到头文件路径；只说明，不修改 `.eide/eide.yml`。
- 在 `SYSCFG_DL_init()` 之后执行绑定、传感器初始化、校准和 AHRS 初始化。
- 示例主循环将 `gyro_dps * (PI / 180.0f)` 转为 rad/s，再调用 `ahrs_update_6axis`。
- FreeRTOS 部署后，周期任务可使用 `vTaskDelayUntil` 调度，但 AHRS 的实际 `dt` 仍取 TIMG0；禁止改用毫秒分辨率的 `xTaskGetTickCount()`。
- 说明六轴 yaw 会漂移，不代表绝对航向。
- 上板清单：WHO_AM_I 0x47、静止加速度模长约 1 g、校准后陀螺仪接近 0 dps、roll/pitch 方向符合安装坐标。
- 故障排查：全 `0x00`、全 `0xFF`、ID 错误、姿态 NaN、姿态方向相反和 FreeRTOS 运行后计时异常。

示例初始化保持单一入口：

```c
static const icm42688_config_t imu_config = {
    .interface_type = ICM42688_INTERFACE_SPI,
    .acc_sample = ICM42688_ACC_SAMPLE_SGN_8G,
    .gyro_sample = ICM42688_GYRO_SAMPLE_SGN_1000DPS,
    .sample_rate = ICM42688_SAMPLE_RATE_1000,
};

SYSCFG_DL_init();
icm42688_mspm0_bind(&imu_config);
if (icm42688_init() != ICM42688_STATUS_OK) {
    for (;;) { __WFI(); }
}
ahrs_hal_init(&icm42688_mspm0_timer);
ahrs_init();
```

- [ ] **Step 3: Run all host tests from the documented command**

```powershell
powershell -ExecutionPolicy Bypass -File .\temp\tests\run_tests.ps1
```

Expected final line: `ICM42688 host tests: PASS`.

- [ ] **Step 4: Compile-check production C files with strict warnings**

Core and AHRS:

```powershell
gcc -std=c99 -Wall -Wextra -Werror -Iinc -c src/icm42688_hal.c -o temp/icm42688_hal.o
gcc -std=c99 -Wall -Wextra -Werror -Iinc -c src/ahrs_hal.c -o temp/ahrs_hal.o
```

Adapter with fake DriverLib:

```powershell
gcc -std=c99 -Wall -Wextra -Werror -Itemp/tests/fakes -Iinc -c src/icm42688_mspm0.c -o temp/icm42688_mspm0.o
```

Expected: all commands exit 0 with no warnings.

- [ ] **Step 5: Confirm scope and worktree boundaries**

```powershell
git status --short
git diff -- modules/ICM42688
```

Expected: only `modules/ICM42688` contains changes made by this task; `controller/` has no modifications. Do not revert unrelated pre-existing changes elsewhere.

## Plan Self-Review

- Spec coverage: sensor driver, AHRS, SPI1/PB20, TIMG0/FreeRTOS, README, tests and `controller/` exclusion each have a dedicated task.
- Placeholder scan: no TBD, TODO, deferred implementation or unspecified error-handling steps remain.
- Type consistency: the platform adapter exports the exact core/AHRS interface types defined in Tasks 1 and 2; README uses the same function and field names.
- Scope: DMA, FIFO, interrupt acquisition, I2C and nine-axis fusion remain explicitly excluded.
