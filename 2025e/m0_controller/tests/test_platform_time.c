/**
 * @file      test_platform_time.c
 * @brief     Host tests for platform_time pure conversion functions
 * @date      2026-07-18
 */

#include <stdio.h>
#include <stdint.h>
#include <assert.h>

/* Include the header with pure testable functions */
#include "../inc/platform_time.h"

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
    
    /* First call: high word = 0, returns input as 64-bit */
    assert(PlatformTime_Extend32(0xFFFFFFF0U) == UINT64_C(0xFFFFFFF0));
    
    /* Wrap detected (0x10 < 0xFFFFFFF0): high word increments to 1 */
    assert(PlatformTime_Extend32(0x00000010U) == UINT64_C(0x100000010));
    
    /* No wrap (0x20 > 0x10): high word stays at 1 */
    assert(PlatformTime_Extend32(0x00000020U) == UINT64_C(0x100000020));
    
    /* Another wrap simulation */
    assert(PlatformTime_Extend32(0xFFFFFFFEU) == UINT64_C(0x1FFFFFFFE));
    assert(PlatformTime_Extend32(0x00000005U) == UINT64_C(0x200000005));

    printf("All platform_time tests PASSED\n");
    return 0;
}
