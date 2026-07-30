/**
 * @file icm42688_stm32.c
 * @brief ICM42688 STM32平台适配器实现（SPI2）
 * @date 2026-07-29
 */

#include "icm42688_stm32.h"
#include "icm42688_hal.h"
#include "spi.h"
#include "gpio.h"
#include "main.h"
#include "platform_time.h"
#include "cmsis_os.h"

/* SPI 读寄存器 */
static uint8_t _read_reg(uint8_t reg) {
    uint8_t tx[2] = {(uint8_t)(reg | 0x80), 0x00};  // 读操作：最高位置1
    uint8_t rx[2] = {0, 0};

    HAL_GPIO_WritePin(IMU_CS_GPIO_Port, IMU_CS_Pin, GPIO_PIN_RESET);
    HAL_SPI_TransmitReceive(&hspi2, tx, rx, 2, 10);
    HAL_GPIO_WritePin(IMU_CS_GPIO_Port, IMU_CS_Pin, GPIO_PIN_SET);

    return rx[1];
}

/* SPI 写寄存器 */
static void _write_reg(uint8_t reg, uint8_t value) {
    uint8_t tx[2] = {(uint8_t)(reg & 0x7F), value};  // 写操作：最高位清0

    HAL_GPIO_WritePin(IMU_CS_GPIO_Port, IMU_CS_Pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(&hspi2, tx, 2, 10);
    HAL_GPIO_WritePin(IMU_CS_GPIO_Port, IMU_CS_Pin, GPIO_PIN_SET);
}

/* SPI 读多个寄存器 */
static void _read_regs(uint8_t reg, uint8_t *data, uint8_t len) {
    uint8_t tx_reg = (uint8_t)(reg | 0x80);  // 读操作：最高位置1

    HAL_GPIO_WritePin(IMU_CS_GPIO_Port, IMU_CS_Pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(&hspi2, &tx_reg, 1, 10);
    HAL_SPI_Receive(&hspi2, data, len, 10);
    HAL_GPIO_WritePin(IMU_CS_GPIO_Port, IMU_CS_Pin, GPIO_PIN_SET);
}

/* 初始化（空实现，SPI已由CubeMX初始化） */
static void _init(void) {
    // SPI2和CS引脚已由CubeMX初始化
}

/* 延时（FreeRTOS 任务内用 osDelay；初始化时在任务外可用 HAL_Delay） */
static void _delay_ms(uint16_t ms) {
    if (osKernelGetState() == osKernelRunning) {
        osDelay(ms);
    } else {
        HAL_Delay(ms);
    }
}

static const icm42688_comm_t   s_comm   = {
    .init      = _init,
    .read_reg  = _read_reg,
    .write_reg = _write_reg,
    .read_regs = _read_regs
};

static const icm42688_system_t s_system = {
    .delay_ms = _delay_ms
};

void icm42688_stm32_bind(const icm42688_config_t *config) {
    icm42688_hal_init(&s_comm, &s_system, config);
}
