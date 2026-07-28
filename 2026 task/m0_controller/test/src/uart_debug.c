/**
 * @file uart_debug.c
 * @brief UART0 debug logging implementation
 * @date 2026-07-23
 */

#include "uart_debug.h"
#include "ti_msp_dl_config.h"
#include <string.h>
#include <stdio.h>

/* Retarget printf to UART0 for Arm Compiler 6 with microLIB */
#if defined(__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050)

/**
 * @brief Retarget fputc for microLIB (used by printf)
 * @note This is called by printf in microLIB
 */
int fputc(int ch, FILE *f)
{
    (void)f;
    UartDebug_PutChar((char)ch);
    return ch;
}

/**
 * @brief Retarget ferror (required by microLIB)
 */
int ferror(FILE *f)
{
    (void)f;
    return 0;  /* No errors */
}

#endif /* __ARMCC_VERSION */

void UartDebug_Init(void)
{
    /* UART0 is already initialized by SYSCFG_DL_init() */
    /* This function exists for API completeness */
}

void UartDebug_PutChar(char c)
{
    /* Wait until TX FIFO has space */
    while (DL_UART_isTXFIFOFull(UART0_INST));

    /* Transmit the character */
    DL_UART_Main_transmitData(UART0_INST, c);
}

void UartDebug_Print(const char *str)
{
    if (str == NULL) return;

    while (*str) {
        UartDebug_PutChar(*str++);
    }
}

void UartDebug_Println(const char *str)
{
    UartDebug_Print(str);
    UartDebug_PutChar('\r');
    UartDebug_PutChar('\n');
}

void UartDebug_PrintU32(uint32_t value)
{
    char buffer[12];  /* Max: 4294967295 = 10 digits + sign + null */
    int idx = 0;

    if (value == 0) {
        UartDebug_PutChar('0');
        return;
    }

    /* Convert to string (reverse order) */
    uint32_t temp = value;
    while (temp > 0) {
        buffer[idx++] = '0' + (temp % 10);
        temp /= 10;
    }

    /* Print in correct order */
    for (int i = idx - 1; i >= 0; i--) {
        UartDebug_PutChar(buffer[i]);
    }
}

void UartDebug_PrintI32(int32_t value)
{
    if (value < 0) {
        UartDebug_PutChar('-');
        value = -value;
    }
    UartDebug_PrintU32((uint32_t)value);
}

void UartDebug_PrintHex(uint32_t value, bool prefix)
{
    if (prefix) {
        UartDebug_Print("0x");
    }

    const char hex_chars[] = "0123456789ABCDEF";
    char buffer[8];

    for (int i = 7; i >= 0; i--) {
        buffer[i] = hex_chars[value & 0xF];
        value >>= 4;
    }

    /* Skip leading zeros, but always print at least one digit */
    int start = 0;
    while (start < 7 && buffer[start] == '0') {
        start++;
    }

    for (int i = start; i < 8; i++) {
        UartDebug_PutChar(buffer[i]);
    }
}

void UartDebug_PrintFloat(float value)
{
    /* Handle negative */
    if (value < 0.0f) {
        UartDebug_PutChar('-');
        value = -value;
    }

    /* Integer part */
    uint32_t int_part = (uint32_t)value;
    UartDebug_PrintU32(int_part);

    /* Decimal point */
    UartDebug_PutChar('.');

    /* Fractional part (3 decimal places) */
    float frac = value - (float)int_part;
    for (int i = 0; i < 3; i++) {
        frac *= 10.0f;
        uint32_t digit = (uint32_t)frac;
        UartDebug_PutChar('0' + (digit % 10));
    }
}
