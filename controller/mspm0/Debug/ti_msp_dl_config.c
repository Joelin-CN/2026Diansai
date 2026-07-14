/*
 * Copyright (c) 2023, Texas Instruments Incorporated
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
 *  ============ ti_msp_dl_config.c =============
 *  Configured MSPM0 DriverLib module definitions
 *
 *  DO NOT EDIT - This file is generated for the MSPM0G350X
 *  by the SysConfig tool.
 */

#include "ti_msp_dl_config.h"

DL_TimerA_backupConfig gMOTOR_PWM_ABackup;
DL_TimerA_backupConfig gMOTOR_PWM_BBackup;
DL_UART_Main_backupConfig gUART3Backup;
DL_SPI_backupConfig gSPI1Backup;

/*
 *  ======== SYSCFG_DL_init ========
 *  Perform any initialization needed before using any board APIs
 */
SYSCONFIG_WEAK void SYSCFG_DL_init(void)
{
    SYSCFG_DL_initPower();
    SYSCFG_DL_GPIO_init();
    /* Module-Specific Initializations*/
    SYSCFG_DL_SYSCTL_init();
    SYSCFG_DL_MOTOR_PWM_A_init();
    SYSCFG_DL_MOTOR_PWM_B_init();
    SYSCFG_DL_I2C0_init();
    SYSCFG_DL_UART0_init();
    SYSCFG_DL_UART1_init();
    SYSCFG_DL_UART3_init();
    SYSCFG_DL_SPI1_init();
    SYSCFG_DL_DMA_init();
    /* Ensure backup structures have no valid state */
	gMOTOR_PWM_ABackup.backupRdy 	= false;
	gMOTOR_PWM_BBackup.backupRdy 	= false;
	gUART3Backup.backupRdy 	= false;
	gSPI1Backup.backupRdy 	= false;

}
/*
 * User should take care to save and restore register configuration in application.
 * See Retention Configuration section for more details.
 */
SYSCONFIG_WEAK bool SYSCFG_DL_saveConfiguration(void)
{
    bool retStatus = true;

	retStatus &= DL_TimerA_saveConfiguration(MOTOR_PWM_A_INST, &gMOTOR_PWM_ABackup);
	retStatus &= DL_TimerA_saveConfiguration(MOTOR_PWM_B_INST, &gMOTOR_PWM_BBackup);
	retStatus &= DL_UART_Main_saveConfiguration(UART3_INST, &gUART3Backup);
	retStatus &= DL_SPI_saveConfiguration(SPI1_INST, &gSPI1Backup);

    return retStatus;
}


SYSCONFIG_WEAK bool SYSCFG_DL_restoreConfiguration(void)
{
    bool retStatus = true;

	retStatus &= DL_TimerA_restoreConfiguration(MOTOR_PWM_A_INST, &gMOTOR_PWM_ABackup, false);
	retStatus &= DL_TimerA_restoreConfiguration(MOTOR_PWM_B_INST, &gMOTOR_PWM_BBackup, false);
	retStatus &= DL_UART_Main_restoreConfiguration(UART3_INST, &gUART3Backup);
	retStatus &= DL_SPI_restoreConfiguration(SPI1_INST, &gSPI1Backup);

    return retStatus;
}

SYSCONFIG_WEAK void SYSCFG_DL_initPower(void)
{
    DL_GPIO_reset(GPIOA);
    DL_GPIO_reset(GPIOB);
    DL_TimerA_reset(MOTOR_PWM_A_INST);
    DL_TimerA_reset(MOTOR_PWM_B_INST);
    DL_I2C_reset(I2C0_INST);
    DL_UART_Main_reset(UART0_INST);
    DL_UART_Main_reset(UART1_INST);
    DL_UART_Main_reset(UART3_INST);
    DL_SPI_reset(SPI1_INST);


    DL_GPIO_enablePower(GPIOA);
    DL_GPIO_enablePower(GPIOB);
    DL_TimerA_enablePower(MOTOR_PWM_A_INST);
    DL_TimerA_enablePower(MOTOR_PWM_B_INST);
    DL_I2C_enablePower(I2C0_INST);
    DL_UART_Main_enablePower(UART0_INST);
    DL_UART_Main_enablePower(UART1_INST);
    DL_UART_Main_enablePower(UART3_INST);
    DL_SPI_enablePower(SPI1_INST);

    delay_cycles(POWER_STARTUP_DELAY);
}

