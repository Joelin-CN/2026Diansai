/**
 * @file      ahrs_hal.c
 * @brief     Portable six-axis Mahony AHRS implementation
 * @note      Ported from the original STM32 ICM42688 driver with hardening:
 *            - C99 memcpy invSqrt (no type-punning)
 *            - unsigned timestamp wrap handling
 *            - timestamp updated before elapsed check; 1..100000 us accepted
 *            - zero accel magnitude skips gravity feedback, gyro still integrates
 *            - quaternion sum-of-squares checked before normalizing
 *            - asinf input clamped to [-1, 1]
 */

#include "ahrs_hal.h"
#include <string.h>

/* PI gains per axis (kept from original driver) */
#define KP_ROLL     3.2f
#define KP_PITCH    2.95f
#define KP_YAW      1.0f

#define KI_ROLL     0.095f
#define KI_PITCH    0.085f
#define KI_YAW      0.065f

/* Deadbands */
#define GYRO_DEADBAND     0.01f
#define ERROR_DEADBAND    0.0050f

/* Integral limits */
#define INT_LIMIT        0.1f
#define INT_LIMIT_FACTOR 0.5f

#define US_TO_S           1000000.0f
#define HALF_PERIOD_SCALE 2.0f

/* Accepted dt window (microseconds) */
#define DT_MIN_US 1U
#define DT_MAX_US 100000U

/* Quaternion state */
static float q0 = 1.0f, q1 = 0.0f, q2 = 0.0f, q3 = 0.0f;

/* Integral error terms */
static float exInt = 0.0f, eyInt = 0.0f, ezInt = 0.0f;

/* Output attitude */
static ahrs_euler_angle_t attitude = {0.0f, 0.0f, 0.0f};

/* Platform timer (NULL until bound) */
static const ahrs_timer_t *p_timer = NULL;

/* Last sample timestamp; 0 means "baseline not yet established" */
static uint32_t lastTime = 0U;

/**
 * @brief  Fast inverse square root using C99 memcpy (no aliasing)
 */
static float inv_sqrt(float value)
{
    float result;
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    bits = 0x5F3759DFU - (bits >> 1U);
    memcpy(&result, &bits, sizeof(result));
    return result * (1.5f - (0.5f * value * result * result));
}

/**
 * @brief  Clamp the integral term with a dynamic limit based on gyro magnitude
 */
static float limit_integral(float value, float limit, float gx, float gy, float gz)
{
    float dynamic_limit = limit + fabsf(gx + gy + gz) * INT_LIMIT_FACTOR;
    return fminf(fmaxf(value, -dynamic_limit), dynamic_limit);
}

/**
 * @brief  Apply a symmetric deadband
 */
static float apply_deadband(float value, float deadband)
{
    if (fabsf(value) < deadband) {
        return 0.0f;
    }
    return value;
}

void ahrs_hal_init(const ahrs_timer_t *timer)
{
    p_timer = timer;
}

uint32_t ahrs_elapsed_us(uint32_t previous, uint32_t current)
{
    return current - previous;
}

void ahrs_reset_parameters(void)
{
    q0 = 1.0f;
    q1 = 0.0f;
    q2 = 0.0f;
    q3 = 0.0f;

    exInt = 0.0f;
    eyInt = 0.0f;
    ezInt = 0.0f;
}

void ahrs_init(void)
{
    if (p_timer == NULL) {
        return;
    }

    if (p_timer->init != NULL) {
        p_timer->init();
    }
    if (p_timer->start != NULL) {
        p_timer->start();
    }

    ahrs_reset_parameters();

    attitude.yaw = 0.0f;
    attitude.pitch = 0.0f;
    attitude.roll = 0.0f;

    /* Mark baseline as not yet established */
    lastTime = 0U;
}

