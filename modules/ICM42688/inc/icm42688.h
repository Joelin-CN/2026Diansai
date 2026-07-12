#ifndef ICM42688_H_
#define ICM42688_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ICM42688_WHO_AM_I_VALUE      (0x47U)
#define ICM42688_ACCEL_LSB_PER_G      (4096.0f)  /* +/-8 g */
#define ICM42688_GYRO_LSB_PER_DPS     (32.768f)  /* +/-1000 dps */

typedef enum {
    ICM42688_STATUS_OK = 0,
    ICM42688_STATUS_INVALID_ARGUMENT,
    ICM42688_STATUS_BAD_DEVICE_ID,
    ICM42688_STATUS_CONFIG_VERIFY_FAILED
} ICM42688_Status;

typedef struct {
    int16_t temperature;
    int16_t accelX;
    int16_t accelY;
    int16_t accelZ;
    int16_t gyroX;
    int16_t gyroY;
    int16_t gyroZ;
} ICM42688_RawData;

typedef struct {
    float temperatureC;
    float accelXG;
    float accelYG;
    float accelZG;
    float gyroXDps;
    float gyroYDps;
    float gyroZDps;
} ICM42688_Data;

/* Call SYSCFG_DL_init() before this function. */
ICM42688_Status ICM42688_Init(void);

uint8_t ICM42688_ReadDeviceId(void);
ICM42688_Status ICM42688_ReadRaw(ICM42688_RawData *data);
ICM42688_Status ICM42688_Read(ICM42688_Data *data);

/* The vehicle must remain stationary while this function is running. */
ICM42688_Status ICM42688_CalibrateGyro(uint16_t sampleCount, uint16_t sampleIntervalMs);
void ICM42688_SetGyroBias(float xDps, float yDps, float zDps);
void ICM42688_GetGyroBias(float *xDps, float *yDps, float *zDps);

#ifdef __cplusplus
}
#endif

#endif /* ICM42688_H_ */
