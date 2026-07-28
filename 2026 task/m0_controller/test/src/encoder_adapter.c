/**
 * @file encoder_adapter.c
 * @brief Encoder adapter for Motion Control - implementation
 * @date 2026-07-18
 */

#include "encoder_adapter.h"
#include "encoder_hw_bridge.h"

/**
 * @brief Physical encoder mapping:
 * - ENCODER_LEFT_FRONT  (0) -> M1 (physical 0)
 * - ENCODER_LEFT_REAR   (1) -> M2 (physical 1)
 * - ENCODER_RIGHT_FRONT (2) -> M3 (physical 2)
 * - ENCODER_RIGHT_REAR  (3) -> M4 (physical 3)
 */

static int32_t EncoderAdapter_GetCount(EncoderId_t id)
{
    uint8_t physical_id;
    
    switch (id) {
        case ENCODER_LEFT_FRONT:
            physical_id = 0; /* M1 */
            break;
        case ENCODER_LEFT_REAR:
            physical_id = 1; /* M2 */
            break;
        case ENCODER_RIGHT_FRONT:
            physical_id = 2; /* M3 */
            break;
        case ENCODER_RIGHT_REAR:
            physical_id = 3; /* M4 */
            break;
        default:
            return 0;
    }
    
    return EncoderHwBridge_GetCount(physical_id);
}

static void EncoderAdapter_ResetCount(EncoderId_t id)
{
    uint8_t physical_id;
    
    switch (id) {
        case ENCODER_LEFT_FRONT:
            physical_id = 0; /* M1 */
            break;
        case ENCODER_LEFT_REAR:
            physical_id = 1; /* M2 */
            break;
        case ENCODER_RIGHT_FRONT:
            physical_id = 2; /* M3 */
            break;
        case ENCODER_RIGHT_REAR:
            physical_id = 3; /* M4 */
            break;
        default:
            return;
    }
    
    EncoderHwBridge_ResetCount(physical_id);
}

/* Static interface instance - module-owned, non-const for Motion Control */
static EncoderInterface_t g_encoder_interface = {
    .getCount = EncoderAdapter_GetCount,
    .resetCount = EncoderAdapter_ResetCount
};

EncoderInterface_t *EncoderAdapter_GetInterface(void)
{
    return &g_encoder_interface;
}
