#include "line_sensor.h"

#include "mcp23017.h"

uint16_t LineSensor_ReadMask(void)
{
    uint16_t mask;

    if (MCP23017_ReadInputs(&mask) != MCP23017_STATUS_OK) return 0U;
#if LINE_SENSOR_ACTIVE_HIGH
    return mask & LINE_SENSOR_USED_MASK;
#else
    return (~mask) & LINE_SENSOR_USED_MASK;
#endif
}

bool LineSensor_GetError(uint16_t mask, int16_t *error)
{
    static const int16_t weights[LINE_SENSOR_COUNT] = {
        -1100, -900, -700, -500, -300, -100, 100, 300, 500, 700, 900, 1100,
    };
    int32_t sum = 0;
    uint8_t activeCount = 0U;
    uint8_t i;

    mask &= LINE_SENSOR_USED_MASK;
    for (i = 0U; i < LINE_SENSOR_COUNT; i++) {
        if ((mask & (1U << i)) != 0U) {
            sum += weights[i];
            activeCount++;
        }
    }

    if (activeCount == 0U) return false;

    *error = (int16_t)(sum / activeCount);
    return true;
}

