#ifndef MOTOR_H_
#define MOTOR_H_

#include <stdbool.h>
#include <stdint.h>

#define MOTOR_SPEED_MAX (1000)

/* Change an individual value to 1 if that motor runs backwards. */
/* After software remapping in motor.c:
 * Software M1 → Physical M1 (left front)  - correct direction ✓
 * Software M2 → Physical M2 (left rear)   - needs flip ✓
 * Software M3 → Physical M3 (right rear)  - correct direction ✓
 * Software M4 → Physical M4 (right front) - correct direction ✓
 */
#define MOTOR_M1_REVERSED (0)  // Software M1 → Physical M1 (left front) → correct
#define MOTOR_M2_REVERSED (1)  // Software M2 → Physical M2 (left rear) → needs reverse
#define MOTOR_M3_REVERSED (1)  // Software M3 → Physical M3 (right rear) → needs reverse
#define MOTOR_M4_REVERSED (0)  // Software M4 → Physical M4 (right front) → correct

void Motor_Init(void);
void Motor_SetStandby(bool enabled);
void Motor_SetFour(int16_t m1Speed, int16_t m2Speed, int16_t m3Speed, int16_t m4Speed);
void Motor_SetSpeed(int16_t leftSpeed, int16_t rightSpeed);
void Motor_Stop(void);

#endif
