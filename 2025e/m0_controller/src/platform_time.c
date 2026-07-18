/**
 * @file      platform_time.c
 * @brief     Monotonic microsecond timebase implementation
 * @date      2026-07-18
 */

#include "platform_time.h"

/* Wrap extension state */
static uint32_t previous32 = 0U;
static uint64_t high_word  = 0U;

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
 * Target API (hardware-dependent, stubbed until Task 7)
 * ====================================================================== */

void PlatformTime_Init(void)
{
    /* Reset extension state */
    previous32 = 0U;
    high_word  = 0U;
    
    /* Actual timer configuration will be done in Task 7 via SysConfig */
}

uint32_t PlatformTime_GetUs32(void)
{
    /* TODO: Task 7 will configure ICM42688_TIMER_INST via SysConfig
     * For now, this is a stub that would read the hardware timer:
     * 
     * uint32_t down_count = DL_TimerG_getTimerCount(ICM42688_TIMER_INST);
     * return PlatformTime_UpCountFromDownCount(down_count);
     */
    return 0U;  /* Stub */
}

uint64_t PlatformTime_GetUs64(void)
{
    uint32_t now32;
    uint64_t result;
    
    /* TODO: Task 7 will enable this code path
     * For now, stub returns 0
     */
    
    /* Critical section to protect wrap detection state */
    /* __disable_irq(); */
    
    now32 = PlatformTime_GetUs32();
    result = PlatformTime_Extend32(now32);
    
    /* __enable_irq(); */
    
    return result;
}
