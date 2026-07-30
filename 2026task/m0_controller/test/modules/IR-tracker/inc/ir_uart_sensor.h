#ifndef IR_UART_SENSOR_H_
#define IR_UART_SENSOR_H_

#include <stdbool.h>
#include <stdint.h>

#define IR_UART_SENSOR_COUNT (8U)
#define IR_UART_SENSOR_FRAME_MAX (96U)

typedef enum {
    IR_UART_SENSOR_STATUS_OK = 0,
    IR_UART_SENSOR_STATUS_NO_FRAME,
    IR_UART_SENSOR_STATUS_BAD_FRAME,
    IR_UART_SENSOR_STATUS_OVERFLOW,
} ir_uart_sensor_status_t;

void IrUartSensor_Init(void);
void IrUartSensor_Reset(void);
void IrUartSensor_RequestAnalogMode(void);
void IrUartSensor_RequestDigitalMode(void);
void IrUartSensor_RxByte(uint8_t byte);
ir_uart_sensor_status_t IrUartSensor_Process(void);
bool IrUartSensor_GetAnalog(uint16_t values[IR_UART_SENSOR_COUNT]);
bool IrUartSensor_GetFrameReady(void);

#endif
