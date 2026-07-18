### Task 1: Platform-Independent ICM42688 Core

**Files:**
- Create: `inc/icm42688_hal.h`
- Create: `src/icm42688_hal.c`
- Create: `temp/tests/test_icm42688.c`

**Interfaces:**
- Consumes: `icm42688_comm_t` callbacks `init/read_reg/write_reg/read_regs` and `icm42688_system_t.delay_ms`.
- Produces: `icm42688_hal_init(...)`, `icm42688_init()`, `icm42688_read(...)`, `icm42688_calibrate_gyro(...)`, `icm42688_acc_transition(...)`, `icm42688_gyro_transition(...)`.

- [ ] **Step 1: Write the failing core-driver tests**

Create `temp/tests/test_icm42688.c` with fake registers, write log, and delay log. Cover:

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

Test config: +-8 g, +-1000 dps, 1000 Hz. Also verify DEVICE_CONFIG=0x01, PWR_MGMT0=0x0F written. Add null-pointer, unbound interface, and calibration-zero tests.

- [ ] **Step 2: Run the test to verify it fails**

Run from `modules/ICM42688`:
```powershell
gcc -std=c99 -Wall -Wextra -Werror -Iinc temp/tests/test_icm42688.c src/icm42688_hal.c -lm -o temp/tests/test_icm42688.exe
```
Expected: FAIL because header and source do not exist yet.

- [ ] **Step 3: Define the public sensor API**

In `inc/icm42688_hal.h`, keep original registers and enums, add:

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

In `src/icm42688_hal.c`:
- Remove STM32 `main.h` and `myInterface.h` dependencies.
- Store interface and config pointers as const.
- Follow original register init sequence; return explicit status.
- Read 12 bytes from ACCEL_DATA_X1 in one burst to avoid acc/gyro time skew.
- Explicit big-endian decode:

```c
static int16_t decode_be16(const uint8_t *bytes)
{
    return (int16_t)(((uint16_t)bytes[0] << 8) | (uint16_t)bytes[1]);
}
```

- Store scale factors per config. Subtract calibrated `gyro_bias_dps` from physical output.
- `icm42688_calibrate_gyro` returns INVALID_ARGUMENT for samples==0, else averages N reads with delay.

- [ ] **Step 5: Run the core-driver tests**

```powershell
gcc -std=c99 -Wall -Wextra -Werror -Iinc temp/tests/test_icm42688.c src/icm42688_hal.c -lm -o temp/tests/test_icm42688.exe
& .\temp\tests\test_icm42688.exe
```
Expected: compile clean, program prints `test_icm42688: PASS`.
