/*
 * Copyright (c) 2023, Texas Instruments Incorporated - http://www.ti.com
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  ============ ti_msp_dl_config.h =============
 *  Configured MSPM0 DriverLib module declarations
 *
 *  DO NOT EDIT - This file is generated for the MSPM0G350X
 *  by the SysConfig tool.
 */
#ifndef ti_msp_dl_config_h
#define ti_msp_dl_config_h

#define CONFIG_MSPM0G350X
#define CONFIG_MSPM0G3507

#if defined(__ti_version__) || defined(__TI_COMPILER_VERSION__)
#define SYSCONFIG_WEAK __attribute__((weak))
#elif defined(__IAR_SYSTEMS_ICC__)
#define SYSCONFIG_WEAK __weak
#elif defined(__GNUC__)
#define SYSCONFIG_WEAK __attribute__((weak))
#endif

#include <ti/devices/msp/msp.h>
#include <ti/driverlib/driverlib.h>
#include <ti/driverlib/m0p/dl_core.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 *  ======== SYSCFG_DL_init ========
 *  Perform all required MSP DL initialization
 *
 *  This function should be called once at a point before any use of
 *  MSP DL.
 */


/* clang-format off */

#define POWER_STARTUP_DELAY                                                (16)



#define CPUCLK_FREQ                                                     32000000



/* Defines for MOTOR_PWM_A */
#define MOTOR_PWM_A_INST                                                   TIMA0
#define MOTOR_PWM_A_INST_IRQHandler                             TIMA0_IRQHandler
#define MOTOR_PWM_A_INST_INT_IRQN                               (TIMA0_INT_IRQn)
#define MOTOR_PWM_A_INST_CLK_FREQ                                       32000000
/* GPIO defines for channel 0 */
#define GPIO_MOTOR_PWM_A_C0_PORT                                           GPIOA
#define GPIO_MOTOR_PWM_A_C0_PIN                                   DL_GPIO_PIN_21
#define GPIO_MOTOR_PWM_A_C0_IOMUX                                (IOMUX_PINCM46)
#define GPIO_MOTOR_PWM_A_C0_IOMUX_FUNC               IOMUX_PINCM46_PF_TIMA0_CCP0
#define GPIO_MOTOR_PWM_A_C0_IDX                              DL_TIMER_CC_0_INDEX
/* GPIO defines for channel 1 */
#define GPIO_MOTOR_PWM_A_C1_PORT                                           GPIOA
#define GPIO_MOTOR_PWM_A_C1_PIN                                   DL_GPIO_PIN_22
#define GPIO_MOTOR_PWM_A_C1_IOMUX                                (IOMUX_PINCM47)
#define GPIO_MOTOR_PWM_A_C1_IOMUX_FUNC               IOMUX_PINCM47_PF_TIMA0_CCP1
#define GPIO_MOTOR_PWM_A_C1_IDX                              DL_TIMER_CC_1_INDEX
/* GPIO defines for channel 3 */
#define GPIO_MOTOR_PWM_A_C3_PORT                                           GPIOA
#define GPIO_MOTOR_PWM_A_C3_PIN                                   DL_GPIO_PIN_25
#define GPIO_MOTOR_PWM_A_C3_IOMUX                                (IOMUX_PINCM55)
#define GPIO_MOTOR_PWM_A_C3_IOMUX_FUNC               IOMUX_PINCM55_PF_TIMA0_CCP3
#define GPIO_MOTOR_PWM_A_C3_IDX                              DL_TIMER_CC_3_INDEX

