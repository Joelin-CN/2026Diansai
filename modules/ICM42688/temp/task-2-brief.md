### Task 2: Portable Six-Axis AHRS

**Files:**
- Create: `inc/ahrs_hal.h`
- Create: `src/ahrs_hal.c`
- Create: `temp/tests/test_ahrs.c`

**Interfaces:**
- Consumes: `ahrs_timer_t.get_time_us/init/start` and gyroscope values in radians per second.
- Produces: `ahrs_hal_init(...)`, `ahrs_init()`, `ahrs_update_6axis(...)`, `ahrs_get_attitude(...)`, `ahrs_reset_parameters()`, `ahrs_elapsed_us()`.

- [ ] **Step 1: Write failing AHRS tests**

Create `temp/tests/test_ahrs.c` with controllable `fake_time_us`:

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

Also test: first frame only establishes time baseline, zero accel produces no NaN, dt==0 and >100ms dropped, positive X gyro makes roll increase, null output pointer safe.

- [ ] **Step 2: Run the test to verify it fails**

```powershell
gcc -std=c99 -Wall -Wextra -Werror -Iinc temp/tests/test_ahrs.c src/ahrs_hal.c -lm -o temp/tests/test_ahrs.exe
```
Expected: FAIL because AHRS files do not exist.

- [ ] **Step 3: Define the AHRS API**

In `inc/ahrs_hal.h`:

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

Do NOT keep unused magnetometer parameters or `ahrs_system_t`. Drop the original offset calibration function (which only delayed without updating AHRS).

- [ ] **Step 4: Port and harden the Mahony implementation**

In `src/ahrs_hal.c`, keep original KP/KI, deadband, and integral limit parameters. Also:

- Remove `main.h`, `myInterface.h`, `icm42688_hal.h` dependencies.
- Replace `invSqrt` with C99 memcpy version using uint32_t:

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

- Unsigned subtraction for wrap handling:

```c
uint32_t ahrs_elapsed_us(uint32_t previous, uint32_t current)
{
    return current - previous;
}
```

- Update timestamp BEFORE checking elapsed_us. Accept only 1..100000 us.
- Skip gravity feedback when accel magnitude is zero, but allow pure gyro integration with valid dt.
- Check quaternion sum-of-squares is positive before normalizing.
- Clamp asinf input to [-1, 1].

- [ ] **Step 5: Run the AHRS tests**

```powershell
gcc -std=c99 -Wall -Wextra -Werror -Iinc temp/tests/test_ahrs.c src/ahrs_hal.c -lm -o temp/tests/test_ahrs.exe
& .\temp\tests\test_ahrs.exe
```
Expected: compile clean, program prints `test_ahrs: PASS`.
