#ifndef LINE_SENSOR_H_
#define LINE_SENSOR_H_

#include <stdbool.h>
#include <stdint.h>

#define LINE_SENSOR_COUNT       (8U)
#define LINE_SENSOR_ACTIVE_HIGH (1)
#define LINE_SENSOR_USED_MASK   (0x00FFU)

uint16_t LineSensor_ReadMask(void);
bool LineSensor_GetError(uint16_t mask, int16_t *error);

#endif
