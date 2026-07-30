#include "encoder.h"

#include "ti_msp_dl_config.h"

static volatile int32_t g_encoderCount[ENCODER_ID_COUNT];
static uint8_t g_encoderState[ENCODER_ID_COUNT];

/* Debug: interrupt counters */
static volatile uint32_t g_interruptCountA = 0;
static volatile uint32_t g_interruptCountB = 0;

static const int8_t g_quadratureDelta[16] = {
    0, 1, -1, 0, -1, 0, 0, 1, 1, 0, 0, -1, 0, -1, 1, 0,
};

static uint8_t Encoder_ReadState(Encoder_Id encoder)
{
    uint32_t pinsA;
    uint32_t pinsB;

    switch (encoder) {
        case ENCODER_M1:
            /* M1 physically connected to ENC2 pins */
            pinsA = DL_GPIO_readPins(GPIOA, ENCODER_ENC2_A_PIN);
            pinsB = DL_GPIO_readPins(GPIOA, ENCODER_ENC2_B_PIN);
            break;
        case ENCODER_M2:
            /* M2 physically connected to ENC1 pins */
            pinsA = DL_GPIO_readPins(GPIOA, ENCODER_ENC1_A_PIN);
            pinsB = DL_GPIO_readPins(GPIOA, ENCODER_ENC1_B_PIN);
            break;
        case ENCODER_M3:
            /* M3: swap A/B to correct direction (hardware A/B phase reversed) */
            pinsA = DL_GPIO_readPins(GPIOA, ENCODER_ENC3_B_PIN);  // Read B as A
            pinsB = DL_GPIO_readPins(GPIOA, ENCODER_ENC3_A_PIN);  // Read A as B
            break;
        case ENCODER_M4:
            /* M4: swap A/B to correct direction (hardware A/B phase reversed) */
            pinsA = DL_GPIO_readPins(GPIOB, ENCODER_ENC4_B_PIN);  // Read B as A
            pinsB = DL_GPIO_readPins(GPIOA, ENCODER_ENC4_A_PIN);  // Read A as B
            break;
        default:
            return 0U;
    }
    return ((pinsA != 0U) ? 2U : 0U) | ((pinsB != 0U) ? 1U : 0U);
}

static void Encoder_Process(Encoder_Id encoder)
{
    const uint8_t current = Encoder_ReadState(encoder);
    g_encoderCount[encoder] += g_quadratureDelta[(g_encoderState[encoder] << 2U) | current];
    g_encoderState[encoder] = current;
}

void Encoder_Init(void)
{
    uint8_t encoder;
    for (encoder = 0U; encoder < (uint8_t)ENCODER_ID_COUNT; encoder++) {
        g_encoderCount[encoder] = 0;
        g_encoderState[encoder] = Encoder_ReadState((Encoder_Id)encoder);
    }

    /* DO NOT enable interrupts yet - for debugging */
    /* Set encoder interrupts to lowest priority (3 = lowest on Cortex-M0+)
     * This allows main loop and other interrupts to preempt encoder processing */
    // NVIC_SetPriority(ENCODER_GPIOA_INT_IRQN, 3);
    // NVIC_SetPriority(ENCODER_GPIOB_INT_IRQN, 3);

    // NVIC_EnableIRQ(ENCODER_GPIOA_INT_IRQN);
    // NVIC_EnableIRQ(ENCODER_GPIOB_INT_IRQN);
}

void Encoder_Poll(void)
{
    /* Call this function periodically (e.g., 1kHz) to update encoder counts.
     * This polling approach avoids GPIO interrupt storms from noisy/floating encoder signals. */
    uint8_t encoder;
    for (encoder = 0U; encoder < (uint8_t)ENCODER_ID_COUNT; encoder++) {
        Encoder_Process((Encoder_Id)encoder);
    }
}

int32_t Encoder_GetCount(Encoder_Id encoder)
{
    return ((uint8_t)encoder < (uint8_t)ENCODER_ID_COUNT) ? g_encoderCount[encoder] : 0;
}

void Encoder_ResetCount(Encoder_Id encoder)
{
    if ((uint8_t)encoder < (uint8_t)ENCODER_ID_COUNT) {
        g_encoderCount[encoder] = 0;
        g_encoderState[encoder] = Encoder_ReadState(encoder);
    }
}

void GPIOA_IRQHandler(void)
{
    g_interruptCountA++;  /* Count every interrupt */

    const uint32_t pins = ENCODER_ENC1_A_PIN | ENCODER_ENC1_B_PIN |
                          ENCODER_ENC2_A_PIN | ENCODER_ENC2_B_PIN |
                          ENCODER_ENC3_A_PIN | ENCODER_ENC3_B_PIN | ENCODER_ENC4_A_PIN;
    const uint32_t status = DL_GPIO_getEnabledInterruptStatus(GPIOA, pins);
    DL_GPIO_clearInterruptStatus(GPIOA, status);

    /* Simplified: don't process, just clear */
    /* This reduces ISR time to minimum */
    // if ((status & (ENCODER_ENC1_A_PIN | ENCODER_ENC1_B_PIN)) != 0U) Encoder_Process(ENCODER_M1);
    // if ((status & (ENCODER_ENC2_A_PIN | ENCODER_ENC2_B_PIN)) != 0U) Encoder_Process(ENCODER_M2);
    // if ((status & (ENCODER_ENC3_A_PIN | ENCODER_ENC3_B_PIN)) != 0U) Encoder_Process(ENCODER_M3);
    // if ((status & ENCODER_ENC4_A_PIN) != 0U) Encoder_Process(ENCODER_M4);
}

void GPIOB_IRQHandler(void)
{
    g_interruptCountB++;  /* Count every interrupt */

    const uint32_t status = DL_GPIO_getEnabledInterruptStatus(GPIOB, ENCODER_ENC4_B_PIN);
    DL_GPIO_clearInterruptStatus(GPIOB, status);

    /* Simplified: don't process, just clear */
    // if ((status & ENCODER_ENC4_B_PIN) != 0U) Encoder_Process(ENCODER_M4);
}

/* Debug function to get interrupt counts */
void Encoder_GetInterruptCounts(uint32_t *countA, uint32_t *countB)
{
    *countA = g_interruptCountA;
    *countB = g_interruptCountB;
}
