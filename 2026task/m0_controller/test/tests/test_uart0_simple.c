/**
 * @file test_uart0_simple.c
 * @brief Simple UART0 test without FreeRTOS
 * @date 2026-07-23
 *
 * This is a minimal test program to verify UART0 functionality.
 * It continuously outputs counter values every second.
 *
 * Hardware requirements:
 *   - Connect USB-to-Serial adapter to UART0 pins:
 *     TX (PA10) -> RX of adapter
 *     RX (PA11) -> TX of adapter (not used in this test)
 *     GND -> GND
 *   - Open serial terminal at 115200 baud, 8N1
 *
 * Expected output:
 *   UART0 Test Starting...
 *   Counter: 0
 *   Counter: 1
 *   Counter: 2
 *   ...
 */

#include "ti_msp_dl_config.h"
#include "uart_debug.h"

/**
 * @brief Simple delay function (blocking)
 * @param ms Milliseconds to delay (approximate at 32 MHz)
 */
static void DelayMs(uint32_t ms)
{
    /* Approximate delay: 32 MHz CPU, ~8000 cycles per ms */
    for (uint32_t i = 0; i < ms; i++) {
        for (volatile uint32_t j = 0; j < 8000; j++) {
            /* Empty loop */
        }
    }
}

/**
 * @brief Main test function
 */
int main(void)
{
    uint32_t counter = 0;

    /* Initialize all hardware (includes UART0) */
    SYSCFG_DL_init();

    /* Small delay for hardware to stabilize */
    DelayMs(100);

    /* Send test banner */
    UartDebug_Println("=================================");
    UartDebug_Println("UART0 Test Starting...");
    UartDebug_Println("=================================");
    UartDebug_Println("");
    UartDebug_Print("Device: MSPM0G3507");
    UartDebug_Println("");
    UartDebug_Print("UART0: 115200 baud, 8N1");
    UartDebug_Println("");
    UartDebug_Print("TX Pin: PA10");
    UartDebug_Println("");
    UartDebug_Print("RX Pin: PA11");
    UartDebug_Println("");
    UartDebug_Println("=================================");
    UartDebug_Println("");

    /* Main loop - print counter every second */
    while (1) {
        UartDebug_Print("Counter: ");
        UartDebug_PrintU32(counter);
        UartDebug_Println("");

        counter++;

        /* Delay 1 second */
        DelayMs(1000);
    }

    return 0;
}
