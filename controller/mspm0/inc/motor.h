#ifndef MOTOR_H_
#define MOTOR_H_

#include <stdbool.h>
#include <stdint.h>

#define MOTOR_SPEED_MAX (1000)

/* Change an individual value to 1 if that motor runs backwards. */
#define MOTOR_M1_REVERSED (0)
#define MOTOR_M2_REVERSED (0)
#define MOTOR_M3_REVERSED (0)
#define MOTOR_M4_REVERSED (0)

void Motor_Init(void);
void Motor_SetStandby(bool enabled);
void Motor_SetFour(int16_t m1Speed, int16_t m2Speed, int16_t m3Speed, int16_t m4Speed);
void Motor_SetSpeed(int16_t leftSpeed, int16_t rightSpeed);
void Motor_Stop(void);

#endif
