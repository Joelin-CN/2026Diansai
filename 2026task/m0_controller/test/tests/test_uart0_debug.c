/**
 * @file test_uart0_debug.c
 * @brief UART0 debug output test - verify serial communication
 * @date 2026-07-23
 */

#include "ti_msp_dl_config.h"
#include <stdint.h>
#include <string.h>

/**
 * @brief Send a single character via UART0
 */
static void UART0_SendChar(char c)
{
    /* Wait until TX FIFO has space */
    while (DL_UART_isTXFIFOFull(UART0_INST));

    /* Transmit the character */
    DL_UART_Main_transmitData(UART0_INST, c);
}

/**
 * @brief Send a string via UART0
 */
static void UART0_SendString(const char *str)
{
    while (*str) {
        UART0_SendChar(*str++);
    }
}

/**
 * @brief Send a string with newline
 */
static void UART0_Println(const char *str)
{
    UART0_SendString(str);
    UART0_SendChar('\r');
    UART0_SendChar('\n');
}

/**
 * @brief Simple delay function
 */
static void DelayMs(uint32_t ms)
{
    /* Approximate delay at 32 MHz */
    for (uint32_t i = 0; i < ms; i++) {
        for (volatile uint32_t j = 0; j < 8000; j++);
    }
}

/**
 * @brief Main test function
 */
int main(void)
{
    uint32_t counter = 0;

    /* Initialize hardware */
    SYSCFG_DL_init();

    /* Send startup message */
    UART0_Println("=================================");
    UART0_Println("UART0 Debug Test Started");
    UART0_Println("=================================");
    UART0_Println("");
    UART0_Println("Configuration:");
    UART0_Println("  Port: UART0");
    UART0_Println("  Baud: 115200");
    UART0_Println("  Pins: TX=PA10, RX=PA11");
    UART0_Println("");

    /* Main loop - send periodic messages */
    while (1) {
        /* Send counter value */
        UART0_SendString("Counter: ");

        /* Convert counter to string manually */
        char buffer[12];
        uint32_t temp = counter;
        int idx = 0;

        if (temp == 0) {
            buffer[idx++] = '0';
        } else {
            char temp_buf[12];
            int temp_idx = 0;
            while (temp > 0) {
                temp_buf[temp_idx++] = '0' + (temp % 10);
                temp /= 10;
            }
            /* Reverse the digits */
            for (int i = temp_idx - 1; i >= 0; i--) {
                buffer[idx++] = temp_buf[i];
            }
        }
        buffer[idx] = '\0';

        UART0_Println(buffer);

        counter++;

        /* Delay 1 second */
        DelayMs(1000);
    }

    return 0;
}
