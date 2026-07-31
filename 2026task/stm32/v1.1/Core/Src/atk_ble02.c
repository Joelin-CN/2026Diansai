#include "atk_ble02.h"

#include "usart.h"

#define ATK_BLE02_DMA_RX_SIZE  256U
#define ATK_BLE02_RING_SIZE    256U

static uint8_t dma_rx_buffer[ATK_BLE02_DMA_RX_SIZE];
static uint8_t rx_ring[ATK_BLE02_RING_SIZE];
static volatile uint16_t rx_head;
static volatile uint16_t rx_tail;
static volatile uint32_t rx_overflow_count;
static uint16_t dma_old_position;

static void ATK_BLE02_PushByte(uint8_t data)
{
  const uint16_t next =
      (uint16_t)((rx_head + 1U) % ATK_BLE02_RING_SIZE);

  if (next == rx_tail)
  {
    ++rx_overflow_count;
    return;
  }

  rx_ring[rx_head] = data;
  rx_head = next;
}

static void ATK_BLE02_CopyDmaRange(uint16_t begin, uint16_t end)
{
  uint16_t index;

  for (index = begin; index < end; ++index)
  {
    ATK_BLE02_PushByte(dma_rx_buffer[index]);
  }
}

HAL_StatusTypeDef ATK_BLE02_Start(void)
{
  HAL_StatusTypeDef status;

  rx_head = 0U;
  rx_tail = 0U;
  rx_overflow_count = 0U;
  dma_old_position = 0U;

  status = HAL_UARTEx_ReceiveToIdle_DMA(&huart4,
                                       dma_rx_buffer,
                                       ATK_BLE02_DMA_RX_SIZE);
  if (status == HAL_OK)
  {
    /* IDLE and transfer-complete events are sufficient for this stream. */
    __HAL_DMA_DISABLE_IT(huart4.hdmarx, DMA_IT_HT);
  }

  return status;
}

HAL_StatusTypeDef ATK_BLE02_Write(const uint8_t *data,
                                 uint16_t length,
                                 uint32_t timeout_ms)
{
  if ((data == NULL) || (length == 0U))
  {
    return HAL_ERROR;
  }

  return HAL_UART_Transmit(&huart4, (uint8_t *)data, length, timeout_ms);
}

size_t ATK_BLE02_Available(void)
{
  const uint16_t head = rx_head;
  const uint16_t tail = rx_tail;

  if (head >= tail)
  {
    return (size_t)(head - tail);
  }

  return (size_t)(ATK_BLE02_RING_SIZE - tail + head);
}

bool ATK_BLE02_ReadByte(uint8_t *data)
{
  uint16_t tail;

  if ((data == NULL) || (rx_tail == rx_head))
  {
    return false;
  }

  tail = rx_tail;
  *data = rx_ring[tail];
  rx_tail = (uint16_t)((tail + 1U) % ATK_BLE02_RING_SIZE);
  return true;
}

size_t ATK_BLE02_Read(uint8_t *data, size_t max_length)
{
  size_t count = 0U;

  if (data == NULL)
  {
    return 0U;
  }

  while ((count < max_length) && ATK_BLE02_ReadByte(&data[count]))
  {
    ++count;
  }

  return count;
}

uint32_t ATK_BLE02_GetOverflowCount(void)
{
  return rx_overflow_count;
}

void ATK_BLE02_OnRxEvent(uint16_t dma_position)
{
  if (dma_position > ATK_BLE02_DMA_RX_SIZE)
  {
    return;
  }

  if (dma_position > dma_old_position)
  {
    ATK_BLE02_CopyDmaRange(dma_old_position, dma_position);
  }
  else if (dma_position < dma_old_position)
  {
    ATK_BLE02_CopyDmaRange(dma_old_position, ATK_BLE02_DMA_RX_SIZE);
    ATK_BLE02_CopyDmaRange(0U, dma_position);
  }

  dma_old_position =
      (dma_position == ATK_BLE02_DMA_RX_SIZE) ? 0U : dma_position;
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t size)
{
  if (huart->Instance == UART4)
  {
    ATK_BLE02_OnRxEvent(size);
  }
}
