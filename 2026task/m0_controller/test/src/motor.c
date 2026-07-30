#include "motor.h"

#include "ti_msp_dl_config.h"

#define MOTOR_PWM_PERIOD (1000U)

static int16_t Motor_ClampSpeed(int16_t speed)
{
    if (speed > MOTOR_SPEED_MAX) return MOTOR_SPEED_MAX;
    if (speed < -MOTOR_SPEED_MAX) return -MOTOR_SPEED_MAX;
    return speed;
}

static void Motor_WritePin(GPIO_Regs *port, uint32_t pin, bool high)
{
    if (high) {
        DL_GPIO_setPins(port, pin);
    } else {
        DL_GPIO_clearPins(port, pin);
    }
}

static void Motor_SetOne(int16_t speed, bool reversed, GPIO_Regs *in1Port, uint32_t in1Pin,
                         GPIO_Regs *in2Port, uint32_t in2Pin, GPTIMER_Regs *pwm,
                         DL_TIMER_CC_INDEX pwmIndex)
{
    uint32_t duty;
    bool forward;

    speed = Motor_ClampSpeed(speed);
    forward = (speed >= 0);
    if (reversed) forward = !forward;

    if (speed < 0) {
        duty = (uint32_t)(-speed);
    } else {
        duty = (uint32_t)speed;
    }

    if (duty == 0U) {
        Motor_WritePin(in1Port, in1Pin, false);
        Motor_WritePin(in2Port, in2Pin, false);
    } else {
        Motor_WritePin(in1Port, in1Pin, forward);
        Motor_WritePin(in2Port, in2Pin, !forward);
    }

    /* SysConfig uses non-inverted edge PWM: compare = period - duty. */
    DL_TimerA_setCaptureCompareValue(pwm, MOTOR_PWM_PERIOD - duty, pwmIndex);
}

void Motor_Init(void)
{
    Motor_Stop();
}

void Motor_SetStandby(bool enabled)
{
    /* TB6612 STBY is permanently enabled by the hardware 3.3 V pull-up. */
    (void)enabled;
}

void Motor_SetFour(int16_t m1Speed, int16_t m2Speed, int16_t m3Speed, int16_t m4Speed)
{
    /* Software-to-hardware remapping:
     * User wants: Software M1→Physical M1, Software M2→Physical M2, etc.
     * But hardware wiring is: Hardware M1↔Physical M2, Hardware M2↔Physical M1
     *
     * Solution: Swap M1 and M2 in software
     * - Software M1 → Use hardware M2 pins → drives Physical M1 (left front)
     * - Software M2 → Use hardware M1 pins → drives Physical M2 (left rear)
     * - Software M3 → Use hardware M3 pins → drives Physical M3 (right rear)
     * - Software M4 → Use hardware M4 pins → drives Physical M4 (right front)
     */

    // Software M1 → Hardware M2 pins → Physical M1 (left front)
    Motor_SetOne(m1Speed, MOTOR_M1_REVERSED != 0, MOTOR_DIR_M2_IN1_PORT, MOTOR_DIR_M2_IN1_PIN,
                 MOTOR_DIR_M2_IN2_PORT, MOTOR_DIR_M2_IN2_PIN, MOTOR_PWM_A_INST,
                 GPIO_MOTOR_PWM_A_C1_IDX);

    // Software M2 → Hardware M1 pins → Physical M2 (left rear)
    Motor_SetOne(m2Speed, MOTOR_M2_REVERSED != 0, MOTOR_DIR_M1_IN1_PORT, MOTOR_DIR_M1_IN1_PIN,
                 MOTOR_DIR_M1_IN2_PORT, MOTOR_DIR_M1_IN2_PIN, MOTOR_PWM_A_INST,
                 GPIO_MOTOR_PWM_A_C0_IDX);

    // Software M3 → Hardware M3 pins → Physical M3 (right rear)
    Motor_SetOne(m3Speed, MOTOR_M3_REVERSED != 0, MOTOR_DIR_M3_IN1_PORT, MOTOR_DIR_M3_IN1_PIN,
                 MOTOR_DIR_M3_IN2_PORT, MOTOR_DIR_M3_IN2_PIN, MOTOR_PWM_B_INST,
                 GPIO_MOTOR_PWM_B_C1_IDX);

    // Software M4 → Hardware M4 pins → Physical M4 (right front)
    Motor_SetOne(m4Speed, MOTOR_M4_REVERSED != 0, MOTOR_DIR_M4_IN1_PORT, MOTOR_DIR_M4_IN1_PIN,
                 MOTOR_DIR_M4_IN2_PORT, MOTOR_DIR_M4_IN2_PIN, MOTOR_PWM_A_INST,
                 GPIO_MOTOR_PWM_A_C3_IDX);
}

void Motor_SetSpeed(int16_t leftSpeed, int16_t rightSpeed)
{
    Motor_SetFour(leftSpeed, leftSpeed, rightSpeed, rightSpeed);
}

void Motor_Stop(void)
{
    Motor_SetSpeed(0, 0);
}
