#ifndef DEBUG_CLI_H
#define DEBUG_CLI_H

#include <stdint.h>
#include <stdbool.h>
#include "usart.h"

#define DEBUG_CLI_RX_BUF  128U
#define DEBUG_CLI_TX_BUF  256U
#define DEBUG_CLI_MAX_ARGC  8U

typedef struct {
    UART_HandleTypeDef *uart;
    uint8_t rx_buf[DEBUG_CLI_RX_BUF];
} DebugCli;

void debug_cli_init(DebugCli *cli, UART_HandleTypeDef *uart);
void debug_cli_process_line(DebugCli *cli, const char *line, uint16_t len);
void debug_cli_printf(DebugCli *cli, const char *fmt, ...);

#endif /* DEBUG_CLI_H */
