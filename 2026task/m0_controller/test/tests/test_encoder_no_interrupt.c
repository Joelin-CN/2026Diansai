/**
 * @file test_encoder_no_interrupt.c
 * @brief Test encoder reading WITHOUT interrupts
 * @date 2026-07-27
 */

#include "encoder.h"
#include "motor.h"
#include "ti_msp_dl_config.h"
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#define TEST_SPEED 500  // 50% PWM

static uint32_t g_loopCounter = 0;
static bool g_initialized = false;

/* Direct encoder count variables - copied from encoder.c */
static volatile int32_t g_localEncoderCount[4] = {0, 0, 0, 0};

/**
 * @brief Simple delay using busy loop
 */
static void SimpleDelay(uint32_t count)
{
    for (volatile uint32_t i = 0; i < count; i++) {
        /* Busy wait */
    }
}

/**
 * @brief Initialize encoder WITHOUT enabling interrupts
 */
static void Encoder_Init_NoInterrupt(void)
{
    /* Initialize counts to zero */
    for (int i = 0; i < 4; i++) {
        g_localEncoderCount[i] = 0;
    }

    /* DO NOT enable interrupts - this is the key difference */
    /* NVIC_EnableIRQ(ENCODER_GPIOA_INT_IRQN); */
    /* NVIC_EnableIRQ(ENCODER_GPIOB_INT_IRQN); */

    printf("Encoder initialized WITHOUT interrupts\n");
}

/**
 * @brief Main test loop - encoder init but NO interrupts
 */
void test_encoder_no_interrupt_main_loop(void)
{
    g_loopCounter++;

    /* Initialize once */
    if (!g_initialized) {
        printf("\n=== Encoder Test Without Interrupts ===\n");
        printf("Testing if interrupt is the problem\n");
        printf("Initializing...\n");

        Motor_Init();
        Encoder_Init_NoInterrupt();  /* Custom init without interrupts */

        printf("Starting M1 motor at 50%% PWM...\n");
        Motor_SetFour(TEST_SPEED, 0, 0, 0);

        g_initialized = true;

        printf("Initialization complete. Starting loop...\n");
        return;
    }

    /* Print status every 1000 loops */
    if ((g_loopCounter % 1000) == 0) {
        /* Read encoder counts directly - they won't change without interrupts */
        int32_t enc1 = Encoder_GetCount(ENCODER_M1);

        printf("[Loop %lu] Enc M1=%ld (should be 0 without interrupts)\n",
               (unsigned long)g_loopCounter, (long)enc1);
    }

    /* Stop after 5000 loops */
    if (g_loopCounter >= 5000) {
        Motor_SetFour(0, 0, 0, 0);
        printf("\n=== Test Complete ===\n");
        printf("Total loops: %lu\n", (unsigned long)g_loopCounter);
        printf("\n");
        printf("RESULT: If you see this message:\n");
        printf("  - Main loop works fine\n");
        printf("  - Encoder_GetCount() works fine\n");
        printf("  - Problem is the GPIO INTERRUPTS!\n");
        printf("\n");
        printf("SOLUTION: Check encoder interrupt configuration\n");
        printf("  - GPIO interrupt trigger mode (edge/level)\n");
        printf("  - Interrupt priority\n");
        printf("  - Signal debouncing\n");

        /* Stay in done state */
        for (;;) {
            /* Halt */
        }
    }

    /* Simple delay - roughly 1ms at 32MHz */
    SimpleDelay(8000);
}