/* Defines for MOTOR_PWM_B */
#define MOTOR_PWM_B_INST                                                   TIMA1
#define MOTOR_PWM_B_INST_IRQHandler                             TIMA1_IRQHandler
#define MOTOR_PWM_B_INST_INT_IRQN                               (TIMA1_INT_IRQn)
#define MOTOR_PWM_B_INST_CLK_FREQ                                       32000000
/* GPIO defines for channel 1 */
#define GPIO_MOTOR_PWM_B_C1_PORT                                           GPIOA
#define GPIO_MOTOR_PWM_B_C1_PIN                                   DL_GPIO_PIN_24
#define GPIO_MOTOR_PWM_B_C1_IOMUX                                (IOMUX_PINCM54)
#define GPIO_MOTOR_PWM_B_C1_IOMUX_FUNC               IOMUX_PINCM54_PF_TIMA1_CCP1
#define GPIO_MOTOR_PWM_B_C1_IDX                              DL_TIMER_CC_1_INDEX




/* Defines for I2C0 */
#define I2C0_INST                                                           I2C0
#define I2C0_INST_IRQHandler                                     I2C0_IRQHandler
#define I2C0_INST_INT_IRQN                                         I2C0_INT_IRQn
#define I2C0_BUS_SPEED_HZ                                                 100000
#define GPIO_I2C0_SDA_PORT                                                 GPIOA
#define GPIO_I2C0_SDA_PIN                                          DL_GPIO_PIN_0
#define GPIO_I2C0_IOMUX_SDA                                       (IOMUX_PINCM1)
#define GPIO_I2C0_IOMUX_SDA_FUNC                        IOMUX_PINCM1_PF_I2C0_SDA
#define GPIO_I2C0_SCL_PORT                                                 GPIOA
#define GPIO_I2C0_SCL_PIN                                          DL_GPIO_PIN_1
#define GPIO_I2C0_IOMUX_SCL                                       (IOMUX_PINCM2)
#define GPIO_I2C0_IOMUX_SCL_FUNC                        IOMUX_PINCM2_PF_I2C0_SCL


/* Defines for UART0 */
#define UART0_INST                                                         UART0
#define UART0_INST_FREQUENCY                                            32000000
#define UART0_INST_IRQHandler                                   UART0_IRQHandler
#define UART0_INST_INT_IRQN                                       UART0_INT_IRQn
#define GPIO_UART0_RX_PORT                                                 GPIOA
#define GPIO_UART0_TX_PORT                                                 GPIOA
#define GPIO_UART0_RX_PIN                                         DL_GPIO_PIN_11
#define GPIO_UART0_TX_PIN                                         DL_GPIO_PIN_10
#define GPIO_UART0_IOMUX_RX                                      (IOMUX_PINCM22)
#define GPIO_UART0_IOMUX_TX                                      (IOMUX_PINCM21)
#define GPIO_UART0_IOMUX_RX_FUNC                       IOMUX_PINCM22_PF_UART0_RX
#define GPIO_UART0_IOMUX_TX_FUNC                       IOMUX_PINCM21_PF_UART0_TX
#define UART0_BAUD_RATE                                                 (115200)
#define UART0_IBRD_32_MHZ_115200_BAUD                                       (17)
#define UART0_FBRD_32_MHZ_115200_BAUD                                       (23)
/* Defines for UART1 */
#define UART1_INST                                                         UART1
#define UART1_INST_FREQUENCY                                            32000000
#define UART1_INST_IRQHandler                                   UART1_IRQHandler
#define UART1_INST_INT_IRQN                                       UART1_INT_IRQn
#define GPIO_UART1_RX_PORT                                                 GPIOA
#define GPIO_UART1_TX_PORT                                                 GPIOA
#define GPIO_UART1_RX_PIN                                          DL_GPIO_PIN_9
#define GPIO_UART1_TX_PIN                                          DL_GPIO_PIN_8
#define GPIO_UART1_IOMUX_RX                                      (IOMUX_PINCM20)
#define GPIO_UART1_IOMUX_TX                                      (IOMUX_PINCM19)
#define GPIO_UART1_IOMUX_RX_FUNC                       IOMUX_PINCM20_PF_UART1_RX
#define GPIO_UART1_IOMUX_TX_FUNC                       IOMUX_PINCM19_PF_UART1_TX
#define UART1_BAUD_RATE                                                 (115200)
#define UART1_IBRD_32_MHZ_115200_BAUD                                       (17)
#define UART1_FBRD_32_MHZ_115200_BAUD                                       (23)
/* Defines for UART3 */
#define UART3_INST                                                         UART3
#define UART3_INST_FREQUENCY                                            32000000
#define UART3_INST_IRQHandler                                   UART3_IRQHandler
#define UART3_INST_INT_IRQN                                       UART3_INT_IRQn
#define GPIO_UART3_RX_PORT                                                 GPIOB
#define GPIO_UART3_TX_PORT                                                 GPIOB
#define GPIO_UART3_RX_PIN                                          DL_GPIO_PIN_3
#define GPIO_UART3_TX_PIN                                          DL_GPIO_PIN_2
#define GPIO_UART3_IOMUX_RX                                      (IOMUX_PINCM16)
#define GPIO_UART3_IOMUX_TX                                      (IOMUX_PINCM15)
#define GPIO_UART3_IOMUX_RX_FUNC                       IOMUX_PINCM16_PF_UART3_RX
#define GPIO_UART3_IOMUX_TX_FUNC                       IOMUX_PINCM15_PF_UART3_TX
#define UART3_BAUD_RATE                                                 (115200)
#define UART3_IBRD_32_MHZ_115200_BAUD                                       (17)
#define UART3_FBRD_32_MHZ_115200_BAUD                                       (23)




