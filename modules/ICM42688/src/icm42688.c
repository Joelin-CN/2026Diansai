#include "icm42688.h"

#include <stddef.h>

#include "ti_msp_dl_config.h"

#define ICM42688_REG_DEVICE_CONFIG       (0x11U)
#define ICM42688_REG_TEMP_DATA1          (0x1DU)
#define ICM42688_REG_PWR_MGMT0           (0x4EU)
#define ICM42688_REG_GYRO_CONFIG0        (0x4FU)
#define ICM42688_REG_ACCEL_CONFIG0       (0x50U)
#define ICM42688_REG_WHO_AM_I            (0x75U)

#define ICM42688_SPI_READ_BIT            (0x80U)
#define ICM42688_SPI_WRITE_MASK          (0x7FU)
#define ICM42688_SPI_DUMMY_BYTE          (0xFFU)

#define ICM42688_SOFT_RESET              (0x01U)
#define ICM42688_ACCEL_GYRO_LOW_NOISE    (0x0FU)
#define ICM42688_FS_8G_ODR_1KHZ          (0x26U)
#define ICM42688_FS_1000DPS_ODR_1KHZ     (0x26U)

#define ICM42688_BURST_DATA_LENGTH       (14U)
#define ICM42688_TEMPERATURE_SCALE       (132.48f)
#define ICM42688_TEMPERATURE_OFFSET_C    (25.0f)

static float gGyroBiasXDps;
static float gGyroBiasYDps;
static float gGyroBiasZDps;

static void ICM42688_DelayMs(uint32_t milliseconds)
{
    while (milliseconds > 0U) {
        delay_cycles(CPUCLK_FREQ / 1000U);
        milliseconds--;
    }
}

static void ICM42688_ChipSelectLow(void)
{
    DL_GPIO_clearPins(ICM42688_PORT, ICM42688_CS_PIN);
}

static void ICM42688_ChipSelectHigh(void)
{
    while (DL_SPI_isBusy(SPI1_INST)) {
    }
    DL_GPIO_setPins(ICM42688_PORT, ICM42688_CS_PIN);
}

static void ICM42688_ClearReceiveFifo(void)
{
    while (!DL_SPI_isRXFIFOEmpty(SPI1_INST)) {
        (void) DL_SPI_receiveData8(SPI1_INST);
    }
}

static uint8_t ICM42688_TransferByte(uint8_t transmitData)
{
    DL_SPI_transmitDataBlocking8(SPI1_INST, transmitData);
    return DL_SPI_receiveDataBlocking8(SPI1_INST);
}

static void ICM42688_WriteRegister(uint8_t address, uint8_t value)
{
    ICM42688_ClearReceiveFifo();
    ICM42688_ChipSelectLow();
    (void) ICM42688_TransferByte(address & ICM42688_SPI_WRITE_MASK);
    (void) ICM42688_TransferByte(value);
    ICM42688_ChipSelectHigh();
}

static uint8_t ICM42688_ReadRegister(uint8_t address)
{
    uint8_t value;

    ICM42688_ClearReceiveFifo();
    ICM42688_ChipSelectLow();
    (void) ICM42688_TransferByte(address | ICM42688_SPI_READ_BIT);
    value = ICM42688_TransferByte(ICM42688_SPI_DUMMY_BYTE);
    ICM42688_ChipSelectHigh();

    return value;
}

static void ICM42688_ReadRegisters(uint8_t startAddress, uint8_t *data, uint32_t length)
{
    uint32_t index;

    ICM42688_ClearReceiveFifo();
    ICM42688_ChipSelectLow();
    (void) ICM42688_TransferByte(startAddress | ICM42688_SPI_READ_BIT);
    for (index = 0U; index < length; index++) {
        data[index] = ICM42688_TransferByte(ICM42688_SPI_DUMMY_BYTE);
    }
    ICM42688_ChipSelectHigh();
}

static int16_t ICM42688_MakeInt16(uint8_t highByte, uint8_t lowByte)
{
    return (int16_t) (((uint16_t) highByte << 8U) | (uint16_t) lowByte);
}

uint8_t ICM42688_ReadDeviceId(void)
{
    return ICM42688_ReadRegister(ICM42688_REG_WHO_AM_I);
}

