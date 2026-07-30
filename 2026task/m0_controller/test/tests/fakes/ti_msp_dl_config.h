#ifndef TI_MSP_DL_CONFIG_H_
#define TI_MSP_DL_CONFIG_H_

#include <stdbool.h>
#include <stdint.h>

/* Fake I2C peripheral instance */
#define I2C0_INST ((void *)0)

/* I2C interrupt flags */
#define DL_I2C_INTERRUPT_CONTROLLER_TX_DONE         (1U << 0)
#define DL_I2C_INTERRUPT_CONTROLLER_RX_DONE         (1U << 1)
#define DL_I2C_INTERRUPT_CONTROLLER_NACK            (1U << 2)
#define DL_I2C_INTERRUPT_CONTROLLER_ARBITRATION_LOST (1U << 3)

/* I2C transfer direction */
typedef enum {
    DL_I2C_CONTROLLER_DIRECTION_TX,
    DL_I2C_CONTROLLER_DIRECTION_RX
} DL_I2C_CONTROLLER_DIRECTION;

/* Fake I2C state for test control */
typedef enum {
    FAKE_I2C_STATE_NORMAL,
    FAKE_I2C_STATE_TX_BUSY_FOREVER,
    FAKE_I2C_STATE_NACK
} fake_i2c_state_t;

extern fake_i2c_state_t g_fake_i2c_state;
extern uint32_t g_fake_i2c_timeout_counter;
extern uint8_t g_fake_i2c_rx_data[2];

void fake_i2c_reset(void);

/* Fake I2C DriverLib API */
uint32_t DL_I2C_getRawInterruptStatus(void *i2c, uint32_t mask);
void DL_I2C_clearInterruptStatus(void *i2c, uint32_t mask);
uint32_t DL_I2C_fillControllerTXFIFO(void *i2c, const uint8_t *data, uint32_t length);
void DL_I2C_startControllerTransfer(void *i2c, uint32_t target, DL_I2C_CONTROLLER_DIRECTION dir, uint32_t length);
bool DL_I2C_isControllerRXFIFOEmpty(void *i2c);
uint8_t DL_I2C_receiveControllerData(void *i2c);

#define ICM42688_TIMER_INST ((void *)1)

void DL_TimerG_startCounter(void *timer);
uint32_t DL_TimerG_getTimerCount(void *timer);
uint32_t __get_PRIMASK(void);
void __disable_irq(void);
void __enable_irq(void);

#endif
