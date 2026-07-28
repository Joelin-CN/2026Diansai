/**
 * @file test_encoder_polling.c
 * @brief Polling-based encoder test - avoids GPIO interrupt storm
 *
 * This test uses Encoder_Poll() instead of GPIO interrupts to read encoders.
 * Encoder_Poll() is called at 1kHz from the main loop.
 */

#include "ti_msp_dl_config.h"
#include "motor.h"
#include "encoder.h"
#include <stdio.h>
#include <string.h>

/* Test state machine */
typedef enum {
    STATE_IDLE,
    STATE_M1_FWD,
    STATE_M1_REV,
    STATE_M2_FWD,
    STATE_M2_REV,
    STATE_M3_FWD,
    STATE_M3_REV,
    STATE_M4_FWD,
    STATE_M4_REV,
    STATE_DONE
} TestState;

static volatile TestState g_state = STATE_IDLE;
static volatile uint32_t g_stateStartMs = 0;
static volatile uint32_t g_systemMs = 0;
static volatile uint32_t g_lastPollMs = 0;

/* 1ms tick from SysTick */
void SysTick_Handler(void)
{
    g_systemMs++;
}

/* Call Encoder_Poll() at 1kHz */
static void EncoderPollTask(void)
{
    if (g_systemMs - g_lastPollMs >= 1) {
        g_lastPollMs = g_systemMs;
        Encoder_Poll();
    }
}

