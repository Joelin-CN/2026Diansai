#include "line_sensor.h"

#include "ti_msp_dl_config.h"

#if defined(LINE_SENSOR_S1_PIN)

#define SENSOR_ON(name)                                                                            \
    (((DL_GPIO_readPins(LINE_SENSOR_##name##_PORT, LINE_SENSOR_##name##_PIN) != 0U) ? 1U : 0U) == \
     LINE_SENSOR_ACTIVE_HIGH)

uint16_t LineSensor_ReadMask(void)
{
    uint16_t mask = 0U;

    if (SENSOR_ON(S1)) mask |= (1U << 0);
    if (SENSOR_ON(S2)) mask |= (1U << 1);
    if (SENSOR_ON(S3)) mask |= (1U << 2);
    if (SENSOR_ON(S4)) mask |= (1U << 3);
    if (SENSOR_ON(S5)) mask |= (1U << 4);
    if (SENSOR_ON(S6)) mask |= (1U << 5);
    if (SENSOR_ON(S7)) mask |= (1U << 6);
    if (SENSOR_ON(S8)) mask |= (1U << 7);
    if (SENSOR_ON(S9)) mask |= (1U << 8);
    if (SENSOR_ON(S10)) mask |= (1U << 9);
    if (SENSOR_ON(S11)) mask |= (1U << 10);
    if (SENSOR_ON(S12)) mask |= (1U << 11);

    return mask & LINE_SENSOR_USED_MASK;
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

#else

uint16_t LineSensor_ReadMask(void)
{
    return 0U;
}

bool LineSensor_GetError(uint16_t mask, int16_t *error)
{
    (void)mask;
    (void)error;
    return false;
}

#endif
