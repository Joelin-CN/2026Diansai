/**
 * @file test_motor_debug.c
 * @brief Motor test with debug information
 */

#include <stdio.h>
#include "ti_msp_dl_config.h"
#include "motor.h"

static void DelayMs(uint32_t ms)
{
    for (uint32_t i = 0; i < ms; i++) {
        for (volatile uint32_t j = 0; j < 8000; j++) {
        }
    }
}

static void uart_print(const char *text)
{
    while (*text != '\0') {
        while (DL_UART_isTXFIFOFull(UART0_INST));
        DL_UART_Main_transmitData(UART0_INST, (uint8_t)*text++);
    }
}

static void print_gpio_state(GPIO_Regs *port, uint32_t pin, const char *name)
{
    uint32_t val = DL_GPIO_readPins(port, pin);
    uart_print(name);
    uart_print(": ");
    uart_print(val ? "HIGH\r\n" : "LOW\r\n");
}

static void print_pwm_value(GPTIMER_Regs *timer, DL_TIMER_CC_INDEX idx, const char *name)
{
    uint32_t ccr = DL_TimerA_getCaptureCompareValue(timer, idx);
    char buf[64];
    sprintf(buf, "%s CCR=%u (duty=%u%%)\r\n", name, (unsigned)ccr,
            (unsigned)((1000 - ccr) / 10));
    uart_print(buf);
}

void test_motor_debug_main_loop(void)
{
    static bool initialized = false;

    if (!initialized) {
        uart_print("\r\n=== MOTOR DEBUG TEST ===\r\n\r\n");

        /* Initialize motor */
        Motor_Init();
        uart_print("Motor_Init() complete\r\n\r\n");

        /* Set motors to 30% */
        Motor_SetFour(300, 300, 300, 300);
        uart_print("Motor_SetFour(300, 300, 300, 300) called\r\n\r\n");

        /* Wait a bit for settings to take effect */
        DelayMs(100);

        /* Read back PWM values */
        uart_print("--- PWM Values ---\r\n");
        print_pwm_value(MOTOR_PWM_A_INST, GPIO_MOTOR_PWM_A_C0_IDX, "M1 PWM");
        print_pwm_value(MOTOR_PWM_A_INST, GPIO_MOTOR_PWM_A_C1_IDX, "M2 PWM");
        print_pwm_value(MOTOR_PWM_B_INST, GPIO_MOTOR_PWM_B_C1_IDX, "M3 PWM");
        print_pwm_value(MOTOR_PWM_A_INST, GPIO_MOTOR_PWM_A_C3_IDX, "M4 PWM");
        uart_print("\r\n");

        /* Read back GPIO direction pins */
        uart_print("--- Direction Pins ---\r\n");
        print_gpio_state(MOTOR_DIR_M1_IN1_PORT, MOTOR_DIR_M1_IN1_PIN, "M1_IN1");
        print_gpio_state(MOTOR_DIR_M1_IN2_PORT, MOTOR_DIR_M1_IN2_PIN, "M1_IN2");
        print_gpio_state(MOTOR_DIR_M2_IN1_PORT, MOTOR_DIR_M2_IN1_PIN, "M2_IN1");
        print_gpio_state(MOTOR_DIR_M2_IN2_PORT, MOTOR_DIR_M2_IN2_PIN, "M2_IN2");
        print_gpio_state(MOTOR_DIR_M3_IN1_PORT, MOTOR_DIR_M3_IN1_PIN, "M3_IN1");
        print_gpio_state(MOTOR_DIR_M3_IN2_PORT, MOTOR_DIR_M3_IN2_PIN, "M3_IN2");
        print_gpio_state(MOTOR_DIR_M4_IN1_PORT, MOTOR_DIR_M4_IN1_PIN, "M4_IN1");
        print_gpio_state(MOTOR_DIR_M4_IN2_PORT, MOTOR_DIR_M4_IN2_PIN, "M4_IN2");
        uart_print("\r\n");

        uart_print("=== Check the values above ===\r\n");
        uart_print("Expected:\r\n");
        uart_print("  PWM CCR should be 700 (30%% duty)\r\n");
        uart_print("  IN1 should be HIGH, IN2 should be LOW\r\n");
        uart_print("\r\n");

        initialized = true;
    }

    __WFI();
}
