/**
 * @file encoder_hw_bridge.h
 * @brief Hardware bridge for encoder - isolates low-level driver dependencies
 * @date 2026-07-18
 * 
 * This bridge provides a clean interface to encoder hardware without exposing
 * the low-level driver headers (encoder.h). It uses only stdint types.
 */

#ifndef ENCODER_HW_BRIDGE_H
#define ENCODER_HW_BRIDGE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Get encoder count for a physical encoder ID
 * @param physical_id Physical encoder identifier (0-3)
 * @return Encoder count, or 0 if physical_id is out of range
 */
int32_t EncoderHwBridge_GetCount(uint8_t physical_id);

/**
 * @brief Reset encoder count for a physical encoder ID
 * @param physical_id Physical encoder identifier (0-3)
 * @note This function is ISR-safe (uses critical section)
 */
void EncoderHwBridge_ResetCount(uint8_t physical_id);

#ifdef __cplusplus
}
#endif

#endif /* ENCODER_HW_BRIDGE_H */
