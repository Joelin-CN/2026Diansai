/**
 * @file ir_sensor_test.h
 * @brief IR sensor hardware test and verification
 * @date 2026-07-30
 */

#ifndef IR_SENSOR_TEST_H_
#define IR_SENSOR_TEST_H_

#include <stdint.h>

/**
 * @brief Run comprehensive IR sensor test
 *
 * Test sequence:
 * 1. Initialize IR sensor UART
 * 2. Request analog mode
 * 3. Poll and display 8-channel analog values for 10 seconds
 * 4. Verify frame reception and data validity
 *
 * Expected output format:
 * IR[0]=xxxx IR[1]=xxxx ... IR[7]=xxxx
 *
 * @note This function blocks for about 10 seconds
 */
void IrSensorTest_Run(void);

/**
 * @brief Continuous IR monitoring mode (call in main loop)
 *
 * Prints IR sensor values every 200ms
 * Use for real-time line tracking observation
 */
void IrSensorTest_Monitor(void);

#endif // IR_SENSOR_TEST_H_