ICM42688_Status ICM42688_Init(void)
{
    ICM42688_ChipSelectHigh();
    ICM42688_DelayMs(10U);

    ICM42688_WriteRegister(ICM42688_REG_DEVICE_CONFIG, ICM42688_SOFT_RESET);
    ICM42688_DelayMs(50U);

    if (ICM42688_ReadDeviceId() != ICM42688_WHO_AM_I_VALUE) {
        return ICM42688_STATUS_BAD_DEVICE_ID;
    }

    ICM42688_WriteRegister(ICM42688_REG_PWR_MGMT0, ICM42688_ACCEL_GYRO_LOW_NOISE);
    ICM42688_DelayMs(2U);
    ICM42688_WriteRegister(ICM42688_REG_GYRO_CONFIG0, ICM42688_FS_1000DPS_ODR_1KHZ);
    ICM42688_WriteRegister(ICM42688_REG_ACCEL_CONFIG0, ICM42688_FS_8G_ODR_1KHZ);
    ICM42688_DelayMs(10U);

    if ((ICM42688_ReadRegister(ICM42688_REG_PWR_MGMT0) != ICM42688_ACCEL_GYRO_LOW_NOISE) ||
        (ICM42688_ReadRegister(ICM42688_REG_GYRO_CONFIG0) != ICM42688_FS_1000DPS_ODR_1KHZ) ||
        (ICM42688_ReadRegister(ICM42688_REG_ACCEL_CONFIG0) != ICM42688_FS_8G_ODR_1KHZ)) {
        return ICM42688_STATUS_CONFIG_VERIFY_FAILED;
    }

    ICM42688_SetGyroBias(0.0f, 0.0f, 0.0f);
    return ICM42688_STATUS_OK;
}

ICM42688_Status ICM42688_ReadRaw(ICM42688_RawData *data)
{
    uint8_t buffer[ICM42688_BURST_DATA_LENGTH];

    if (data == NULL) {
        return ICM42688_STATUS_INVALID_ARGUMENT;
    }

    ICM42688_ReadRegisters(ICM42688_REG_TEMP_DATA1, buffer, ICM42688_BURST_DATA_LENGTH);
    data->temperature = ICM42688_MakeInt16(buffer[0], buffer[1]);
    data->accelX = ICM42688_MakeInt16(buffer[2], buffer[3]);
    data->accelY = ICM42688_MakeInt16(buffer[4], buffer[5]);
    data->accelZ = ICM42688_MakeInt16(buffer[6], buffer[7]);
    data->gyroX = ICM42688_MakeInt16(buffer[8], buffer[9]);
    data->gyroY = ICM42688_MakeInt16(buffer[10], buffer[11]);
    data->gyroZ = ICM42688_MakeInt16(buffer[12], buffer[13]);

    return ICM42688_STATUS_OK;
}

ICM42688_Status ICM42688_Read(ICM42688_Data *data)
{
    ICM42688_RawData raw;
    ICM42688_Status status;

    if (data == NULL) {
        return ICM42688_STATUS_INVALID_ARGUMENT;
    }

    status = ICM42688_ReadRaw(&raw);
    if (status != ICM42688_STATUS_OK) {
        return status;
    }

    data->temperatureC = ((float) raw.temperature / ICM42688_TEMPERATURE_SCALE) +
                         ICM42688_TEMPERATURE_OFFSET_C;
    data->accelXG = (float) raw.accelX / ICM42688_ACCEL_LSB_PER_G;
    data->accelYG = (float) raw.accelY / ICM42688_ACCEL_LSB_PER_G;
    data->accelZG = (float) raw.accelZ / ICM42688_ACCEL_LSB_PER_G;
    data->gyroXDps = ((float) raw.gyroX / ICM42688_GYRO_LSB_PER_DPS) - gGyroBiasXDps;
    data->gyroYDps = ((float) raw.gyroY / ICM42688_GYRO_LSB_PER_DPS) - gGyroBiasYDps;
    data->gyroZDps = ((float) raw.gyroZ / ICM42688_GYRO_LSB_PER_DPS) - gGyroBiasZDps;

    return ICM42688_STATUS_OK;
}

ICM42688_Status ICM42688_CalibrateGyro(uint16_t sampleCount, uint16_t sampleIntervalMs)
{
    ICM42688_RawData raw;
    int64_t sumX = 0;
    int64_t sumY = 0;
    int64_t sumZ = 0;
    uint16_t index;

    if (sampleCount == 0U) {
        return ICM42688_STATUS_INVALID_ARGUMENT;
    }

    for (index = 0U; index < sampleCount; index++) {
        (void) ICM42688_ReadRaw(&raw);
        sumX += raw.gyroX;
        sumY += raw.gyroY;
        sumZ += raw.gyroZ;
        ICM42688_DelayMs(sampleIntervalMs);
    }

    gGyroBiasXDps = ((float) sumX / (float) sampleCount) / ICM42688_GYRO_LSB_PER_DPS;
    gGyroBiasYDps = ((float) sumY / (float) sampleCount) / ICM42688_GYRO_LSB_PER_DPS;
    gGyroBiasZDps = ((float) sumZ / (float) sampleCount) / ICM42688_GYRO_LSB_PER_DPS;

    return ICM42688_STATUS_OK;
}

void ICM42688_SetGyroBias(float xDps, float yDps, float zDps)
{
    gGyroBiasXDps = xDps;
    gGyroBiasYDps = yDps;
    gGyroBiasZDps = zDps;
}

void ICM42688_GetGyroBias(float *xDps, float *yDps, float *zDps)
{
    if (xDps != NULL) {
        *xDps = gGyroBiasXDps;
    }
    if (yDps != NULL) {
        *yDps = gGyroBiasYDps;
    }
    if (zDps != NULL) {
        *zDps = gGyroBiasZDps;
    }
}
