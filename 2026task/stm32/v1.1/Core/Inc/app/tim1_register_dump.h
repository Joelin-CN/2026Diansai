/**
 * @file tim1_register_dump.h
 * @brief TIM1 register dump tool for deep debugging
 */

#ifndef TIM1_REGISTER_DUMP_H
#define TIM1_REGISTER_DUMP_H

#include <stdint.h>

/**
 * @brief Dump all TIM1 registers
 */
void TIM1_DumpRegisters(void);

/**
 * @brief Force TIM1 PWM output test
 */
void TIM1_ForceTest(void);

#endif // TIM1_REGISTER_DUMP_H
