#include "line_sensor.h"
#include "motor.h"
#include "ti_msp_dl_config.h"

#define BASE_SPEED          (450)
#define MAX_RUNNING_SPEED   (850)
#define SEARCH_SPEED        (260)
#define LOST_STOP_COUNT     (250U)
#define CONTROL_PERIOD_MS   (2U)
#define KP_NUMERATOR        (30)
#define KD_NUMERATOR        (15)
#define GAIN_DENOMINATOR    (100)

static int16_t ClampRunningSpeed(int32_t speed)
{
    if (speed > MAX_RUNNING_SPEED) return MAX_RUNNING_SPEED;
    if (speed < -MAX_RUNNING_SPEED) return -MAX_RUNNING_SPEED;
    return (int16_t)speed;
}

int main(void)
{
    int16_t error = 0;
    int16_t lastError = 0;
    uint16_t lostCount = LOST_STOP_COUNT;

    SYSCFG_DL_init();
    Motor_Init();

    while (1) {
        const uint16_t sensorMask = LineSensor_ReadMask();

        if (LineSensor_GetError(sensorMask, &error)) {
            const int32_t derivative = (int32_t)error - lastError;
            const int32_t correction =
                ((int32_t)KP_NUMERATOR * error + (int32_t)KD_NUMERATOR * derivative) /
                GAIN_DENOMINATOR;
            const int16_t leftSpeed = ClampRunningSpeed(BASE_SPEED + correction);
            const int16_t rightSpeed = ClampRunningSpeed(BASE_SPEED - correction);

            Motor_SetSpeed(leftSpeed, rightSpeed);
            lastError = error;
            lostCount = 0U;
        } else if (lostCount < LOST_STOP_COUNT) {
            /* Briefly turn toward the last known line position. */
            if (lastError < 0) {
                Motor_SetSpeed(-SEARCH_SPEED, SEARCH_SPEED);
            } else {
                Motor_SetSpeed(SEARCH_SPEED, -SEARCH_SPEED);
            }
            lostCount++;
        } else {
            Motor_Stop();
        }

        delay_cycles((CPUCLK_FREQ / 1000U) * CONTROL_PERIOD_MS);
    }
}