/* Defines for SPI1 */
#define SPI1_INST                                                          SPI1
#define SPI1_INST_IRQHandler                                    SPI1_IRQHandler
#define SPI1_INST_INT_IRQN                                        SPI1_INT_IRQn
#define GPIO_SPI1_PICO_PORT                                               GPIOA
#define GPIO_SPI1_PICO_PIN                                       DL_GPIO_PIN_18
#define GPIO_SPI1_IOMUX_PICO                                    (IOMUX_PINCM40)
#define GPIO_SPI1_IOMUX_PICO_FUNC                    IOMUX_PINCM40_PF_SPI1_PICO
#define GPIO_SPI1_POCI_PORT                                               GPIOA
#define GPIO_SPI1_POCI_PIN                                       DL_GPIO_PIN_16
#define GPIO_SPI1_IOMUX_POCI                                    (IOMUX_PINCM38)
#define GPIO_SPI1_IOMUX_POCI_FUNC                    IOMUX_PINCM38_PF_SPI1_POCI
/* GPIO configuration for SPI1 */
#define GPIO_SPI1_SCLK_PORT                                               GPIOA
#define GPIO_SPI1_SCLK_PIN                                       DL_GPIO_PIN_17
#define GPIO_SPI1_IOMUX_SCLK                                    (IOMUX_PINCM39)
#define GPIO_SPI1_IOMUX_SCLK_FUNC                    IOMUX_PINCM39_PF_SPI1_SCLK



/* Defines for DMA_SPI1_RX */
#define DMA_SPI1_RX_CHAN_ID                                                  (2)
#define SPI1_INST_DMA_TRIGGER_0                               (DMA_SPI1_RX_TRIG)
/* Defines for DMA_SPI1_TX */
#define DMA_SPI1_TX_CHAN_ID                                                  (3)
#define SPI1_INST_DMA_TRIGGER_1                               (DMA_SPI1_TX_TRIG)


/* Port definition for Pin Group ICM42688 */
#define ICM42688_PORT                                                    (GPIOB)

