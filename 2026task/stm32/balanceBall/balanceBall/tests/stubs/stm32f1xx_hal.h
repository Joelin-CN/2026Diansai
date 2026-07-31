#ifndef STM32F1XX_HAL_H
#define STM32F1XX_HAL_H

#include <stdint.h>

typedef enum {
    HAL_OK = 0x00U,
    HAL_ERROR = 0x01U,
    HAL_BUSY = 0x02U,
    HAL_TIMEOUT = 0x03U
} HAL_StatusTypeDef;

typedef struct DMA_HandleTypeDef {
    uint32_t disabled_interrupts;
} DMA_HandleTypeDef;

typedef struct __UART_HandleTypeDef {
    DMA_HandleTypeDef *hdmarx;
} UART_HandleTypeDef;

#define DMA_IT_HT 0x00000004U
#define __HAL_DMA_DISABLE_IT(handle, interrupt) \
    ((handle)->disabled_interrupts |= (interrupt))

HAL_StatusTypeDef HAL_UART_Transmit_DMA(UART_HandleTypeDef *handle,
                                        const uint8_t *data, uint16_t size);
HAL_StatusTypeDef HAL_UARTEx_ReceiveToIdle_DMA(UART_HandleTypeDef *handle,
                                               uint8_t *data, uint16_t size);
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef *handle);
uint32_t HAL_GetTick(void);
uint32_t __get_PRIMASK(void);
void __disable_irq(void);
void __set_PRIMASK(uint32_t primask);

#endif
