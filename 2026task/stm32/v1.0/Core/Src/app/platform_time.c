/**
 * @file platform_time.c
 * @brief 平台时间实现（STM32F407 DWT）
 * @date 2026-07-29
 */

#include "platform_time.h"
#include "stm32f4xx.h"

static uint32_t s_overflow_count = 0;
static uint32_t s_last_cyccnt    = 0;

void PlatformTime_Init(void) {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
    s_overflow_count = 0;
    s_last_cyccnt    = 0;
}

uint32_t PlatformTime_GetUs32(void) {
    return DWT->CYCCNT / 168U;   // 168 MHz → 微秒
}

uint64_t PlatformTime_GetUs64(void) {
    uint32_t now = DWT->CYCCNT;
    if (now < s_last_cyccnt) {
        s_overflow_count++;
    }
    s_last_cyccnt = now;
    uint64_t cycles = ((uint64_t)s_overflow_count << 32) | (uint64_t)now;
    return cycles / 168ULL;
}
