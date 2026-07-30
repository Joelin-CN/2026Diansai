/**
 * @file spi_diagnostic.c
 * @brief SPI通信诊断工具
 * @date 2026-07-29
 */

#include "spi_diagnostic.h"
#include "spi.h"
#include "main.h"
#include <stdio.h>

void SpiDiag_TestICM42688(void) {
    printf("\n");
    printf("========================================\n");
    printf("  SPI2 Communication Diagnostic Tool\n");
    printf("========================================\n\n");

    /* 测试1: 读取WHO_AM_I寄存器 */
    printf("[Test 1] Reading WHO_AM_I register (0x75)...\n");
    uint8_t tx[2] = {0x75 | 0x80, 0x00};  // 读操作
    uint8_t rx[2] = {0, 0};

    HAL_GPIO_WritePin(IMU_CS_GPIO_Port, IMU_CS_Pin, GPIO_PIN_RESET);
    HAL_StatusTypeDef status = HAL_SPI_TransmitReceive(&hspi2, tx, rx, 2, 100);
    HAL_GPIO_WritePin(IMU_CS_GPIO_Port, IMU_CS_Pin, GPIO_PIN_SET);

    printf("  HAL Status: %d (0=OK)\n", status);
    printf("  TX: [0x%02X, 0x%02X]\n", tx[0], tx[1]);
    printf("  RX: [0x%02X, 0x%02X]\n", rx[0], rx[1]);
    printf("  WHO_AM_I value: 0x%02X (expected 0x47)\n", rx[1]);

    if (rx[1] == 0xFF) {
        printf("  [ERROR] Read 0xFF - MISO stuck high\n");
        printf("    Possible causes:\n");
        printf("    - MISO not connected\n");
        printf("    - Sensor not powered\n");
        printf("    - Pull-up resistor on MISO\n");
    } else if (rx[1] == 0x00) {
        printf("  [ERROR] Read 0x00 - MISO stuck low or no response\n");
        printf("    Possible causes:\n");
        printf("    - SPI clock not running\n");
        printf("    - CS not connected to sensor\n");
        printf("    - Wrong CS pin\n");
    } else if (rx[1] == 0x47) {
        printf("  [OK] WHO_AM_I correct! ICM42688 detected.\n");
    } else {
        printf("  [WARNING] Unexpected value 0x%02X\n", rx[1]);
        printf("    - May be different sensor model\n");
        printf("    - Or communication timing issue\n");
    }

    /* 测试2: 读取DEVICE_CONFIG寄存器 */
    printf("\n[Test 2] Reading DEVICE_CONFIG register (0x11)...\n");
    tx[0] = 0x11 | 0x80;
    tx[1] = 0x00;
    rx[0] = 0; rx[1] = 0;

    HAL_GPIO_WritePin(IMU_CS_GPIO_Port, IMU_CS_Pin, GPIO_PIN_RESET);
    status = HAL_SPI_TransmitReceive(&hspi2, tx, rx, 2, 100);
    HAL_GPIO_WritePin(IMU_CS_GPIO_Port, IMU_CS_Pin, GPIO_PIN_SET);

    printf("  DEVICE_CONFIG: 0x%02X\n", rx[1]);

    /* 测试3: 读取PWR_MGMT0寄存器 */
    printf("\n[Test 3] Reading PWR_MGMT0 register (0x4E)...\n");
    tx[0] = 0x4E | 0x80;
    tx[1] = 0x00;
    rx[0] = 0; rx[1] = 0;

    HAL_GPIO_WritePin(IMU_CS_GPIO_Port, IMU_CS_Pin, GPIO_PIN_RESET);
    status = HAL_SPI_TransmitReceive(&hspi2, tx, rx, 2, 100);
    HAL_GPIO_WritePin(IMU_CS_GPIO_Port, IMU_CS_Pin, GPIO_PIN_SET);

    printf("  PWR_MGMT0: 0x%02X (0x00=sleep, 0x0F=accel+gyro on)\n", rx[1]);

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
    printf("  Mode: 0 (CPOL=0, CPHA=0)\n");
    printf("  Speed: PCLK/32 (~1.3MHz @ 42MHz APB1)\n");
    printf("  Data size: 8-bit\n");
    printf("  First bit: MSB\n");
    printf("\n");
}
