/**
 * @file spi_diagnostic.c
 * @brief SPI通信诊断工具
 * @date 2026-07-29
 */

#include "spi_diagnostic.h"
#include "spi.h"
#include "main.h"
#include <stdio.h>

static HAL_StatusTypeDef SpiDiag_ReadRegister(uint8_t reg, uint8_t *value)
{
    uint8_t address = (uint8_t)(reg | 0x80U);
    HAL_StatusTypeDef tx_status;
    HAL_StatusTypeDef rx_status;

    *value = 0U;
    HAL_GPIO_WritePin(IMU_CS_GPIO_Port, IMU_CS_Pin, GPIO_PIN_RESET);
    tx_status = HAL_SPI_Transmit(&hspi2, &address, 1U, 100U);
    rx_status = HAL_SPI_Receive(&hspi2, value, 1U, 100U);
    HAL_GPIO_WritePin(IMU_CS_GPIO_Port, IMU_CS_Pin, GPIO_PIN_SET);

    return (tx_status != HAL_OK) ? tx_status : rx_status;
}

void SpiDiag_TestICM42688(void) {
    printf("\n");
    printf("========================================\n");
    printf("  SPI2 Communication Diagnostic Tool\n");
    printf("========================================\n\n");

    /* 测试1: 读取WHO_AM_I寄存器 */
    printf("[Test 1] Reading WHO_AM_I register (0x75)...\n");
    uint8_t value = 0U;
    HAL_StatusTypeDef status = SpiDiag_ReadRegister(0x75U, &value);

    printf("  HAL Status: %d (0=OK)\n", status);
    printf("  TX address: 0xF5; RX value: 0x%02X\n", value);
    printf("  WHO_AM_I value: 0x%02X (expected 0x47)\n", value);

    if (value == 0xFF) {
        printf("  [ERROR] Read 0xFF - MISO stuck high\n");
        printf("    Possible causes:\n");
        printf("    - MISO not connected\n");
        printf("    - Sensor not powered\n");
        printf("    - Pull-up resistor on MISO\n");
    } else if (value == 0x00) {
        printf("  [ERROR] Read 0x00 - MISO stuck low or no response\n");
        printf("    Possible causes:\n");
        printf("    - SPI clock not running\n");
        printf("    - CS not connected to sensor\n");
        printf("    - Wrong CS pin\n");
    } else if (value == 0x47) {
        printf("  [OK] WHO_AM_I correct! ICM42688 detected.\n");
    } else {
        printf("  [WARNING] Unexpected value 0x%02X\n", value);
        printf("    - May be different sensor model\n");
        printf("    - Or communication timing issue\n");
    }

    /* 测试2: 读取DEVICE_CONFIG寄存器 */
    printf("\n[Test 2] Reading DEVICE_CONFIG register (0x11)...\n");
    status = SpiDiag_ReadRegister(0x11U, &value);
    printf("  HAL Status: %d (0=OK)\n", status);
    printf("  DEVICE_CONFIG: 0x%02X\n", value);

    /* 测试3: 读取PWR_MGMT0寄存器 */
    printf("\n[Test 3] Reading PWR_MGMT0 register (0x4E)...\n");
    status = SpiDiag_ReadRegister(0x4EU, &value);
    printf("  HAL Status: %d (0=OK)\n", status);
    printf("  PWR_MGMT0: 0x%02X (0x00=sleep, 0x0F=accel+gyro on)\n", value);

    /* 硬件信息总结 */
    printf("\n========================================\n");
    printf("  Hardware Configuration Summary\n");
    printf("========================================\n");
    printf("SPI2 Pins:\n");
    printf("  SCK  (PB13) - SPI2_SCK\n");
    printf("  MISO (PB14) - SPI2_MISO\n");
    printf("  MOSI (PB15) - SPI2_MOSI\n");
    printf("  CS   (PE7)  - IMU_CS (GPIO output)\n");
    printf("\nSPI2 Configuration:\n");
    printf("  Mode: 1 (CPOL=0, CPHA=1 / 2EDGE)\n");
    printf("  Speed: PCLK/4 (10.5MHz @ 42MHz APB1)\n");
    printf("  Data size: 8-bit\n");
    printf("  First bit: MSB\n");
    printf("\n");
}
