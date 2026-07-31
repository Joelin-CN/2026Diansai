/**
 * @file      track_demo.c
 * @brief     Demo application for track path following
 * @author    Claude (Kiro)
 * @version   1.0.0
 * @date      2026-07-30
 * @note      Example usage of track control app in FreeRTOS task
 *
 * Track geometry:
 *   - 2 semicircles: radius = 0.5m
 *   - 2 straights: length = 1.5m
 *   - Total perimeter ≈ 6.14m
 *
 * Usage:
 *   1. Replace StartDefaultTask() in freertos.c with this code
 *   2. Set target laps (1-10)
 *   3. Compile and flash to STM32F407
 *   4. Place vehicle on track at start position (semicircle 1 entry point)
 *   5. Vehicle will automatically follow track for specified laps
 */

#include "track_control_app.h"
#include "cmsis_os.h"
#include "usart.h"
#include <stdio.h>

/* USART2 handle from STM32CubeMX generated code */
extern UART_HandleTypeDef huart2;

/**
 * @brief FreeRTOS default task using track path
 * @param argument Unused
 *
 * This replaces the StartDefaultTask() in freertos.c
 */
void StartDefaultTask(void *argument)
{
    printf("\n\n");
    printf("========================================\n");
    printf("  Track Path Following Demo\n");
    printf("========================================\n");
    printf("Track: 2 semicircles (R=0.5m) + 2 straights (L=1.5m)\n");
    printf("Perimeter: ~6.14m\n");
    printf("\n");

    /* Initialize track control application with target laps */
    const uint8_t TARGET_LAPS = 3;  /* Adjust this for different lap counts */

    if (!TrackControlApp_Init(TARGET_LAPS)) {
        printf("[FATAL] TrackControlApp_Init failed\n");
        printf("System halted. Check:\n");
        printf("  1. Motor connections (TB6612)\n");
        printf("  2. IR sensor UART (USART2)\n");
        printf("  3. Encoders (TIM3, TIM4)\n");
        printf("  4. IMU SPI (optional)\n");
        for (;;) {
            osDelay(1000);
        }
    }

    /* IMPORTANT: Defensive fix for USART2 RX interrupt
     * printf() may have disabled RXNEIE during initialization.
     * Re-enable it to ensure IR sensor data reception.
     */
    SET_BIT(huart2.Instance->CR1, USART_CR1_RXNEIE);
    printf("[INFO] USART2 RXNEIE re-enabled after initialization\n");

    printf("\n");
    printf("========================================\n");
    printf("  Control Loop Running (500 Hz)\n");
    printf("========================================\n");
    printf("Target laps: %u\n", TARGET_LAPS);
    printf("Place vehicle at track start position...\n");
    printf("\n");

    /* Main control loop: 500 Hz (2ms per cycle) */
    uint32_t cycle_count = 0;
    uint8_t last_reported_lap = 0;

    for (;;) {
        /* Execute one control cycle */
        TrackControlApp_RunFastCycle();

        /* Report progress every 5 seconds (2500 cycles at 500Hz) */
        cycle_count++;
        if (cycle_count % 2500 == 0) {
            uint8_t current_lap = TrackControlApp_GetCompletedLaps();
            if (current_lap != last_reported_lap) {
                printf("[Progress] Completed laps: %u / %u\n",
                       current_lap, TARGET_LAPS);
                last_reported_lap = current_lap;
            }
        }

        /* Check if target reached */
        if (TrackControlApp_IsComplete()) {
            printf("\n");
            printf("========================================\n");
            printf("  Mission Complete!\n");
            printf("========================================\n");
            printf("Completed %u laps successfully\n", TARGET_LAPS);
            printf("Vehicle stopped.\n");
            printf("\n");

            /* Halt after completion */
            for (;;) {
                osDelay(1000);
            }
        }

        /* FreeRTOS delay: 2ms = 500Hz */
        osDelay(2);
    }
}

/* ============================================================================
 * Alternative: Adjustable Parameters Version
 * ============================================================================
 *
 * If you need to tune parameters during runtime, replace the above function
 * with this version that allows easy parameter adjustment:
 */

#if 0  /* Set to 1 to use this version instead */

void StartDefaultTask_Tunable(void *argument)
{
    printf("\n=== Track Path Following (Tunable) ===\n\n");

    /* Tunable parameters */
    struct {
        uint8_t target_laps;
        float line_speed_mps;
        float curve_speed_mps;
        float lateral_gain;
        float heading_gain;
    } params = {
        .target_laps = 3,
        .line_speed_mps = 0.5f,   /* Start conservative */
        .curve_speed_mps = 0.3f,
        .lateral_gain = 1.5f,
        .heading_gain = 1.0f
    };

    printf("Initial parameters:\n");
    printf("  Target laps: %u\n", params.target_laps);
    printf("  Line speed: %.2f m/s\n", params.line_speed_mps);
    printf("  Curve speed: %.2f m/s\n", params.curve_speed_mps);
    printf("  Lateral gain: %.2f\n", params.lateral_gain);
    printf("  Heading gain: %.2f\n", params.heading_gain);
    printf("\n");

    if (!TrackControlApp_Init(params.target_laps)) {
        printf("[FATAL] Initialization failed\n");
        for (;;) { osDelay(1000); }
    }

    SET_BIT(huart2.Instance->CR1, USART_CR1_RXNEIE);

    /* TODO: Add runtime parameter adjustment via UART commands */

    for (;;) {
        TrackControlApp_RunFastCycle();

        if (TrackControlApp_IsComplete()) {
            printf("[Complete] Target reached!\n");
            for (;;) { osDelay(1000); }
        }

        osDelay(2);
    }
}

#endif
