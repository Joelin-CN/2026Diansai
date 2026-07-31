#include "mcp23017.h"

#include "ti_msp_dl_config.h"

#define MCP23017_REG_IODIRA (0x00U)
#define MCP23017_REG_IODIRB (0x01U)
#define MCP23017_REG_GPIOA  (0x12U)
#define MCP23017_I2C_TIMEOUT (100000U)
#define MCP23017_I2C_ERRORS \
    (DL_I2C_INTERRUPT_CONTROLLER_NACK | DL_I2C_INTERRUPT_CONTROLLER_ARBITRATION_LOST)

static mcp23017_status_t MCP23017_WaitFor(uint32_t completeMask)
{
    uint32_t timeout = MCP23017_I2C_TIMEOUT;
    while (timeout-- != 0U) {
        const uint32_t flags = DL_I2C_getRawInterruptStatus(I2C0_INST, completeMask | MCP23017_I2C_ERRORS);
        if ((flags & MCP23017_I2C_ERRORS) != 0U) {
            DL_I2C_clearInterruptStatus(I2C0_INST, flags & MCP23017_I2C_ERRORS);
            return MCP23017_STATUS_IO_ERROR;
        }
        if ((flags & completeMask) != 0U) {
            DL_I2C_clearInterruptStatus(I2C0_INST, flags & completeMask);
            return MCP23017_STATUS_OK;
        }
    }
    return MCP23017_STATUS_TIMEOUT;
}

static mcp23017_status_t MCP23017_Write(const uint8_t *data, uint16_t length)
{
    const uint32_t clearMask = DL_I2C_INTERRUPT_CONTROLLER_TX_DONE | MCP23017_I2C_ERRORS;
    DL_I2C_clearInterruptStatus(I2C0_INST, clearMask);
    if (DL_I2C_fillControllerTXFIFO(I2C0_INST, data, length) != length) {
        return MCP23017_STATUS_IO_ERROR;
    }
    DL_I2C_startControllerTransfer(I2C0_INST, MCP23017_I2C_ADDRESS,
                                   DL_I2C_CONTROLLER_DIRECTION_TX, length);
    return MCP23017_WaitFor(DL_I2C_INTERRUPT_CONTROLLER_TX_DONE);
}

static mcp23017_status_t MCP23017_SetRegister(uint8_t reg, uint8_t value)
{
    const uint8_t data[2] = {reg, value};
    return MCP23017_Write(data, sizeof(data));
}

mcp23017_status_t MCP23017_Init(void)
{
    mcp23017_status_t status;
    
    status = MCP23017_SetRegister(MCP23017_REG_IODIRA, 0xFFU);
    if (status != MCP23017_STATUS_OK) return status;
    
    status = MCP23017_SetRegister(MCP23017_REG_IODIRB, 0xFFU);
    return status;
}

mcp23017_status_t MCP23017_ReadInputs(uint16_t *inputs)
{
    const uint8_t reg = MCP23017_REG_GPIOA;
    const uint32_t clearMask = DL_I2C_INTERRUPT_CONTROLLER_RX_DONE | MCP23017_I2C_ERRORS;
    mcp23017_status_t status;
    uint8_t portA;
    uint8_t portB;

    if (inputs == NULL) return MCP23017_STATUS_INVALID_ARGUMENT;
    
    status = MCP23017_Write(&reg, 1U);
    if (status != MCP23017_STATUS_OK) return status;
    
    DL_I2C_clearInterruptStatus(I2C0_INST, clearMask);
    DL_I2C_startControllerTransfer(I2C0_INST, MCP23017_I2C_ADDRESS,
                                   DL_I2C_CONTROLLER_DIRECTION_RX, 2U);
    
    status = MCP23017_WaitFor(DL_I2C_INTERRUPT_CONTROLLER_RX_DONE);
    if (status != MCP23017_STATUS_OK) return status;
    
    if (DL_I2C_isControllerRXFIFOEmpty(I2C0_INST)) return MCP23017_STATUS_IO_ERROR;
    portA = DL_I2C_receiveControllerData(I2C0_INST);
    
    if (DL_I2C_isControllerRXFIFOEmpty(I2C0_INST)) return MCP23017_STATUS_IO_ERROR;
    portB = DL_I2C_receiveControllerData(I2C0_INST);
    
    *inputs = (uint16_t)portA | ((uint16_t)portB << 8U);
    return MCP23017_STATUS_OK;
}
