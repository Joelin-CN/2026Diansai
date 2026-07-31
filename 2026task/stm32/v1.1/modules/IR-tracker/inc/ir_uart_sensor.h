#ifndef IR_UART_SENSOR_H_
#define IR_UART_SENSOR_H_

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief IR传感器阵列通道数
 *
 * @category D: 硬件约束参数（固定）
 *
 * @value 8
 *
 * @origin 硬件规格
 *   - 使用8路红外传感器阵列模块
 *   - 传感器排列成一条直线
 *   - 用于循迹和路径识别
 *
 * @warnings
 *   - 更换不同通道数的传感器阵列时需要修改此值
 *   - 同时需要更新config.c中的ir_weights数组大小
 */
#define IR_UART_SENSOR_COUNT (8U)

/**
 * @brief UART接收帧缓冲区最大长度
 *
 * @category D: 硬件约束参数（协议定义）
 *
 * @value 96字节
 *
 * @origin 基于IR传感器模块UART协议
 *   - 典型帧格式: $A,x1:nnn,x2:nnn,...,x8:nnn#
 *   - 每个通道约10字节
 *   - 8通道 × 10字节 + 协议开销 ≈ 96字节
 *   - 留有一定余量
 *
 * @warnings
 *   - 如果帧格式变化需要重新计算此值
 *   - 过小会导致帧溢出错误
 *   - 过大会浪费RAM
 */
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
