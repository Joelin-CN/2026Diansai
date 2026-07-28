/**
 * @file encoder_hw_bridge.c
 * @brief Hardware bridge for encoder - implementation
 * @date 2026-07-18
 */

#include "encoder_hw_bridge.h"
#include "encoder.h"

/* Platform-specific interrupt control for critical sections */
#if defined(_MSC_VER) || defined(__MINGW32__) || defined(__MINGW64__)
/* Host test build - no actual interrupts */
#define ENTER_CRITICAL_SECTION() do {} while(0)
#define EXIT_CRITICAL_SECTION() do {} while(0)
#else
/* Target build - disable interrupts during reset */
#include "ti_msp_dl_config.h"
#define ENTER_CRITICAL_SECTION() __disable_irq()
#define EXIT_CRITICAL_SECTION() __enable_irq()
#endif

int32_t EncoderHwBridge_GetCount(uint8_t physical_id)
{
    if (physical_id >= ENCODER_ID_COUNT) {
        return 0;
    }
    return Encoder_GetCount((Encoder_Id)physical_id);
}

void EncoderHwBridge_ResetCount(uint8_t physical_id)
{
    if (physical_id >= ENCODER_ID_COUNT) {
        return;
    }
    
    /* Critical section to protect against GPIO ISR races */
    ENTER_CRITICAL_SECTION();
    Encoder_ResetCount((Encoder_Id)physical_id);
    EXIT_CRITICAL_SECTION();
}
