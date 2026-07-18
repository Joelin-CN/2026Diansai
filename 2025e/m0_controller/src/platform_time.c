/**
 * @file      platform_time.c
 * @brief     Monotonic microsecond timebase implementation
 * @date      2026-07-18
 */

#include "platform_time.h"
#include "ti_msp_dl_config.h"

/* Wrap extension state */
static uint32_t previous32 = 0U;
static uint64_t high_word  = 0U;
static uint32_t origin32   = 0U;

/* ======================================================================
 * Pure testable functions (no hardware access)
 * ====================================================================== */

uint32_t PlatformTime_UpCountFromDownCount(uint32_t down_count)
{
    return UINT32_MAX - down_count;
}

uint64_t PlatformTime_Extend32(uint32_t now32)
{
    /* Detect wrap: current value less than previous means counter wrapped */
    if (now32 < previous32) {
        high_word += UINT64_C(1) << 32;
    }
    
    previous32 = now32;
    
    return high_word | (uint64_t)now32;
}

/* ======================================================================
 * Target API (hardware-dependent)
 * ====================================================================== */

void PlatformTime_Init(void)
{
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    DL_TimerG_startCounter(ICM42688_TIMER_INST);
    origin32 = PlatformTime_UpCountFromDownCount(
        DL_TimerG_getTimerCount(ICM42688_TIMER_INST));
    previous32 = 0U;
    high_word  = 0U;
    if (primask == 0U) {
        __enable_irq();
    }
}

uint32_t PlatformTime_GetUs32(void)
{
    uint32_t now32 = PlatformTime_UpCountFromDownCount(
        DL_TimerG_getTimerCount(ICM42688_TIMER_INST));

    return now32 - origin32;
}

uint64_t PlatformTime_GetUs64(void)
{
    uint32_t primask = __get_PRIMASK();
    uint64_t result;

    __disable_irq();
    result = PlatformTime_Extend32(PlatformTime_GetUs32());
    if (primask == 0U) {
        __enable_irq();
    }

    return result;
}
