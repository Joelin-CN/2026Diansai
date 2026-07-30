/**
 * @file encoder_adapter.h
 * @brief Encoder adapter for Motion Control module
 * @date 2026-07-18
 * 
 * Provides the EncoderInterface_t vtable that bridges Motion Control's
 * logical encoder IDs to the physical hardware encoder IDs.
 */

#ifndef ENCODER_ADAPTER_H
#define ENCODER_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../modules/Motion Control/inc/motion_feedback.h"

/**
 * @brief Get the encoder interface for Motion Control
 * @return Pointer to static EncoderInterface_t instance
 */
EncoderInterface_t *EncoderAdapter_GetInterface(void);

#ifdef __cplusplus
}
#endif

#endif /* ENCODER_ADAPTER_H */
