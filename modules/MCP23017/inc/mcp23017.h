#ifndef MCP23017_H_
#define MCP23017_H_

#include <stdbool.h>
#include <stdint.h>

#define MCP23017_I2C_ADDRESS (0x20U)

bool MCP23017_Init(void);
bool MCP23017_ReadInputs(uint16_t *inputs);

#endif
