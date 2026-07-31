#ifndef ATK_BLE02_H
#define ATK_BLE02_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * Start UART4 circular DMA reception for the ATK-BLE02 / ATK-MW579D.
 * Call once after the FreeRTOS scheduler has started.
 */
HAL_StatusTypeDef ATK_BLE02_Start(void);

/**
 * Transmit transparent serial data to the BLE module.
 * UART4 TX intentionally uses the normal HAL path; its DMA stream conflicts
 * with the active SPI2 TX allocation.
 */
HAL_StatusTypeDef ATK_BLE02_Write(const uint8_t *data,
                                 uint16_t length,
                                 uint32_t timeout_ms);

/** Return the number of bytes currently waiting in the software RX ring. */
size_t ATK_BLE02_Available(void);

/** Read one received byte. Returns false when the RX ring is empty. */
bool ATK_BLE02_ReadByte(uint8_t *data);

/** Read at most max_length bytes and return the number actually copied. */
size_t ATK_BLE02_Read(uint8_t *data, size_t max_length);

/** Number of bytes dropped because the software RX ring was full. */
uint32_t ATK_BLE02_GetOverflowCount(void);

/**
 * Feed a UART4 Receive-to-IDLE event into the driver.
 * The global HAL callback in atk_ble02.c calls this automatically.
 */
void ATK_BLE02_OnRxEvent(uint16_t dma_position);

#ifdef __cplusplus
}
#endif

#endif /* ATK_BLE02_H */
