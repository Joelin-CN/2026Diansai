/**
 * @file      icm42688_mspm0.c
 * @brief     MSPM0G3507 DriverLib adapter implementation
 * @date      2026-07-17
 */

#include "icm42688_mspm0.h"

/* ======================================================================
 * Private SPI / GPIO primitives
 * ====================================================================== */

/** Pull the ICM42688 chip-select line low (active). */
static inline void cs_low(void)
{
    DL_GPIO_clearPins(ICM42688_PORT, ICM42688_CS_PIN);
}

/** Release the ICM42688 chip-select line (high / idle). */
static inline void cs_high(void)
{
    DL_GPIO_setPins(ICM42688_PORT, ICM42688_CS_PIN);
}

/** Blocking full-duplex single-byte SPI exchange. */
static uint8_t spi_exchange(uint8_t value)
{
    DL_SPI_transmitDataBlocking8(SPI1_INST, value);
    return DL_SPI_receiveDataBlocking8(SPI1_INST);
}

/* ======================================================================
 * icm42688_comm_t implementation
 * ====================================================================== */

/** Comm init: ensure CS is idle-high. SysConfig owns peripheral setup. */
static void comm_init(void)
{
    cs_high();
}

/** Single-register read: addr | 0x80, then dummy 0xFF to clock data out. */
static uint8_t read_reg(uint8_t reg)
{
    uint8_t value;

    cs_low();
    (void)spi_exchange((uint8_t)(reg | 0x80U));
    value = spi_exchange(0xFFU);
    cs_high();

    return value;
}

/** Single-register write: addr & 0x7F, then data byte. */
static void write_reg(uint8_t reg, uint8_t value)
{
    cs_low();
    (void)spi_exchange((uint8_t)(reg & 0x7FU));
    (void)spi_exchange(value);
    cs_high();
}

/**
 * Burst read: assert CS once, send address | 0x80, then clock out len dummy
 * bytes in a single CS-low window.
 */
static void read_regs(uint8_t reg, uint8_t *data, uint8_t len)
{
    if (data == NULL || len == 0U) {
        return;
    }

    cs_low();
    (void)spi_exchange((uint8_t)(reg | 0x80U));
    for (uint8_t i = 0U; i < len; i++) {
        data[i] = spi_exchange(0xFFU);
    }
    cs_high();
}

/* ======================================================================
 * icm42688_system_t implementation
 * ====================================================================== */

/**
 * Millisecond delay built on delay_cycles().
 * Loops per-millisecond to avoid (ms * cycles_per_ms) overflow at large ms.
 */
static void delay_ms(uint16_t ms)
{
    const uint32_t cycles_per_ms = CPUCLK_FREQ / 1000U;

    for (uint16_t i = 0U; i < ms; i++) {
        delay_cycles(cycles_per_ms);
    }
}

/* ======================================================================
 * ahrs_timer_t implementation
 * ====================================================================== */

/**
 * TIMG0 is configured and started by SysConfig; init/start are no-ops.
 * The timer counter directly yields a microsecond timestamp.
 */
static uint32_t timer_get_time_us(void)
{
    return DL_TimerG_getTimerCount(ICM42688_TIMER_INST);
}

static void timer_noop(void)
{
    /* SysConfig owns TIMG0 init/start - nothing to do here. */
}

/* ======================================================================
 * Exported interface instances
 * ====================================================================== */

const icm42688_comm_t icm42688_mspm0_comm = {
    .init      = comm_init,
    .read_reg  = read_reg,
    .write_reg = write_reg,
    .read_regs = read_regs,
};

const icm42688_system_t icm42688_mspm0_system = {
    .delay_ms = delay_ms,
};

const ahrs_timer_t icm42688_mspm0_timer = {
    .get_time_us = timer_get_time_us,
    .init        = timer_noop,
    .start       = timer_noop,
};

/* ======================================================================
 * Binding
 * ====================================================================== */

void icm42688_mspm0_bind(const icm42688_config_t *config)
{
    icm42688_hal_init(&icm42688_mspm0_comm,
                      &icm42688_mspm0_system,
                      config);
    ahrs_hal_init(&icm42688_mspm0_timer);
}