SYSCONFIG_WEAK void SYSCFG_DL_GPIO_init(void)
{

    DL_GPIO_initPeripheralOutputFunction(GPIO_MOTOR_PWM_A_C0_IOMUX,GPIO_MOTOR_PWM_A_C0_IOMUX_FUNC);
    DL_GPIO_enableOutput(GPIO_MOTOR_PWM_A_C0_PORT, GPIO_MOTOR_PWM_A_C0_PIN);
    DL_GPIO_initPeripheralOutputFunction(GPIO_MOTOR_PWM_A_C1_IOMUX,GPIO_MOTOR_PWM_A_C1_IOMUX_FUNC);
    DL_GPIO_enableOutput(GPIO_MOTOR_PWM_A_C1_PORT, GPIO_MOTOR_PWM_A_C1_PIN);
    DL_GPIO_initPeripheralOutputFunction(GPIO_MOTOR_PWM_A_C3_IOMUX,GPIO_MOTOR_PWM_A_C3_IOMUX_FUNC);
    DL_GPIO_enableOutput(GPIO_MOTOR_PWM_A_C3_PORT, GPIO_MOTOR_PWM_A_C3_PIN);
    DL_GPIO_initPeripheralOutputFunction(GPIO_MOTOR_PWM_B_C1_IOMUX,GPIO_MOTOR_PWM_B_C1_IOMUX_FUNC);
    DL_GPIO_enableOutput(GPIO_MOTOR_PWM_B_C1_PORT, GPIO_MOTOR_PWM_B_C1_PIN);

    
	DL_GPIO_initPeripheralInputFunctionFeatures(
		 GPIO_I2C0_IOMUX_SDA, GPIO_I2C0_IOMUX_SDA_FUNC,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
	DL_GPIO_initPeripheralInputFunctionFeatures(
		 GPIO_I2C0_IOMUX_SCL, GPIO_I2C0_IOMUX_SCL_FUNC,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
    DL_GPIO_enableHiZ(GPIO_I2C0_IOMUX_SDA);
    DL_GPIO_enableHiZ(GPIO_I2C0_IOMUX_SCL);

    DL_GPIO_initPeripheralOutputFunction(
        GPIO_UART0_IOMUX_TX, GPIO_UART0_IOMUX_TX_FUNC);
    DL_GPIO_initPeripheralInputFunction(
        GPIO_UART0_IOMUX_RX, GPIO_UART0_IOMUX_RX_FUNC);
    DL_GPIO_initPeripheralOutputFunction(
        GPIO_UART1_IOMUX_TX, GPIO_UART1_IOMUX_TX_FUNC);
    DL_GPIO_initPeripheralInputFunction(
        GPIO_UART1_IOMUX_RX, GPIO_UART1_IOMUX_RX_FUNC);
    DL_GPIO_initPeripheralOutputFunction(
        GPIO_UART3_IOMUX_TX, GPIO_UART3_IOMUX_TX_FUNC);
    DL_GPIO_initPeripheralInputFunction(
        GPIO_UART3_IOMUX_RX, GPIO_UART3_IOMUX_RX_FUNC);

    DL_GPIO_initPeripheralOutputFunction(
        GPIO_SPI1_IOMUX_SCLK, GPIO_SPI1_IOMUX_SCLK_FUNC);
    DL_GPIO_initPeripheralOutputFunction(
        GPIO_SPI1_IOMUX_PICO, GPIO_SPI1_IOMUX_PICO_FUNC);
    DL_GPIO_initPeripheralInputFunction(
        GPIO_SPI1_IOMUX_POCI, GPIO_SPI1_IOMUX_POCI_FUNC);

    DL_GPIO_initDigitalOutput(ICM42688_CS_IOMUX);

    DL_GPIO_initDigitalOutput(MOTOR_DIR_M1_IN1_IOMUX);

    DL_GPIO_initDigitalOutput(MOTOR_DIR_M1_IN2_IOMUX);

    DL_GPIO_initDigitalOutput(MOTOR_DIR_M2_IN1_IOMUX);

    DL_GPIO_initDigitalOutput(MOTOR_DIR_M2_IN2_IOMUX);

    DL_GPIO_initDigitalOutput(MOTOR_DIR_M3_IN1_IOMUX);

    DL_GPIO_initDigitalOutput(MOTOR_DIR_M3_IN2_IOMUX);

    DL_GPIO_initDigitalOutput(MOTOR_DIR_M4_IN1_IOMUX);

    DL_GPIO_initDigitalOutput(MOTOR_DIR_M4_IN2_IOMUX);

    DL_GPIO_initDigitalInputFeatures(ENCODER_ENC1_A_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_initDigitalInputFeatures(ENCODER_ENC1_B_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_initDigitalInputFeatures(ENCODER_ENC2_A_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_initDigitalInputFeatures(ENCODER_ENC2_B_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_initDigitalInputFeatures(ENCODER_ENC3_A_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_initDigitalInputFeatures(ENCODER_ENC3_B_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_initDigitalInputFeatures(ENCODER_ENC4_A_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_initDigitalInputFeatures(ENCODER_ENC4_B_IOMUX,
		 DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE,
		 DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);

    DL_GPIO_clearPins(GPIOA, MOTOR_DIR_M2_IN2_PIN |
		MOTOR_DIR_M3_IN1_PIN);
    DL_GPIO_enableOutput(GPIOA, MOTOR_DIR_M2_IN2_PIN |
		MOTOR_DIR_M3_IN1_PIN);
    DL_GPIO_setLowerPinsPolarity(GPIOA, DL_GPIO_PIN_12_EDGE_RISE_FALL |
		DL_GPIO_PIN_13_EDGE_RISE_FALL |
		DL_GPIO_PIN_2_EDGE_RISE_FALL);
    DL_GPIO_setUpperPinsPolarity(GPIOA, DL_GPIO_PIN_26_EDGE_RISE_FALL |
		DL_GPIO_PIN_27_EDGE_RISE_FALL |
		DL_GPIO_PIN_28_EDGE_RISE_FALL |
		DL_GPIO_PIN_31_EDGE_RISE_FALL);
    DL_GPIO_clearInterruptStatus(GPIOA, ENCODER_ENC1_A_PIN |
		ENCODER_ENC1_B_PIN |
		ENCODER_ENC2_A_PIN |
		ENCODER_ENC2_B_PIN |
		ENCODER_ENC3_A_PIN |
		ENCODER_ENC3_B_PIN |
		ENCODER_ENC4_A_PIN);
    DL_GPIO_enableInterrupt(GPIOA, ENCODER_ENC1_A_PIN |
		ENCODER_ENC1_B_PIN |
		ENCODER_ENC2_A_PIN |
		ENCODER_ENC2_B_PIN |
		ENCODER_ENC3_A_PIN |
		ENCODER_ENC3_B_PIN |
		ENCODER_ENC4_A_PIN);
    DL_GPIO_clearPins(GPIOB, MOTOR_DIR_M1_IN1_PIN |
		MOTOR_DIR_M1_IN2_PIN |
		MOTOR_DIR_M2_IN1_PIN |
		MOTOR_DIR_M3_IN2_PIN |
		MOTOR_DIR_M4_IN1_PIN |
		MOTOR_DIR_M4_IN2_PIN);
    DL_GPIO_setPins(GPIOB, ICM42688_CS_PIN);
    DL_GPIO_enableOutput(GPIOB, ICM42688_CS_PIN |
		MOTOR_DIR_M1_IN1_PIN |
		MOTOR_DIR_M1_IN2_PIN |
		MOTOR_DIR_M2_IN1_PIN |
		MOTOR_DIR_M3_IN2_PIN |
		MOTOR_DIR_M4_IN1_PIN |
		MOTOR_DIR_M4_IN2_PIN);
    DL_GPIO_setUpperPinsPolarity(GPIOB, DL_GPIO_PIN_18_EDGE_RISE_FALL);
    DL_GPIO_clearInterruptStatus(GPIOB, ENCODER_ENC4_B_PIN);
    DL_GPIO_enableInterrupt(GPIOB, ENCODER_ENC4_B_PIN);

}



SYSCONFIG_WEAK void SYSCFG_DL_SYSCTL_init(void)
{

	//Low Power Mode is configured to be SLEEP0
    DL_SYSCTL_setBORThreshold(DL_SYSCTL_BOR_THRESHOLD_LEVEL_0);

    
	DL_SYSCTL_setSYSOSCFreq(DL_SYSCTL_SYSOSC_FREQ_BASE);
	/* Set default configuration */
	DL_SYSCTL_disableHFXT();
	DL_SYSCTL_disableSYSPLL();

}


/*
 * Timer clock configuration to be sourced by  / 1 (32000000 Hz)
 * timerClkFreq = (timerClkSrc / (timerClkDivRatio * (timerClkPrescale + 1)))
 *   32000000 Hz = 32000000 Hz / (1 * (0 + 1))
 */
static const DL_TimerA_ClockConfig gMOTOR_PWM_AClockConfig = {
    .clockSel = DL_TIMER_CLOCK_BUSCLK,
    .divideRatio = DL_TIMER_CLOCK_DIVIDE_1,
    .prescale = 0U
};

static const DL_TimerA_PWMConfig gMOTOR_PWM_AConfig = {
    .pwmMode = DL_TIMER_PWM_MODE_EDGE_ALIGN,
    .period = 1000,
    .isTimerWithFourCC = true,
    .startTimer = DL_TIMER_START,
};

SYSCONFIG_WEAK void SYSCFG_DL_MOTOR_PWM_A_init(void) {

    DL_TimerA_setClockConfig(
        MOTOR_PWM_A_INST, (DL_TimerA_ClockConfig *) &gMOTOR_PWM_AClockConfig);

    DL_TimerA_initPWMMode(
        MOTOR_PWM_A_INST, (DL_TimerA_PWMConfig *) &gMOTOR_PWM_AConfig);

    // Set Counter control to the smallest CC index being used
    DL_TimerA_setCounterControl(MOTOR_PWM_A_INST,DL_TIMER_CZC_CCCTL0_ZCOND,DL_TIMER_CAC_CCCTL0_ACOND,DL_TIMER_CLC_CCCTL0_LCOND);

    DL_TimerA_setCaptureCompareOutCtl(MOTOR_PWM_A_INST, DL_TIMER_CC_OCTL_INIT_VAL_LOW,
		DL_TIMER_CC_OCTL_INV_OUT_DISABLED, DL_TIMER_CC_OCTL_SRC_FUNCVAL,
		DL_TIMERA_CAPTURE_COMPARE_0_INDEX);

    DL_TimerA_setCaptCompUpdateMethod(MOTOR_PWM_A_INST, DL_TIMER_CC_UPDATE_METHOD_IMMEDIATE, DL_TIMERA_CAPTURE_COMPARE_0_INDEX);
    DL_TimerA_setCaptureCompareValue(MOTOR_PWM_A_INST, 1000, DL_TIMER_CC_0_INDEX);

    DL_TimerA_setCaptureCompareOutCtl(MOTOR_PWM_A_INST, DL_TIMER_CC_OCTL_INIT_VAL_LOW,
		DL_TIMER_CC_OCTL_INV_OUT_DISABLED, DL_TIMER_CC_OCTL_SRC_FUNCVAL,
		DL_TIMERA_CAPTURE_COMPARE_1_INDEX);

    DL_TimerA_setCaptCompUpdateMethod(MOTOR_PWM_A_INST, DL_TIMER_CC_UPDATE_METHOD_IMMEDIATE, DL_TIMERA_CAPTURE_COMPARE_1_INDEX);
    DL_TimerA_setCaptureCompareValue(MOTOR_PWM_A_INST, 1000, DL_TIMER_CC_1_INDEX);

    DL_TimerA_setCaptureCompareOutCtl(MOTOR_PWM_A_INST, DL_TIMER_CC_OCTL_INIT_VAL_LOW,
		DL_TIMER_CC_OCTL_INV_OUT_DISABLED, DL_TIMER_CC_OCTL_SRC_FUNCVAL,
		DL_TIMERA_CAPTURE_COMPARE_3_INDEX);

    DL_TimerA_setCaptCompUpdateMethod(MOTOR_PWM_A_INST, DL_TIMER_CC_UPDATE_METHOD_IMMEDIATE, DL_TIMERA_CAPTURE_COMPARE_3_INDEX);
    DL_TimerA_setCaptureCompareValue(MOTOR_PWM_A_INST, 1000, DL_TIMER_CC_3_INDEX);

    DL_TimerA_enableClock(MOTOR_PWM_A_INST);


    
    DL_TimerA_setCCPDirection(MOTOR_PWM_A_INST , DL_TIMER_CC0_OUTPUT | DL_TIMER_CC1_OUTPUT | DL_TIMER_CC3_OUTPUT );
    DL_TimerA_enableShadowFeatures(MOTOR_PWM_A_INST);


}
/*
 * Timer clock configuration to be sourced by  / 1 (32000000 Hz)
 * timerClkFreq = (timerClkSrc / (timerClkDivRatio * (timerClkPrescale + 1)))
 *   32000000 Hz = 32000000 Hz / (1 * (0 + 1))
 */
static const DL_TimerA_ClockConfig gMOTOR_PWM_BClockConfig = {
    .clockSel = DL_TIMER_CLOCK_BUSCLK,
    .divideRatio = DL_TIMER_CLOCK_DIVIDE_1,
    .prescale = 0U
};

static const DL_TimerA_PWMConfig gMOTOR_PWM_BConfig = {
    .pwmMode = DL_TIMER_PWM_MODE_EDGE_ALIGN,
    .period = 1000,
    .isTimerWithFourCC = false,
    .startTimer = DL_TIMER_START,
};

SYSCONFIG_WEAK void SYSCFG_DL_MOTOR_PWM_B_init(void) {

    DL_TimerA_setClockConfig(
        MOTOR_PWM_B_INST, (DL_TimerA_ClockConfig *) &gMOTOR_PWM_BClockConfig);

    DL_TimerA_initPWMMode(
        MOTOR_PWM_B_INST, (DL_TimerA_PWMConfig *) &gMOTOR_PWM_BConfig);

    // Set Counter control to the smallest CC index being used
    DL_TimerA_setCounterControl(MOTOR_PWM_B_INST,DL_TIMER_CZC_CCCTL1_ZCOND,DL_TIMER_CAC_CCCTL1_ACOND,DL_TIMER_CLC_CCCTL1_LCOND);

    DL_TimerA_setCaptureCompareOutCtl(MOTOR_PWM_B_INST, DL_TIMER_CC_OCTL_INIT_VAL_LOW,
		DL_TIMER_CC_OCTL_INV_OUT_DISABLED, DL_TIMER_CC_OCTL_SRC_FUNCVAL,
		DL_TIMERA_CAPTURE_COMPARE_1_INDEX);

    DL_TimerA_setCaptCompUpdateMethod(MOTOR_PWM_B_INST, DL_TIMER_CC_UPDATE_METHOD_IMMEDIATE, DL_TIMERA_CAPTURE_COMPARE_1_INDEX);
    DL_TimerA_setCaptureCompareValue(MOTOR_PWM_B_INST, 1000, DL_TIMER_CC_1_INDEX);

    DL_TimerA_enableClock(MOTOR_PWM_B_INST);


    
    DL_TimerA_setCCPDirection(MOTOR_PWM_B_INST , DL_TIMER_CC1_OUTPUT );
    DL_TimerA_enableShadowFeatures(MOTOR_PWM_B_INST);


}


static const DL_I2C_ClockConfig gI2C0ClockConfig = {
    .clockSel = DL_I2C_CLOCK_BUSCLK,
    .divideRatio = DL_I2C_CLOCK_DIVIDE_1,
};

SYSCONFIG_WEAK void SYSCFG_DL_I2C0_init(void) {

    DL_I2C_setClockConfig(I2C0_INST,
        (DL_I2C_ClockConfig *) &gI2C0ClockConfig);
    DL_I2C_setAnalogGlitchFilterPulseWidth(I2C0_INST,
        DL_I2C_ANALOG_GLITCH_FILTER_WIDTH_50NS);
    DL_I2C_enableAnalogGlitchFilter(I2C0_INST);

    /* Configure Controller Mode */
    DL_I2C_resetControllerTransfer(I2C0_INST);
    /* Set frequency to 100000 Hz*/
    DL_I2C_setTimerPeriod(I2C0_INST, 31);
    DL_I2C_setControllerTXFIFOThreshold(I2C0_INST, DL_I2C_TX_FIFO_LEVEL_EMPTY);
    DL_I2C_setControllerRXFIFOThreshold(I2C0_INST, DL_I2C_RX_FIFO_LEVEL_BYTES_1);
    DL_I2C_enableControllerClockStretching(I2C0_INST);


    /* Enable module */
    DL_I2C_enableController(I2C0_INST);


}

static const DL_UART_Main_ClockConfig gUART0ClockConfig = {
    .clockSel    = DL_UART_MAIN_CLOCK_BUSCLK,
    .divideRatio = DL_UART_MAIN_CLOCK_DIVIDE_RATIO_1
};

static const DL_UART_Main_Config gUART0Config = {
    .mode        = DL_UART_MAIN_MODE_NORMAL,
    .direction   = DL_UART_MAIN_DIRECTION_TX_RX,
    .flowControl = DL_UART_MAIN_FLOW_CONTROL_NONE,
    .parity      = DL_UART_MAIN_PARITY_NONE,
    .wordLength  = DL_UART_MAIN_WORD_LENGTH_8_BITS,
    .stopBits    = DL_UART_MAIN_STOP_BITS_ONE
};

SYSCONFIG_WEAK void SYSCFG_DL_UART0_init(void)
{
    DL_UART_Main_setClockConfig(UART0_INST, (DL_UART_Main_ClockConfig *) &gUART0ClockConfig);

    DL_UART_Main_init(UART0_INST, (DL_UART_Main_Config *) &gUART0Config);
    /*
     * Configure baud rate by setting oversampling and baud rate divisors.
     *  Target baud rate: 115200
     *  Actual baud rate: 115211.52
     */
    DL_UART_Main_setOversampling(UART0_INST, DL_UART_OVERSAMPLING_RATE_16X);
    DL_UART_Main_setBaudRateDivisor(UART0_INST, UART0_IBRD_32_MHZ_115200_BAUD, UART0_FBRD_32_MHZ_115200_BAUD);


    /* Configure FIFOs */
    DL_UART_Main_enableFIFOs(UART0_INST);
    DL_UART_Main_setRXFIFOThreshold(UART0_INST, DL_UART_RX_FIFO_LEVEL_1_2_FULL);
    DL_UART_Main_setTXFIFOThreshold(UART0_INST, DL_UART_TX_FIFO_LEVEL_1_2_EMPTY);

    DL_UART_Main_enable(UART0_INST);
}
static const DL_UART_Main_ClockConfig gUART1ClockConfig = {
    .clockSel    = DL_UART_MAIN_CLOCK_BUSCLK,
    .divideRatio = DL_UART_MAIN_CLOCK_DIVIDE_RATIO_1
};

static const DL_UART_Main_Config gUART1Config = {
    .mode        = DL_UART_MAIN_MODE_NORMAL,
    .direction   = DL_UART_MAIN_DIRECTION_TX_RX,
    .flowControl = DL_UART_MAIN_FLOW_CONTROL_NONE,
    .parity      = DL_UART_MAIN_PARITY_NONE,
    .wordLength  = DL_UART_MAIN_WORD_LENGTH_8_BITS,
    .stopBits    = DL_UART_MAIN_STOP_BITS_ONE
};

SYSCONFIG_WEAK void SYSCFG_DL_UART1_init(void)
{
    DL_UART_Main_setClockConfig(UART1_INST, (DL_UART_Main_ClockConfig *) &gUART1ClockConfig);

    DL_UART_Main_init(UART1_INST, (DL_UART_Main_Config *) &gUART1Config);
    /*
     * Configure baud rate by setting oversampling and baud rate divisors.
     *  Target baud rate: 115200
     *  Actual baud rate: 115211.52
     */
    DL_UART_Main_setOversampling(UART1_INST, DL_UART_OVERSAMPLING_RATE_16X);
    DL_UART_Main_setBaudRateDivisor(UART1_INST, UART1_IBRD_32_MHZ_115200_BAUD, UART1_FBRD_32_MHZ_115200_BAUD);


    /* Configure FIFOs */
    DL_UART_Main_enableFIFOs(UART1_INST);
    DL_UART_Main_setRXFIFOThreshold(UART1_INST, DL_UART_RX_FIFO_LEVEL_1_2_FULL);
    DL_UART_Main_setTXFIFOThreshold(UART1_INST, DL_UART_TX_FIFO_LEVEL_1_2_EMPTY);

    DL_UART_Main_enable(UART1_INST);
}
static const DL_UART_Main_ClockConfig gUART3ClockConfig = {
    .clockSel    = DL_UART_MAIN_CLOCK_BUSCLK,
    .divideRatio = DL_UART_MAIN_CLOCK_DIVIDE_RATIO_1
};

static const DL_UART_Main_Config gUART3Config = {
    .mode        = DL_UART_MAIN_MODE_NORMAL,
    .direction   = DL_UART_MAIN_DIRECTION_TX_RX,
    .flowControl = DL_UART_MAIN_FLOW_CONTROL_NONE,
    .parity      = DL_UART_MAIN_PARITY_NONE,
    .wordLength  = DL_UART_MAIN_WORD_LENGTH_8_BITS,
    .stopBits    = DL_UART_MAIN_STOP_BITS_ONE
};

SYSCONFIG_WEAK void SYSCFG_DL_UART3_init(void)
{
    DL_UART_Main_setClockConfig(UART3_INST, (DL_UART_Main_ClockConfig *) &gUART3ClockConfig);

    DL_UART_Main_init(UART3_INST, (DL_UART_Main_Config *) &gUART3Config);
    /*
     * Configure baud rate by setting oversampling and baud rate divisors.
     *  Target baud rate: 115200
     *  Actual baud rate: 115211.52
     */
    DL_UART_Main_setOversampling(UART3_INST, DL_UART_OVERSAMPLING_RATE_16X);
    DL_UART_Main_setBaudRateDivisor(UART3_INST, UART3_IBRD_32_MHZ_115200_BAUD, UART3_FBRD_32_MHZ_115200_BAUD);


    /* Configure FIFOs */
    DL_UART_Main_enableFIFOs(UART3_INST);
    DL_UART_Main_setRXFIFOThreshold(UART3_INST, DL_UART_RX_FIFO_LEVEL_1_2_FULL);
    DL_UART_Main_setTXFIFOThreshold(UART3_INST, DL_UART_TX_FIFO_LEVEL_1_2_EMPTY);

    DL_UART_Main_enable(UART3_INST);
}

static const DL_SPI_Config gSPI1_config = {
    .mode        = DL_SPI_MODE_CONTROLLER,
    .frameFormat = DL_SPI_FRAME_FORMAT_MOTO3_POL0_PHA0,
    .parity      = DL_SPI_PARITY_NONE,
    .dataSize    = DL_SPI_DATA_SIZE_8,
    .bitOrder    = DL_SPI_BIT_ORDER_MSB_FIRST,
};

static const DL_SPI_ClockConfig gSPI1_clockConfig = {
    .clockSel    = DL_SPI_CLOCK_BUSCLK,
    .divideRatio = DL_SPI_CLOCK_DIVIDE_RATIO_1
};

SYSCONFIG_WEAK void SYSCFG_DL_SPI1_init(void) {
    DL_SPI_setClockConfig(SPI1_INST, (DL_SPI_ClockConfig *) &gSPI1_clockConfig);

    DL_SPI_init(SPI1_INST, (DL_SPI_Config *) &gSPI1_config);

    /* Configure Controller mode */
    /*
     * Set the bit rate clock divider to generate the serial output clock
     *     outputBitRate = (spiInputClock) / ((1 + SCR) * 2)
     *     1000000 = (32000000)/((1 + 15) * 2)
     */
    DL_SPI_setBitRateSerialClockDivider(SPI1_INST, 15);

    /* Enable SPI TX interrupt as a trigger for DMA */
    DL_SPI_enableDMATransmitEvent(SPI1_INST);

    /* Enable SPI RX interrupt as a trigger for DMA */
    DL_SPI_enableDMAReceiveEvent(SPI1_INST, DL_SPI_DMA_INTERRUPT_RX);
    /* Set RX and TX FIFO threshold levels */
    DL_SPI_setFIFOThreshold(SPI1_INST, DL_SPI_RX_FIFO_LEVEL_1_2_FULL, DL_SPI_TX_FIFO_LEVEL_1_2_EMPTY);

    /* Enable module */
    DL_SPI_enable(SPI1_INST);
}

static const DL_DMA_Config gDMA_SPI1_RXConfig = {
    .transferMode   = DL_DMA_SINGLE_TRANSFER_MODE,
    .extendedMode   = DL_DMA_NORMAL_MODE,
    .destIncrement  = DL_DMA_ADDR_INCREMENT,
    .srcIncrement   = DL_DMA_ADDR_UNCHANGED,
    .destWidth      = DL_DMA_WIDTH_BYTE,
    .srcWidth       = DL_DMA_WIDTH_BYTE,
    .trigger        = SPI1_INST_DMA_TRIGGER_0,
    .triggerType    = DL_DMA_TRIGGER_TYPE_EXTERNAL,
};

SYSCONFIG_WEAK void SYSCFG_DL_DMA_SPI1_RX_init(void)
{
    DL_DMA_initChannel(DMA, DMA_SPI1_RX_CHAN_ID , (DL_DMA_Config *) &gDMA_SPI1_RXConfig);
}
static const DL_DMA_Config gDMA_SPI1_TXConfig = {
    .transferMode   = DL_DMA_SINGLE_TRANSFER_MODE,
    .extendedMode   = DL_DMA_NORMAL_MODE,
    .destIncrement  = DL_DMA_ADDR_UNCHANGED,
    .srcIncrement   = DL_DMA_ADDR_INCREMENT,
    .destWidth      = DL_DMA_WIDTH_BYTE,
    .srcWidth       = DL_DMA_WIDTH_BYTE,
    .trigger        = SPI1_INST_DMA_TRIGGER_1,
    .triggerType    = DL_DMA_TRIGGER_TYPE_EXTERNAL,
};

SYSCONFIG_WEAK void SYSCFG_DL_DMA_SPI1_TX_init(void)
{
    DL_DMA_initChannel(DMA, DMA_SPI1_TX_CHAN_ID , (DL_DMA_Config *) &gDMA_SPI1_TXConfig);
}
SYSCONFIG_WEAK void SYSCFG_DL_DMA_init(void){
    SYSCFG_DL_DMA_SPI1_RX_init();
    SYSCFG_DL_DMA_SPI1_TX_init();
}