void ahrs_update_6axis(float gx, float gy, float gz,
                       float ax, float ay, float az)
{
    float norm;
    float vx, vy, vz;
    float ex, ey, ez, halfT;
    float tempq0, tempq1, tempq2, tempq3;
    uint32_t currentTime;
    uint32_t previous;
    uint32_t elapsed;
    float accel_mag_sq;
    float sum_sq;
    float sin_val;

    if (p_timer == NULL) {
        return;
    }

    /* Gyro deadband */
    gx = apply_deadband(gx, GYRO_DEADBAND);
    gy = apply_deadband(gy, GYRO_DEADBAND);
    gz = apply_deadband(gz, GYRO_DEADBAND);

    /* Quaternion products */
    float q0q0 = q0 * q0;
    float q0q1 = q0 * q1;
    float q0q2 = q0 * q2;
    float q1q1 = q1 * q1;
    float q1q3 = q1 * q3;
    float q2q2 = q2 * q2;
    float q2q3 = q2 * q3;
    float q3q3 = q3 * q3;

    /* Sample timestamp */
    currentTime = p_timer->get_time_us();
    previous = lastTime;
    /* Update timestamp BEFORE validating elapsed (keeps wrap-safe baseline) */
    lastTime = currentTime;

    /* First frame only establishes the time baseline */
    if (previous == 0U) {
        return;
    }

    elapsed = ahrs_elapsed_us(previous, currentTime);
    if (elapsed < DT_MIN_US || elapsed > DT_MAX_US) {
        return;
    }

    halfT = (float)elapsed / (US_TO_S * HALF_PERIOD_SCALE);

    /* Gravity feedback only when accel magnitude is non-zero */
    accel_mag_sq = ax * ax + ay * ay + az * az;
    if (accel_mag_sq > 0.0f) {
        norm = inv_sqrt(accel_mag_sq);
        ax = ax * norm;
        ay = ay * norm;
        az = az * norm;

        /* Estimated gravity direction from current quaternion */
        vx = 2.0f * (q1q3 - q0q2);
        vy = 2.0f * (q0q1 + q2q3);
        vz = q0q0 - q1q1 - q2q2 + q3q3;

        /* Cross product of measured and estimated gravity = error */
        ex = (ay * vz - az * vy);
        ey = (az * vx - ax * vz);
        ez = (ax * vy - ay * vx);

        ex = apply_deadband(ex, ERROR_DEADBAND);
        ey = apply_deadband(ey, ERROR_DEADBAND);
        ez = apply_deadband(ez, ERROR_DEADBAND);

        if (ex != 0.0f) {
            exInt = exInt + ex * KI_ROLL * halfT;
            exInt = limit_integral(exInt, INT_LIMIT, gx, gy, gz);
            gx = gx + KP_ROLL * ex + exInt;
        }
        if (ey != 0.0f) {
            eyInt = eyInt + ey * KI_PITCH * halfT;
            eyInt = limit_integral(eyInt, INT_LIMIT, gx, gy, gz);
            gy = gy + KP_PITCH * ey + eyInt;
        }
        if (ez != 0.0f) {
            ezInt = ezInt + ez * KI_YAW * halfT;
            ezInt = limit_integral(ezInt, INT_LIMIT, gx, gy, gz);
            gz = gz + KP_YAW * ez + ezInt;
        }
    }

    /* Quaternion integration (pure gyro when accel is zero) */
    tempq0 = q0 + (-q1 * gx - q2 * gy - q3 * gz) * halfT;
    tempq1 = q1 + ( q0 * gx + q2 * gz - q3 * gy) * halfT;
    tempq2 = q2 + ( q0 * gy - q1 * gz + q3 * gx) * halfT;
    tempq3 = q3 + ( q0 * gz + q1 * gy - q2 * gx) * halfT;

    /* Normalize only when the sum of squares is positive */
    sum_sq = tempq0 * tempq0 + tempq1 * tempq1 + tempq2 * tempq2 + tempq3 * tempq3;
    if (sum_sq > 0.0f) {
        norm = inv_sqrt(sum_sq);
        q0 = tempq0 * norm;
        q1 = tempq1 * norm;
        q2 = tempq2 * norm;
        q3 = tempq3 * norm;
    }

    /* Euler angles (degrees) */
    attitude.yaw = -atan2f(2.0f * q1 * q2 + 2.0f * q0 * q3,
                           -2.0f * q2 * q2 - 2.0f * q3 * q3 + 1.0f) * 180.0f / AHRS_PI;

    sin_val = -2.0f * q1 * q3 + 2.0f * q0 * q2;
    if (sin_val > 1.0f) {
        sin_val = 1.0f;
    } else if (sin_val < -1.0f) {
        sin_val = -1.0f;
    }
    attitude.pitch = -asinf(sin_val) * 180.0f / AHRS_PI;

    attitude.roll = atan2f(2.0f * q2 * q3 + 2.0f * q0 * q1,
                           -2.0f * q1 * q1 - 2.0f * q2 * q2 + 1.0f) * 180.0f / AHRS_PI;

    if (attitude.yaw < 0.0f) {
        attitude.yaw += 360.0f;
    }
}

void ahrs_get_attitude(ahrs_euler_angle_t *angle)
{
    if (angle == NULL) {
        return;
    }
    angle->roll = attitude.roll;
    angle->pitch = attitude.pitch;
    angle->yaw = attitude.yaw;
}