int main(void)
{
    SYSCFG_DL_init();

    /* Configure SysTick for 1ms tick @ 32MHz */
    SysTick_Config(32000000 / 1000);

    /* Initialize subsystems */
    Motor_Init();
    Encoder_Init();  /* Does NOT enable GPIO interrupts anymore */

    printf("\n\n");
    printf("Polling-Based Encoder Test\n");
    printf("===========================\n");
    printf("Board: MSPM0G3507\n");
    printf("Encoder Poll Rate: 1kHz\n");
    printf("\n");

    printf("Motor driver initialized\n");
    printf("Encoder polling ready\n");
    printf("\n");

    /* Wait 500ms for stabilization */
    delay_cycles(16000000);

    g_state = STATE_M1_FWD;
    g_stateStartMs = g_systemMs;

    while (1) {
        /* Call encoder polling task at 1kHz */
        EncoderPollTask();

        const uint32_t elapsedMs = g_systemMs - g_stateStartMs;

        /* State machine - each motor runs for 2 seconds per direction */
        switch (g_state) {
            case STATE_IDLE:
                break;

            case STATE_M1_FWD:
                if (elapsedMs == 0) {
                    Motor_SetSpeed(MOTOR_M1, 50);
                    Encoder_ResetCount(ENCODER_M1);
                    printf("=== M1 Forward @ 50%% ===\n");
                }
                if (elapsedMs >= 2000) {
                    int32_t count = Encoder_GetCount(ENCODER_M1);
                    printf("M1 FWD: %ld counts in 2000ms\n", (long)count);
                    Motor_SetSpeed(MOTOR_M1, 0);
                    g_state = STATE_M1_REV;
                    g_stateStartMs = g_systemMs;
                }
                break;

            case STATE_M1_REV:
                if (elapsedMs == 0) {
                    Motor_SetSpeed(MOTOR_M1, -50);
                    Encoder_ResetCount(ENCODER_M1);
                    printf("=== M1 Reverse @ -50%% ===\n");
                }
                if (elapsedMs >= 2000) {
                    int32_t count = Encoder_GetCount(ENCODER_M1);
                    printf("M1 REV: %ld counts in 2000ms\n", (long)count);
                    Motor_SetSpeed(MOTOR_M1, 0);
                    g_state = STATE_M2_FWD;
                    g_stateStartMs = g_systemMs;
                }
                break;

            case STATE_M2_FWD:
                if (elapsedMs == 0) {
                    Motor_SetSpeed(MOTOR_M2, 50);
                    Encoder_ResetCount(ENCODER_M2);
                    printf("=== M2 Forward @ 50%% ===\n");
                }
                if (elapsedMs >= 2000) {
                    int32_t count = Encoder_GetCount(ENCODER_M2);
                    printf("M2 FWD: %ld counts in 2000ms\n", (long)count);
                    Motor_SetSpeed(MOTOR_M2, 0);
                    g_state = STATE_M2_REV;
                    g_stateStartMs = g_systemMs;
                }
                break;

            case STATE_M2_REV:
                if (elapsedMs == 0) {
                    Motor_SetSpeed(MOTOR_M2, -50);
                    Encoder_ResetCount(ENCODER_M2);
                    printf("=== M2 Reverse @ -50%% ===\n");
                }
                if (elapsedMs >= 2000) {
                    int32_t count = Encoder_GetCount(ENCODER_M2);
                    printf("M2 REV: %ld counts in 2000ms\n", (long)count);
                    Motor_SetSpeed(MOTOR_M2, 0);
                    g_state = STATE_M3_FWD;
                    g_stateStartMs = g_systemMs;
                }
                break;

            case STATE_M3_FWD:
                if (elapsedMs == 0) {
                    Motor_SetSpeed(MOTOR_M3, 50);
                    Encoder_ResetCount(ENCODER_M3);
                    printf("=== M3 Forward @ 50%% ===\n");
                }
                if (elapsedMs >= 2000) {
                    int32_t count = Encoder_GetCount(ENCODER_M3);
                    printf("M3 FWD: %ld counts in 2000ms\n", (long)count);
                    Motor_SetSpeed(MOTOR_M3, 0);
                    g_state = STATE_M3_REV;
                    g_stateStartMs = g_systemMs;
                }
                break;

            case STATE_M3_REV:
                if (elapsedMs == 0) {
                    Motor_SetSpeed(MOTOR_M3, -50);
                    Encoder_ResetCount(ENCODER_M3);
                    printf("=== M3 Reverse @ -50%% ===\n");
                }
                if (elapsedMs >= 2000) {
                    int32_t count = Encoder_GetCount(ENCODER_M3);
                    printf("M3 REV: %ld counts in 2000ms\n", (long)count);
                    Motor_SetSpeed(MOTOR_M3, 0);
                    g_state = STATE_M4_FWD;
                    g_stateStartMs = g_systemMs;
                }
                break;

            case STATE_M4_FWD:
                if (elapsedMs == 0) {
                    Motor_SetSpeed(MOTOR_M4, 50);
                    Encoder_ResetCount(ENCODER_M4);
                    printf("=== M4 Forward @ 50%% ===\n");
                }
                if (elapsedMs >= 2000) {
                    int32_t count = Encoder_GetCount(ENCODER_M4);
                    printf("M4 FWD: %ld counts in 2000ms\n", (long)count);
                    Motor_SetSpeed(MOTOR_M4, 0);
                    g_state = STATE_M4_REV;
                    g_stateStartMs = g_systemMs;
                }
                break;

            case STATE_M4_REV:
                if (elapsedMs == 0) {
                    Motor_SetSpeed(MOTOR_M4, -50);
                    Encoder_ResetCount(ENCODER_M4);
                    printf("=== M4 Reverse @ -50%% ===\n");
                }
                if (elapsedMs >= 2000) {
                    int32_t count = Encoder_GetCount(ENCODER_M4);
                    printf("M4 REV: %ld counts in 2000ms\n", (long)count);
                    Motor_SetSpeed(MOTOR_M4, 0);
                    g_state = STATE_DONE;
                    g_stateStartMs = g_systemMs;
                }
                break;

            case STATE_DONE:
                if (elapsedMs == 0) {
                    printf("\n=== Test Complete ===\n");
                    printf("All motors tested with polling-based encoder reading.\n");
                }
                /* Stay in DONE state forever */
                break;
        }

        /* Debug output every 500ms */
        static uint32_t lastDebugMs = 0;
        if (g_systemMs - lastDebugMs >= 500) {
            lastDebugMs = g_systemMs;
            int32_t c1 = Encoder_GetCount(ENCODER_M1);
            int32_t c2 = Encoder_GetCount(ENCODER_M2);
            int32_t c3 = Encoder_GetCount(ENCODER_M3);
            int32_t c4 = Encoder_GetCount(ENCODER_M4);
            printf("[%lu ms] State=%d, Counts: M1=%ld M2=%ld M3=%ld M4=%ld\n",
                   (unsigned long)g_systemMs, (int)g_state,
                   (long)c1, (long)c2, (long)c3, (long)c4);
        }
    }
}