/* Defines for CS: GPIOB.20 with pinCMx 48 on package pin 41 */
#define ICM42688_CS_PIN                                         (DL_GPIO_PIN_20)
#define ICM42688_CS_IOMUX                                        (IOMUX_PINCM48)
/* Defines for M1_IN1: GPIOB.6 with pinCMx 23 on package pin 20 */
#define MOTOR_DIR_M1_IN1_PORT                                            (GPIOB)
#define MOTOR_DIR_M1_IN1_PIN                                     (DL_GPIO_PIN_6)
#define MOTOR_DIR_M1_IN1_IOMUX                                   (IOMUX_PINCM23)
/* Defines for M1_IN2: GPIOB.7 with pinCMx 24 on package pin 21 */
#define MOTOR_DIR_M1_IN2_PORT                                            (GPIOB)
#define MOTOR_DIR_M1_IN2_PIN                                     (DL_GPIO_PIN_7)
#define MOTOR_DIR_M1_IN2_IOMUX                                   (IOMUX_PINCM24)
/* Defines for M2_IN1: GPIOB.8 with pinCMx 25 on package pin 22 */
#define MOTOR_DIR_M2_IN1_PORT                                            (GPIOB)
#define MOTOR_DIR_M2_IN1_PIN                                     (DL_GPIO_PIN_8)
#define MOTOR_DIR_M2_IN1_IOMUX                                   (IOMUX_PINCM25)
/* Defines for M2_IN2: GPIOA.7 with pinCMx 14 on package pin 13 */
#define MOTOR_DIR_M2_IN2_PORT                                            (GPIOA)
#define MOTOR_DIR_M2_IN2_PIN                                     (DL_GPIO_PIN_7)
#define MOTOR_DIR_M2_IN2_IOMUX                                   (IOMUX_PINCM14)
/* Defines for M3_IN1: GPIOA.15 with pinCMx 37 on package pin 30 */
#define MOTOR_DIR_M3_IN1_PORT                                            (GPIOA)
#define MOTOR_DIR_M3_IN1_PIN                                    (DL_GPIO_PIN_15)
#define MOTOR_DIR_M3_IN1_IOMUX                                   (IOMUX_PINCM37)
/* Defines for M3_IN2: GPIOB.9 with pinCMx 26 on package pin 23 */
#define MOTOR_DIR_M3_IN2_PORT                                            (GPIOB)
#define MOTOR_DIR_M3_IN2_PIN                                     (DL_GPIO_PIN_9)
#define MOTOR_DIR_M3_IN2_IOMUX                                   (IOMUX_PINCM26)
/* Defines for M4_IN1: GPIOB.19 with pinCMx 45 on package pin 38 */
#define MOTOR_DIR_M4_IN1_PORT                                            (GPIOB)
#define MOTOR_DIR_M4_IN1_PIN                                    (DL_GPIO_PIN_19)
#define MOTOR_DIR_M4_IN1_IOMUX                                   (IOMUX_PINCM45)
/* Defines for M4_IN2: GPIOB.24 with pinCMx 52 on package pin 42 */
#define MOTOR_DIR_M4_IN2_PORT                                            (GPIOB)
#define MOTOR_DIR_M4_IN2_PIN                                    (DL_GPIO_PIN_24)
#define MOTOR_DIR_M4_IN2_IOMUX                                   (IOMUX_PINCM52)
/* Defines for ENC1_A: GPIOA.12 with pinCMx 34 on package pin 27 */
#define ENCODER_ENC1_A_PORT                                              (GPIOA)
// pins affected by this interrupt request:["ENC1_A","ENC1_B","ENC2_A","ENC2_B","ENC3_A","ENC3_B","ENC4_A"]
#define ENCODER_GPIOA_INT_IRQN                                  (GPIOA_INT_IRQn)
#define ENCODER_GPIOA_INT_IIDX                  (DL_INTERRUPT_GROUP1_IIDX_GPIOA)
#define ENCODER_ENC1_A_IIDX                                 (DL_GPIO_IIDX_DIO12)
#define ENCODER_ENC1_A_PIN                                      (DL_GPIO_PIN_12)
#define ENCODER_ENC1_A_IOMUX                                     (IOMUX_PINCM34)
/* Defines for ENC1_B: GPIOA.13 with pinCMx 35 on package pin 28 */
#define ENCODER_ENC1_B_PORT                                              (GPIOA)
#define ENCODER_ENC1_B_IIDX                                 (DL_GPIO_IIDX_DIO13)
#define ENCODER_ENC1_B_PIN                                      (DL_GPIO_PIN_13)
#define ENCODER_ENC1_B_IOMUX                                     (IOMUX_PINCM35)
/* Defines for ENC2_A: GPIOA.2 with pinCMx 7 on package pin 8 */
#define ENCODER_ENC2_A_PORT                                              (GPIOA)
#define ENCODER_ENC2_A_IIDX                                  (DL_GPIO_IIDX_DIO2)
#define ENCODER_ENC2_A_PIN                                       (DL_GPIO_PIN_2)
#define ENCODER_ENC2_A_IOMUX                                      (IOMUX_PINCM7)
/* Defines for ENC2_B: GPIOA.26 with pinCMx 59 on package pin 46 */
#define ENCODER_ENC2_B_PORT                                              (GPIOA)
#define ENCODER_ENC2_B_IIDX                                 (DL_GPIO_IIDX_DIO26)
#define ENCODER_ENC2_B_PIN                                      (DL_GPIO_PIN_26)
#define ENCODER_ENC2_B_IOMUX                                     (IOMUX_PINCM59)
/* Defines for ENC3_A: GPIOA.27 with pinCMx 60 on package pin 47 */
#define ENCODER_ENC3_A_PORT                                              (GPIOA)
#define ENCODER_ENC3_A_IIDX                                 (DL_GPIO_IIDX_DIO27)
#define ENCODER_ENC3_A_PIN                                      (DL_GPIO_PIN_27)
#define ENCODER_ENC3_A_IOMUX                                     (IOMUX_PINCM60)
/* Defines for ENC3_B: GPIOA.28 with pinCMx 3 on package pin 3 */
#define ENCODER_ENC3_B_PORT                                              (GPIOA)
#define ENCODER_ENC3_B_IIDX                                 (DL_GPIO_IIDX_DIO28)
#define ENCODER_ENC3_B_PIN                                      (DL_GPIO_PIN_28)
#define ENCODER_ENC3_B_IOMUX                                      (IOMUX_PINCM3)
/* Defines for ENC4_A: GPIOA.31 with pinCMx 6 on package pin 5 */
#define ENCODER_ENC4_A_PORT                                              (GPIOA)
#define ENCODER_ENC4_A_IIDX                                 (DL_GPIO_IIDX_DIO31)
#define ENCODER_ENC4_A_PIN                                      (DL_GPIO_PIN_31)
#define ENCODER_ENC4_A_IOMUX                                      (IOMUX_PINCM6)
/* Defines for ENC4_B: GPIOB.18 with pinCMx 44 on package pin 37 */
#define ENCODER_ENC4_B_PORT                                              (GPIOB)
// pins affected by this interrupt request:["ENC4_B"]
#define ENCODER_GPIOB_INT_IRQN                                  (GPIOB_INT_IRQn)
#define ENCODER_GPIOB_INT_IIDX                  (DL_INTERRUPT_GROUP1_IIDX_GPIOB)
#define ENCODER_ENC4_B_IIDX                                 (DL_GPIO_IIDX_DIO18)
#define ENCODER_ENC4_B_PIN                                      (DL_GPIO_PIN_18)
#define ENCODER_ENC4_B_IOMUX                                     (IOMUX_PINCM44)

/* clang-format on */

void SYSCFG_DL_init(void);
void SYSCFG_DL_initPower(void);
void SYSCFG_DL_GPIO_init(void);
void SYSCFG_DL_SYSCTL_init(void);
void SYSCFG_DL_MOTOR_PWM_A_init(void);
void SYSCFG_DL_MOTOR_PWM_B_init(void);
void SYSCFG_DL_I2C0_init(void);
void SYSCFG_DL_UART0_init(void);
void SYSCFG_DL_UART1_init(void);
void SYSCFG_DL_UART3_init(void);
void SYSCFG_DL_SPI1_init(void);
void SYSCFG_DL_DMA_init(void);


bool SYSCFG_DL_saveConfiguration(void);
bool SYSCFG_DL_restoreConfiguration(void);

#ifdef __cplusplus
}
#endif

#endif /* ti_msp_dl_config_h */
