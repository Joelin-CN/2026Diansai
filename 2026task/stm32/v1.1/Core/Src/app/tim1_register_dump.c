/**
 * @file tim1_register_dump.c
 * @brief TIM1 register dump and direct control test
 */

#include "tim1_register_dump.h"
#include "stm32f4xx_hal.h"
#include "tim.h"
#include <stdio.h>

void TIM1_DumpRegisters(void) {
    printf("\n");
    printf("========================================\n");
    printf("  TIM1 Register Dump\n");
    printf("========================================\n");
    printf("\n");

    printf("[Control Registers]\n");
    printf("  CR1   = 0x%04X\n", (unsigned int)TIM1->CR1);
    printf("    CEN (bit 0) = %u (Counter Enable: %s)\n",
           (TIM1->CR1 & TIM_CR1_CEN) ? 1 : 0,
           (TIM1->CR1 & TIM_CR1_CEN) ? "ENABLED" : "DISABLED");
    printf("    ARPE (bit 7) = %u (Auto-reload preload)\n",
           (TIM1->CR1 & TIM_CR1_ARPE) ? 1 : 0);
    printf("\n");

    printf("  BDTR  = 0x%04X (Break and Dead-Time Register)\n", (unsigned int)TIM1->BDTR);
    printf("    MOE (bit 15) = %u (Main Output Enable: %s) **CRITICAL**\n",
           (TIM1->BDTR & TIM_BDTR_MOE) ? 1 : 0,
           (TIM1->BDTR & TIM_BDTR_MOE) ? "ENABLED" : "DISABLED");
    printf("    OSSR (bit 11) = %u (Off-state selection for Run)\n",
           (TIM1->BDTR & TIM_BDTR_OSSR) ? 1 : 0);
    printf("    OSSI (bit 10) = %u (Off-state selection for Idle)\n",
           (TIM1->BDTR & TIM_BDTR_OSSI) ? 1 : 0);
    printf("\n");

    printf("[Timing Registers]\n");
    printf("  PSC   = %u (Prescaler)\n", (unsigned int)TIM1->PSC);
    printf("  ARR   = %u (Auto-Reload Register / Period)\n", (unsigned int)TIM1->ARR);
    printf("  CNT   = %u (Counter)\n", (unsigned int)TIM1->CNT);
    printf("  Calculated PWM Freq = 168MHz / ((PSC+1) * (ARR+1))\n");
    printf("                      = 168000000 / ((%u+1) * (%u+1))\n",
           (unsigned int)TIM1->PSC, (unsigned int)TIM1->ARR);
    uint32_t freq = 168000000 / ((TIM1->PSC + 1) * (TIM1->ARR + 1));
    printf("                      = %lu Hz\n", freq);
    printf("\n");

    printf("[Channel 1 - Left Motor (PE9)]\n");
    printf("  CCMR1 = 0x%04X\n", (unsigned int)TIM1->CCMR1);
    printf("    OC1M (bits 6:4) = 0x%X (Output Compare Mode)\n",
           (TIM1->CCMR1 >> 4) & 0x7);
    printf("      (0x6 = PWM mode 1, 0x7 = PWM mode 2)\n");
    printf("    OC1PE (bit 3) = %u (Output Compare Preload Enable)\n",
           (TIM1->CCMR1 & TIM_CCMR1_OC1PE) ? 1 : 0);
    printf("  CCER  = 0x%04X\n", (unsigned int)TIM1->CCER);
    printf("    CC1E (bit 0) = %u (Capture/Compare 1 output enable: %s)\n",
           (TIM1->CCER & TIM_CCER_CC1E) ? 1 : 0,
           (TIM1->CCER & TIM_CCER_CC1E) ? "ENABLED" : "DISABLED");
    printf("    CC1P (bit 1) = %u (Polarity: %s)\n",
           (TIM1->CCER & TIM_CCER_CC1P) ? 1 : 0,
           (TIM1->CCER & TIM_CCER_CC1P) ? "ACTIVE LOW" : "ACTIVE HIGH");
    printf("  CCR1  = %u (Capture/Compare value: %u%%)\n",
           (unsigned int)TIM1->CCR1,
           (unsigned int)(TIM1->CCR1 * 100 / (TIM1->ARR + 1)));
    printf("\n");

    printf("[Channel 2 - Right Motor (PE11)]\n");
    printf("  CCMR1 = 0x%04X\n", (unsigned int)TIM1->CCMR1);
    printf("    OC2M (bits 14:12) = 0x%X (Output Compare Mode)\n",
           (TIM1->CCMR1 >> 12) & 0x7);
    printf("    OC2PE (bit 11) = %u (Output Compare Preload Enable)\n",
           (TIM1->CCMR1 & TIM_CCMR1_OC2PE) ? 1 : 0);
    printf("  CCER  = 0x%04X\n", (unsigned int)TIM1->CCER);
    printf("    CC2E (bit 4) = %u (Capture/Compare 2 output enable: %s)\n",
           (TIM1->CCER & TIM_CCER_CC2E) ? 1 : 0,
           (TIM1->CCER & TIM_CCER_CC2E) ? "ENABLED" : "DISABLED");
    printf("    CC2P (bit 5) = %u (Polarity: %s)\n",
           (TIM1->CCER & TIM_CCER_CC2P) ? 1 : 0,
           (TIM1->CCER & TIM_CCER_CC2P) ? "ACTIVE LOW" : "ACTIVE HIGH");
    printf("  CCR2  = %u (Capture/Compare value: %u%%)\n",
           (unsigned int)TIM1->CCR2,
           (unsigned int)(TIM1->CCR2 * 100 / (TIM1->ARR + 1)));
    printf("\n");

    printf("[GPIO Configuration - PE9/PE11]\n");
    printf("  GPIOE->MODER  = 0x%08lX\n", GPIOE->MODER);
    printf("    PE9  mode (bits 19:18) = 0x%lX ", (GPIOE->MODER >> 18) & 0x3);
    switch ((GPIOE->MODER >> 18) & 0x3) {
        case 0: printf("(Input)\n"); break;
        case 1: printf("(Output)\n"); break;
        case 2: printf("(Alternate Function) **CORRECT**\n"); break;
        case 3: printf("(Analog)\n"); break;
    }
    printf("    PE11 mode (bits 23:22) = 0x%lX ", (GPIOE->MODER >> 22) & 0x3);
    switch ((GPIOE->MODER >> 22) & 0x3) {
        case 0: printf("(Input)\n"); break;
        case 1: printf("(Output)\n"); break;
        case 2: printf("(Alternate Function) **CORRECT**\n"); break;
        case 3: printf("(Analog)\n"); break;
    }
    printf("  GPIOE->AFR[1] = 0x%08lX (Alternate Function High Register)\n", GPIOE->AFR[1]);
    printf("    PE9  AF (bits 7:4)   = 0x%lX (should be 0x1 for TIM1)\n", (GPIOE->AFR[1] >> 4) & 0xF);
    printf("    PE11 AF (bits 15:12) = 0x%lX (should be 0x1 for TIM1)\n", (GPIOE->AFR[1] >> 12) & 0xF);
    printf("\n");

    printf("[RCC - Clock Enable]\n");
    printf("  RCC->APB2ENR = 0x%08lX\n", RCC->APB2ENR);
    printf("    TIM1EN (bit 0) = %lu (TIM1 Clock: %s)\n",
           (RCC->APB2ENR & RCC_APB2ENR_TIM1EN) ? 1UL : 0UL,
           (RCC->APB2ENR & RCC_APB2ENR_TIM1EN) ? "ENABLED" : "DISABLED");
    printf("  RCC->AHB1ENR = 0x%08lX\n", RCC->AHB1ENR);
    printf("    GPIOEEN (bit 4) = %lu (GPIOE Clock: %s)\n",
           (RCC->AHB1ENR & RCC_AHB1ENR_GPIOEEN) ? 1UL : 0UL,
           (RCC->AHB1ENR & RCC_AHB1ENR_GPIOEEN) ? "ENABLED" : "DISABLED");
    printf("\n");

    printf("[Critical Checklist]\n");
    if (!(TIM1->BDTR & TIM_BDTR_MOE)) {
        printf("  ❌ MOE is DISABLED - PWM will NOT output!\n");
    } else {
        printf("  ✅ MOE is ENABLED\n");
    }
    if (!(TIM1->CR1 & TIM_CR1_CEN)) {
        printf("  ❌ Counter is DISABLED\n");
    } else {
        printf("  ✅ Counter is ENABLED\n");
    }
    if (!(TIM1->CCER & TIM_CCER_CC1E)) {
        printf("  ❌ Channel 1 output is DISABLED\n");
    } else {
        printf("  ✅ Channel 1 output is ENABLED\n");
    }
    if (!(TIM1->CCER & TIM_CCER_CC2E)) {
        printf("  ❌ Channel 2 output is DISABLED\n");
    } else {
        printf("  ✅ Channel 2 output is ENABLED\n");
    }
    if (((GPIOE->MODER >> 18) & 0x3) != 2) {
        printf("  ❌ PE9 is NOT in Alternate Function mode\n");
    } else {
        printf("  ✅ PE9 is in Alternate Function mode\n");
    }
    if (((GPIOE->MODER >> 22) & 0x3) != 2) {
        printf("  ❌ PE11 is NOT in Alternate Function mode\n");
    } else {
        printf("  ✅ PE11 is in Alternate Function mode\n");
    }

    printf("\n");
    printf("========================================\n");
    printf("\n");
}

