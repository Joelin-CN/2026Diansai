/**
 * @file ir_raw_capture.h
 * @brief IR sensor raw data capture tool
 * @date 2026-07-30
 */

#ifndef IR_RAW_CAPTURE_H_
#define IR_RAW_CAPTURE_H_

#include <stdint.h>

/**
 * @brief Capture raw IR sensor data for 2 seconds and display
 *
 * Shows:
 * - Raw data in ASCII/Hex format
 * - Pure hex dump
 * - Frame marker analysis
 * - First complete frame extraction
 */
void IrRawCapture_Run(void);

/**
 * @brief Capture raw byte (call from USART2 IRQ handler)
 * @param byte Received byte
 */
void IrRawCapture_RxByte(uint8_t byte);

#endif // IR_RAW_CAPTURE_H_
