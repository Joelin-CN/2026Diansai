/**
 * @file      test_platform_time.c
 * @brief     Host tests for platform_time pure conversion functions
 * @date      2026-07-18
 */

#include <stdio.h>
#include <stdint.h>
#include <assert.h>

#include "ti_msp_dl_config.h"
#include "../inc/platform_time.h"

static uint32_t fake_down_count = UINT32_MAX;
static uint32_t fake_primask = 0U;
static unsigned timer_start_calls = 0U;
static unsigned disable_irq_calls = 0U;
static unsigned enable_irq_calls = 0U;

void DL_TimerG_startCounter(void *timer)
{
    assert(timer == ICM42688_TIMER_INST);
    ++timer_start_calls;
}

uint32_t DL_TimerG_getTimerCount(void *timer)
{
    assert(timer == ICM42688_TIMER_INST);
    return fake_down_count;
}

uint32_t __get_PRIMASK(void)
{
    return fake_primask;
}

void __disable_irq(void)
{
    fake_primask = 1U;
    ++disable_irq_calls;
}

void __enable_irq(void)
{
    fake_primask = 0U;
    ++enable_irq_calls;
}

int main(void)
{
    printf("Testing PlatformTime_UpCountFromDownCount...\n");
    
    /* Down-counter at max (reload value) -> up-count at 0 */
    assert(PlatformTime_UpCountFromDownCount(UINT32_MAX) == 0U);
    
    /* Down-counter decreased by 25 -> up-count at 25 */
    assert(PlatformTime_UpCountFromDownCount(UINT32_MAX - 25U) == 25U);
    
    /* Down-counter at 0 -> up-count at max */
    assert(PlatformTime_UpCountFromDownCount(0U) == UINT32_MAX);

    printf("Testing PlatformTime_Extend32 wrap detection...\n");

    PlatformTime_Init();

    /* First call: high word = 0, returns input as 64-bit */
    assert(PlatformTime_Extend32(0xFFFFFFF0U) == UINT64_C(0xFFFFFFF0));
    
    /* Wrap detected (0x10 < 0xFFFFFFF0): high word increments to 1 */
    assert(PlatformTime_Extend32(0x00000010U) == UINT64_C(0x100000010));
    
    /* No wrap (0x20 > 0x10): high word stays at 1 */
    assert(PlatformTime_Extend32(0x00000020U) == UINT64_C(0x100000020));

    PlatformTime_Init();

    /* Another wrap simulation */
    assert(PlatformTime_Extend32(0xFFFFFFFEU) == UINT64_C(0xFFFFFFFE));
    assert(PlatformTime_Extend32(0x00000005U) == UINT64_C(0x100000005));

    printf("Testing platform timer and interrupt integration...\n");

    timer_start_calls = 0U;
    PlatformTime_Init();
    assert(timer_start_calls == 1U);

    fake_down_count = UINT32_MAX - 25U;
    assert(PlatformTime_GetUs32() == 25U);

    fake_down_count = UINT32_MAX - 20000U;
    assert(PlatformTime_GetUs64() == UINT64_C(20000));
    assert(disable_irq_calls == 1U);
    assert(enable_irq_calls == 1U);

    fake_primask = 1U;
    fake_down_count = UINT32_MAX - 40000U;
    assert(PlatformTime_GetUs64() == UINT64_C(40000));
    assert(fake_primask == 1U);
    assert(enable_irq_calls == 1U);

    printf("All platform_time tests PASSED\n");
    return 0;
}
