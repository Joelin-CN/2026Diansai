/**
 * @file      platform_time.h
 * @brief     Monotonic microsecond timebase for MSPM0G3507
 * @date      2026-07-18
 * @note      Wraps a hardware down-counter into monotonic 32-bit and 64-bit
 *            microsecond timestamps. Pure conversion functions are testable
 *            without hardware; target wrappers use DriverLib.
 */

#ifndef PLATFORM_TIME_H
#define PLATFORM_TIME_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ======================================================================
 * Pure testable conversion functions (no hardware dependencies)
 * ====================================================================== */

/**
 * @brief  Convert down-counter value to up-counter value
 * @param  down_count  Hardware down-counter reading
 * @return Equivalent up-counter value (0 at timer reload, increases as timer counts down)
 * @note   Pure function: UINT32_MAX - down_count
 */
uint32_t PlatformTime_UpCountFromDownCount(uint32_t down_count);

/**
 * @brief  Extend 32-bit timestamp to 64-bit with wrap detection
 * @param  now32  Current 32-bit timestamp
 * @return 64-bit extended timestamp
 * @note   Detects wraps by comparing with previous sample. Call sequentially
 *         from a single context (or protect with critical section in multi-threaded use).
 */
uint64_t PlatformTime_Extend32(uint32_t now32);

/* ======================================================================
 * Target API (uses hardware timer)
 * ====================================================================== */

/**
 * @brief  Initialize platform timebase
 * @note   SysConfig configures TIMG12 as a stopped 1 MHz down-counter.
 *         This function starts the timer and establishes a new software epoch.
 */
void PlatformTime_Init(void);

/**
 * @brief  Get current monotonic timestamp (32-bit, wraps ~71 minutes)
 * @return Microseconds since PlatformTime_Init()
 */
uint32_t PlatformTime_GetUs32(void);

/**
 * @brief  Get current monotonic timestamp (64-bit, no practical wrap)
 * @return Microseconds since PlatformTime_Init()
 */
uint64_t PlatformTime_GetUs64(void);

#ifdef __cplusplus
}
#endif

#endif /* PLATFORM_TIME_H */
