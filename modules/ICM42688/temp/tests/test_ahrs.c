/**
 * @file  test_ahrs.c
 * @brief Host-side tests for the portable six-axis AHRS layer.
 */

#include "ahrs_hal.h"
#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

static uint32_t fake_time_us = 0U;
static int init_called = 0;
static int start_called = 0;

static uint32_t get_time_us(void) { return fake_time_us; }
static void timer_init(void) { init_called = 1; (void)init_called; }
static void timer_start(void) { start_called = 1; (void)start_called; }

static const ahrs_timer_t fake_timer = {
    get_time_us,
    timer_init,
    timer_start
};

static void bind_fake_timer(void)
{
    ahrs_hal_init(&fake_timer);
}

static int failures = 0;
static int total = 0;

#define CHECK(cond) do {                                  \
    total++;                                              \
    if (!(cond)) {                                        \
        failures++;                                       \
        printf("FAIL: %s (line %d)\n", #cond, __LINE__);  \
    }                                                     \
} while (0)

/* 1. Level sensor stays level under zero gyro and gravity along +Z. */
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
    CHECK(fabsf(angle.roll) < 0.05f);
    CHECK(fabsf(angle.pitch) < 0.05f);
}

/* 2. Unsigned timestamp wrap yields a one-tick difference. */
static void test_unsigned_timestamp_wrap_is_one_tick(void)
{
    CHECK(ahrs_elapsed_us(UINT32_MAX, 0U) == 1U);
}

/* 3. First frame only establishes the time baseline: attitude unchanged. */
static void test_first_frame_establishes_baseline(void)
{
    ahrs_euler_angle_t angle;
    bind_fake_timer();
    ahrs_init();
    fake_time_us = 5000U;
    /* Even with non-trivial gyro/accel, the first frame must not move q. */
    ahrs_update_6axis(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
    ahrs_get_attitude(&angle);
    CHECK(fabsf(angle.roll) < 1e-6f);
    CHECK(fabsf(angle.pitch) < 1e-6f);
    CHECK(fabsf(angle.yaw) < 1e-6f);
}

/* 4. Zero accel must not produce NaN; pure gyro integration still works. */
static void test_zero_accel_no_nan(void)
{
    ahrs_euler_angle_t angle;
    bind_fake_timer();
    ahrs_init();
    fake_time_us = 1000U;
    ahrs_update_6axis(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    fake_time_us = 11000U;
    ahrs_update_6axis(0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    fake_time_us = 21000U;
    ahrs_update_6axis(0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    ahrs_get_attitude(&angle);
    CHECK(angle.roll == angle.roll); /* not NaN */
    CHECK(angle.pitch == angle.pitch);
    CHECK(angle.yaw == angle.yaw);
}

/* 5. dt == 0 is dropped (no movement, no NaN). */
static void test_dt_zero_dropped(void)
{
    ahrs_euler_angle_t angle;
    bind_fake_timer();
    ahrs_init();
    fake_time_us = 1000U;
    ahrs_update_6axis(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f); /* baseline */
    /* Same timestamp => elapsed 0 => dropped */
    ahrs_update_6axis(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
    ahrs_get_attitude(&angle);
    CHECK(fabsf(angle.roll) < 1e-6f);
}

/* 6. dt > 100 ms is dropped. */
static void test_dt_too_large_dropped(void)
{
    ahrs_euler_angle_t angle;
    bind_fake_timer();
    ahrs_init();
    fake_time_us = 1000U;
    ahrs_update_6axis(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f); /* baseline */
    fake_time_us = 1000U + 200000U; /* 200 ms, out of range */
    ahrs_update_6axis(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
    ahrs_get_attitude(&angle);
    CHECK(fabsf(angle.roll) < 1e-6f);
}

/* 7. Positive X gyro makes roll increase. */
static void test_positive_x_gyro_increases_roll(void)
{
    ahrs_euler_angle_t angle;
    float before, after;
    bind_fake_timer();
    ahrs_init();
    fake_time_us = 1000U;
    ahrs_update_6axis(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f); /* baseline */
    fake_time_us = 11000U;
    ahrs_update_6axis(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f); /* level settle */
    ahrs_get_attitude(&angle);
    before = angle.roll;
    fake_time_us = 21000U;
    ahrs_update_6axis(2.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
    ahrs_get_attitude(&angle);
    after = angle.roll;
    CHECK(after > before);
}

/* 8. NULL output pointer is safe. */
static void test_null_output_pointer_safe(void)
{
    ahrs_get_attitude(NULL);
    CHECK(1); /* reached here without crashing */
}

/* 9. ahrs_reset_parameters restores identity attitude. */
static void test_reset_parameters(void)
{
    ahrs_euler_angle_t angle;
    bind_fake_timer();
    ahrs_init();
    fake_time_us = 1000U;
    ahrs_update_6axis(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f); /* baseline */
    fake_time_us = 11000U;
    ahrs_update_6axis(2.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f); /* tilt */
    ahrs_get_attitude(&angle);
    CHECK(fabsf(angle.roll) > 1e-3f);
    /* Reset clears quaternion/integral; a fresh level frame returns to ~0. */
    ahrs_reset_parameters();
    fake_time_us = 31000U;
    ahrs_update_6axis(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f); /* baseline */
    fake_time_us = 41000U;
    ahrs_update_6axis(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f); /* level */
    ahrs_get_attitude(&angle);
    CHECK(fabsf(angle.roll) < 0.05f);
    CHECK(fabsf(angle.pitch) < 0.05f);
}

int main(void)
{
    test_level_sensor_stays_level();
    test_unsigned_timestamp_wrap_is_one_tick();
    test_first_frame_establishes_baseline();
    test_zero_accel_no_nan();
    test_dt_zero_dropped();
    test_dt_too_large_dropped();
    test_positive_x_gyro_increases_roll();
    test_null_output_pointer_safe();
    test_reset_parameters();

    if (failures == 0) {
        printf("test_ahrs: PASS (%d assertions)\n", total);
        return 0;
    }
    printf("test_ahrs: FAIL (%d/%d assertions failed)\n", failures, total);
    return 1;
}
