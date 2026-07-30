#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "ti_msp_dl_config.h"

/* Fake I2C state management - definitions */
fake_i2c_state_t g_fake_i2c_state = FAKE_I2C_STATE_NORMAL;
uint32_t g_fake_i2c_timeout_counter = 0;
uint8_t g_fake_i2c_rx_data[2] = {0x55, 0x0A}; /* Default: 0x0A55 */

void fake_i2c_reset(void)
{
    g_fake_i2c_state = FAKE_I2C_STATE_NORMAL;
    g_fake_i2c_timeout_counter = 0;
    g_fake_i2c_rx_data[0] = 0x55;
    g_fake_i2c_rx_data[1] = 0x0A;
}

/* Fake I2C DriverLib implementation */

static uint32_t s_pending_flags = 0;
static uint8_t s_rx_index = 0;

uint32_t DL_I2C_getRawInterruptStatus(void *i2c, uint32_t mask)
{
    (void)i2c;
    
    if (g_fake_i2c_state == FAKE_I2C_STATE_TX_BUSY_FOREVER) {
        g_fake_i2c_timeout_counter++;
        return 0;
    }
    
    if (g_fake_i2c_state == FAKE_I2C_STATE_NACK) {
        return DL_I2C_INTERRUPT_CONTROLLER_NACK & mask;
    }
    
    return s_pending_flags & mask;
}

void DL_I2C_clearInterruptStatus(void *i2c, uint32_t mask)
{
    (void)i2c;
    s_pending_flags &= ~mask;
}

uint32_t DL_I2C_fillControllerTXFIFO(void *i2c, const uint8_t *data, uint32_t length)
{
    (void)i2c;
    (void)data;
    return length;
}

void DL_I2C_startControllerTransfer(void *i2c, uint32_t target, DL_I2C_CONTROLLER_DIRECTION dir, uint32_t length)
{
    (void)i2c;
    (void)target;
    (void)length;
    
    if (dir == DL_I2C_CONTROLLER_DIRECTION_TX) {
        s_pending_flags = DL_I2C_INTERRUPT_CONTROLLER_TX_DONE;
    } else {
        s_pending_flags = DL_I2C_INTERRUPT_CONTROLLER_RX_DONE;
        s_rx_index = 0;
    }
}

bool DL_I2C_isControllerRXFIFOEmpty(void *i2c)
{
    (void)i2c;
    return s_rx_index >= 2;
}

uint8_t DL_I2C_receiveControllerData(void *i2c)
{
    (void)i2c;
    if (s_rx_index < 2) {
        return g_fake_i2c_rx_data[s_rx_index++];
    }
    return 0;
}

/* Include MCP23017 implementation */
#include "../modules/MCP23017/src/mcp23017.c"

/* Test helpers */
static void test_invalid_argument(void)
{
    fake_i2c_reset();
    assert(MCP23017_ReadInputs(NULL) == MCP23017_STATUS_INVALID_ARGUMENT);
    printf("PASS: test_invalid_argument\n");
}

static void test_normal_operation(void)
{
    uint16_t mask = 0;
    fake_i2c_reset();
    
    assert(MCP23017_Init() == MCP23017_STATUS_OK);
    assert(MCP23017_ReadInputs(&mask) == MCP23017_STATUS_OK);
    assert(mask == UINT16_C(0x0A55));
    printf("PASS: test_normal_operation\n");
}

static void test_timeout(void)
{
    uint16_t mask = 0;
    fake_i2c_reset();
    g_fake_i2c_state = FAKE_I2C_STATE_TX_BUSY_FOREVER;
    
    assert(MCP23017_Init() == MCP23017_STATUS_TIMEOUT);
    assert(g_fake_i2c_timeout_counter > 0);
    printf("PASS: test_timeout\n");
}

static void test_nack(void)
{
    uint16_t mask = 0;
    fake_i2c_reset();
    g_fake_i2c_state = FAKE_I2C_STATE_NACK;
    
    assert(MCP23017_Init() == MCP23017_STATUS_IO_ERROR);
    printf("PASS: test_nack\n");
}

int main(void)
{
    test_invalid_argument();
    test_normal_operation();
    test_timeout();
    test_nack();
    
    printf("All MCP23017 tests passed!\n");
    return 0;
}