void TIM1_ForceTest(void) {
    printf("\n");
    printf("========================================\n");
    printf("  TIM1 Force PWM Test\n");
    printf("========================================\n");
    printf("\n");

    printf("Manually forcing TIM1 configuration...\n");

    // 1. Enable clocks
    __HAL_RCC_TIM1_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    printf("  [1/6] Clocks enabled\n");

    // 2. Configure GPIO
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_9 | GPIO_PIN_11;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF1_TIM1;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
    printf("  [2/6] GPIO configured (PE9, PE11 as AF1)\n");

    // 3. Set prescaler and period
    TIM1->PSC = 0;
    TIM1->ARR = 8399;
    printf("  [3/6] PSC=0, ARR=8399 (20kHz PWM)\n");

    // 4. Configure PWM mode
    TIM1->CCMR1 = (0x6 << 4) | (1 << 3) |  // CH1: PWM mode 1, preload enable
                  (0x6 << 12) | (1 << 11); // CH2: PWM mode 1, preload enable
    printf("  [4/6] PWM mode 1 configured\n");

    // 5. Set duty cycle (50%)
    TIM1->CCR1 = 4200;  // 50%
    TIM1->CCR2 = 4200;  // 50%
    printf("  [5/6] CCR1=CCR2=4200 (50%% duty)\n");

    // 6. Enable outputs
    TIM1->CCER = TIM_CCER_CC1E | TIM_CCER_CC2E;  // Enable CH1 and CH2 outputs
    TIM1->BDTR |= TIM_BDTR_MOE;                   // Enable main output
    TIM1->CR1 |= TIM_CR1_CEN | TIM_CR1_ARPE;      // Enable counter and auto-reload
    printf("  [6/6] Outputs enabled, MOE set, counter started\n");

    printf("\n");
    printf("TIM1 is now manually configured and running!\n");
    printf("If motors still don't spin:\n");
    printf("  - Check TB6612 VM power (6-12V)\n");
    printf("  - Check TB6612 STBY pin (should be HIGH)\n");
    printf("  - Check motor connections\n");
    printf("  - Use oscilloscope on PE9/PE11 to verify PWM output\n");
    printf("\n");
    printf("========================================\n");
    printf("\n");
}
