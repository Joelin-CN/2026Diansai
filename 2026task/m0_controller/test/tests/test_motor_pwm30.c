/**
 * @file test_motor_pwm30.c
 * @brief Simple 30% PWM motor test - based on validated reference
 * @date 2026-07-24
 *
 * This test directly sets PWM and direction pins to drive all motors at 30%.
 * No encoder, no control loop, just raw hardware control.
 */

#include <stdint.h>
#include "ti_msp_dl_config.h"

#define MOTOR_PWM_PERIOD_COUNTS       (1000U)
#define MOTOR_PWM_DUTY_PERCENT        (30U)
#define MOTOR_PWM_COMPARE_30_PERCENT  \
    (MOTOR_PWM_PERIOD_COUNTS - \
        ((MOTOR_PWM_PERIOD_COUNTS * MOTOR_PWM_DUTY_PERCENT) / 100U))

/**
 * @brief Simple UART output
 */
static void uart_print(const char *text)
{
    while (*text != '\0') {
        DL_UART_Main_transmitDataBlocking(UART0_INST, (uint8_t)*text++);
    }
}

/**
 * @brief Stop all motors
 */
static void motor_stop_all(void)
{
    /* Set PWM to 0% (compare = period) */
    DL_TimerA_setCaptureCompareValue(
        MOTOR_PWM_A_INST, MOTOR_PWM_PERIOD_COUNTS, GPIO_MOTOR_PWM_A_C0_IDX);
    DL_TimerA_setCaptureCompareValue(
        MOTOR_PWM_A_INST, MOTOR_PWM_PERIOD_COUNTS, GPIO_MOTOR_PWM_A_C1_IDX);
    DL_TimerA_setCaptureCompareValue(
        MOTOR_PWM_A_INST, MOTOR_PWM_PERIOD_COUNTS, GPIO_MOTOR_PWM_A_C3_IDX);
    DL_TimerA_setCaptureCompareValue(
        MOTOR_PWM_B_INST, MOTOR_PWM_PERIOD_COUNTS, GPIO_MOTOR_PWM_B_C1_IDX);

    /* Clear all direction pins */
    DL_GPIO_clearPins(MOTOR_DIR_M1_IN1_PORT, MOTOR_DIR_M1_IN1_PIN);
    DL_GPIO_clearPins(MOTOR_DIR_M1_IN2_PORT, MOTOR_DIR_M1_IN2_PIN);
    DL_GPIO_clearPins(MOTOR_DIR_M2_IN1_PORT, MOTOR_DIR_M2_IN1_PIN);
    DL_GPIO_clearPins(MOTOR_DIR_M2_IN2_PORT, MOTOR_DIR_M2_IN2_PIN);
    DL_GPIO_clearPins(MOTOR_DIR_M3_IN1_PORT, MOTOR_DIR_M3_IN1_PIN);
    DL_GPIO_clearPins(MOTOR_DIR_M3_IN2_PORT, MOTOR_DIR_M3_IN2_PIN);
    DL_GPIO_clearPins(MOTOR_DIR_M4_IN1_PORT, MOTOR_DIR_M4_IN1_PIN);
    DL_GPIO_clearPins(MOTOR_DIR_M4_IN2_PORT, MOTOR_DIR_M4_IN2_PIN);
}

/**
 * @brief Start all motors at 30% PWM in forward direction
 */
static void motor_start_all_forward_30_percent(void)
{
    /* Set direction pins for forward motion */
    /* M1: IN1=HIGH, IN2=LOW */
    DL_GPIO_setPins(MOTOR_DIR_M1_IN1_PORT, MOTOR_DIR_M1_IN1_PIN);
    DL_GPIO_clearPins(MOTOR_DIR_M1_IN2_PORT, MOTOR_DIR_M1_IN2_PIN);

    /* M2: IN1=HIGH, IN2=LOW */
    DL_GPIO_setPins(MOTOR_DIR_M2_IN1_PORT, MOTOR_DIR_M2_IN1_PIN);
    DL_GPIO_clearPins(MOTOR_DIR_M2_IN2_PORT, MOTOR_DIR_M2_IN2_PIN);

    /* M3: IN1=HIGH, IN2=LOW */
    DL_GPIO_setPins(MOTOR_DIR_M3_IN1_PORT, MOTOR_DIR_M3_IN1_PIN);
    DL_GPIO_clearPins(MOTOR_DIR_M3_IN2_PORT, MOTOR_DIR_M3_IN2_PIN);

    /* M4: IN1=HIGH, IN2=LOW */
    DL_GPIO_setPins(MOTOR_DIR_M4_IN1_PORT, MOTOR_DIR_M4_IN1_PIN);
    DL_GPIO_clearPins(MOTOR_DIR_M4_IN2_PORT, MOTOR_DIR_M4_IN2_PIN);

    /* Set PWM to 30% (compare = 700 for period 1000) */
    DL_TimerA_setCaptureCompareValue(
        MOTOR_PWM_A_INST, MOTOR_PWM_COMPARE_30_PERCENT, GPIO_MOTOR_PWM_A_C0_IDX);
    DL_TimerA_setCaptureCompareValue(
        MOTOR_PWM_A_INST, MOTOR_PWM_COMPARE_30_PERCENT, GPIO_MOTOR_PWM_A_C1_IDX);
    DL_TimerA_setCaptureCompareValue(
        MOTOR_PWM_A_INST, MOTOR_PWM_COMPARE_30_PERCENT, GPIO_MOTOR_PWM_A_C3_IDX);
    DL_TimerA_setCaptureCompareValue(
        MOTOR_PWM_B_INST, MOTOR_PWM_COMPARE_30_PERCENT, GPIO_MOTOR_PWM_B_C1_IDX);
}

/**
 * @brief Main test loop
 */
void test_motor_pwm30_main_loop(void)
{
    static bool initialized = false;

    if (!initialized) {
        /* Stop motors initially */
        motor_stop_all();

        /* Wait ~10ms */
        delay_cycles(320000U);

        /* Start all motors at 30% forward */
        motor_start_all_forward_30_percent();

        /* Print status */
        uart_print(
            "\r\n"
            "========================================\r\n"
            "TB6612 MOTOR 30% PWM TEST\r\n"
            "All four motors running at 30% PWM\r\n"
            "Forward direction (IN1=HIGH, IN2=LOW)\r\n"
            "No control loop, just fixed PWM\r\n"
            "========================================\r\n"
            "\r\n"
            "Motors should be running now.\r\n"
            "Press RESET to stop.\r\n"
            "\r\n");

        initialized = true;
    }

    /* Idle loop - motors keep running */
    __WFI();
}
