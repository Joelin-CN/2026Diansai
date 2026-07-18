#ifndef MCP23017_H_
#define MCP23017_H_

#include <stdbool.h>
#include <stdint.h>

#define MCP23017_I2C_ADDRESS (0x20U)

typedef enum {
    MCP23017_STATUS_OK = 0,
    MCP23017_STATUS_INVALID_ARGUMENT,
    MCP23017_STATUS_TIMEOUT,
    MCP23017_STATUS_IO_ERROR
} mcp23017_status_t;

mcp23017_status_t MCP23017_Init(void);
mcp23017_status_t MCP23017_ReadInputs(uint16_t *inputs);

/* Legacy compatibility - deprecated */
bool MCP23017_Init_Legacy(void);
bool MCP23017_ReadInputs_Legacy(uint16_t *inputs);

#endif
