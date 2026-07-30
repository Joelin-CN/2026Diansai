/**
 * @file      icm42688_mspm0.h
 * @brief     MSPM0G3507 DriverLib adapter for the portable ICM42688 / AHRS HAL
 * @author    Port task 3
 * @date      2026-07-17
 *
 * @note      This adapter binds the platform-independent core (icm42688_hal /
 *            ahrs_hal) to TI MSPM0 DriverLib using SysConfig-generated symbols.
 *            The controller MUST call SYSCFG_DL_init() (system init) before
 *            icm42688_mspm0_bind(). SysConfig must provide:
 *              - SPI peripheral named SPI1            -> SPI1_INST
 *              - GPIO output for ICM42688 CS          -> ICM42688_PORT, ICM42688_CS_PIN
 *            and CPUCLK_FREQ (default 32000000). The controller owns TIMG12
 *            through PlatformTime and starts it with PlatformTime_Init().
 */

#ifndef _ICM42688_MSPM0_H_
#define _ICM42688_MSPM0_H_

#include "icm42688_hal.h"
#include "ahrs_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------------
 * Exported interface instances
 * ---------------------------------------------------------------------- */

/** SPI + GPIO backed communication interface (init/read_reg/write_reg/read_regs). */
extern const icm42688_comm_t icm42688_mspm0_comm;

/** delay_ms backed by delay_cycles() loop. */
extern const icm42688_system_t icm42688_mspm0_system;

/** Controller PlatformTime-backed microsecond timer; the controller owns startup. */
extern const ahrs_timer_t icm42688_mspm0_timer;

/* ----------------------------------------------------------------------
 * Binding
 * ---------------------------------------------------------------------- */

/**
 * @brief Bind the MSPM0 DriverLib adapter to the platform-independent core.
 * @param config  Sensor configuration (acc/gyro range, sample rate). Must not
 *                be NULL; icm42688_init() will return ICM42688_STATUS_NOT_READY
 *                if config is NULL (the core has no platform-default path).
 * @note  Does NOT call SYSCFG_DL_init(); the controller must perform system
 *        initialization first. Only binds interface pointers.
 */
void icm42688_mspm0_bind(const icm42688_config_t *config);

#ifdef __cplusplus
}
#endif

#endif /* _ICM42688_MSPM0_H_ */
