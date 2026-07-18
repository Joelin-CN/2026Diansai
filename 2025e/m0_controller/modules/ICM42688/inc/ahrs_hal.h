/**
 * @file      ahrs_hal.h
 * @brief     Portable six-axis Mahony AHRS hardware abstraction layer
 * @note      Platform-independent. No STM32/MSPM0 dependencies.
 */

#ifndef _AHRS_HAL_H_
#define _AHRS_HAL_H_

#include <stdint.h>
#include <math.h>
#include <stddef.h>

#define AHRS_PI (float)3.1415926535

/**
 * @brief  Euler angle data (degrees)
 */
typedef struct
{
    float yaw;   /**< Yaw   (rotation about z) */
    float pitch; /**< Pitch (rotation about y) */
    float roll;  /**< Roll  (rotation about x) */
} ahrs_euler_angle_t;

/**
 * @brief  Timer interface supplied by the platform
 */
typedef struct
{
    uint32_t (*get_time_us)(void); /**< Current microsecond timestamp */
    void (*init)(void);            /**< Initialize the timer */
    void (*start)(void);           /**< Start the timer */
} ahrs_timer_t;

/**
 * @brief      Bind the platform timer to the AHRS layer
 * @param      timer  Timer interface (may be NULL to unbind)
 */
void ahrs_hal_init(const ahrs_timer_t *timer);

/**
 * @brief Initialize the AHRS: identity quaternion, reference vectors, baseline timestamp
 */
void ahrs_init(void);

/**
 * @brief      Update the attitude estimate (Mahony, six-axis)
 * @param      gx_rad_s  Gyro X rate (rad/s)
 * @param      gy_rad_s  Gyro Y rate (rad/s)
 * @param      gz_rad_s  Gyro Z rate (rad/s)
 * @param      ax_g      Accel X (g, may be 0)
 * @param      ay_g      Accel Y (g, may be 0)
 * @param      az_g      Accel Z (g, may be 0)
 */
void ahrs_update_6axis(float gx_rad_s, float gy_rad_s, float gz_rad_s,
                       float ax_g, float ay_g, float az_g);

/**
 * @brief      Get the current Euler angles (degrees)
 * @param      angle  Output pointer (NULL is safe, no-op)
 */
void ahrs_get_attitude(ahrs_euler_angle_t *angle);

/**
 * @brief Reset quaternion to identity and clear integral error terms
 */
void ahrs_reset_parameters(void);

/**
 * @brief      Unsigned microsecond difference with automatic wrap
 * @param      previous  Earlier timestamp
 * @param      current   Later timestamp
 * @return     current - previous (wraps on uint32_t overflow)
 */
uint32_t ahrs_elapsed_us(uint32_t previous, uint32_t current);

#endif /* _AHRS_HAL_H_ */
