/**
 * @file ir_sensor_calibration.h
 * @brief IR传感器符号和配置验证工具接口
 * @date 2026-07-30
 */

#ifndef IR_SENSOR_CALIBRATION_H
#define IR_SENSOR_CALIBRATION_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 运行IR传感器校准程序
 *
 * 验证内容:
 *   1. 传感器原始值是否正常
 *   2. lateral_error 符号是否正确:
 *      - 小车向右偏 → lateral_error < 0
 *      - 小车向左偏 → lateral_error > 0
 *   3. 传感器覆盖范围测试
 */
void IrSensorCalibration_Run(void);

#ifdef __cplusplus
}
#endif

#endif /* IR_SENSOR_CALIBRATION_H */
