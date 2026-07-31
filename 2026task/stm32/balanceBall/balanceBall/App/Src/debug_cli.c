/* App/Src/debug_cli.c */
#include "debug_cli.h"
#include "balance_motor.h"
#include "emm_v5_protocol.h"
#include "emm_v5_uart.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>

/* Globals from main.c */
extern BalanceMotor g_balance_motor;
extern EmmV5Uart g_emm_uart;

void debug_cli_init(DebugCli *cli, UART_HandleTypeDef *uart)
{
    cli->uart = uart;
}

void debug_cli_printf(DebugCli *cli, const char *fmt, ...)
{
    char buf[DEBUG_CLI_TX_BUF];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (len > 0) {
        HAL_UART_Transmit(cli->uart, (uint8_t *)buf, (uint16_t)len, 100U);
    }
}

/* Split line into argc/argv (modifies a local copy of line) */
static int tokenize(char *buf, char **argv, int max_argc)
{
    int argc = 0;
    char *p = buf;
    while (*p && argc < max_argc) {
        /* skip whitespace */
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0') break;
        argv[argc++] = p;
        /* find end of token */
        while (*p && *p != ' ' && *p != '\t') p++;
        if (*p) { *p = '\0'; p++; }
    }
    return argc;
}

/* --- en: enable motor --- */
static void cmd_enable(DebugCli *cli, int argc, char **argv)
{
    (void)argc; (void)argv;
    uint8_t frame_buf[EMM_V5_MAX_FRAME_SIZE];
    EmmV5Frame frame = { frame_buf, sizeof(frame_buf), 0 };

    if (emm_v5_encode_enable(g_balance_motor.config.address, true, false, &frame) != EMM_V5_OK) {
        debug_cli_printf(cli, "ERR: encode failed\r\n");
        return;
    }
    BalanceMotorTxResult r = emm_v5_uart_send(
        &g_emm_uart, frame.data, frame.length, 0xF3U, 3U);
    if (r == BALANCE_MOTOR_TX_ACCEPTED) {
        debug_cli_printf(cli, "OK motor enabled\r\n");
    } else {
        debug_cli_printf(cli, "ERR: uart busy or failed (%d)\r\n", (int)r);
    }
}

/* --- dis: disable motor --- */
static void cmd_disable(DebugCli *cli, int argc, char **argv)
{
    (void)argc; (void)argv;
    BalanceMotorResult r = balance_motor_disable(&g_balance_motor);
    if (r == BALANCE_MOTOR_OK || r == BALANCE_MOTOR_QUEUED) {
        debug_cli_printf(cli, "OK motor disabled\r\n");
    } else {
        debug_cli_printf(cli, "ERR: disable failed (%d)\r\n", (int)r);
    }
}

/* --- stop: emergency stop --- */
static void cmd_stop(DebugCli *cli, int argc, char **argv)
{
    (void)argc; (void)argv;
    BalanceMotorResult r = balance_motor_stop(&g_balance_motor);
    if (r == BALANCE_MOTOR_OK || r == BALANCE_MOTOR_QUEUED) {
        debug_cli_printf(cli, "OK stopped\r\n");
    } else {
        debug_cli_printf(cli, "ERR: stop failed (%d)\r\n", (int)r);
    }
}

/* --- zero: set current position as software zero --- */
static void cmd_zero(DebugCli *cli, int argc, char **argv)
{
    (void)argc; (void)argv;
    BalanceMotorResult r = balance_motor_request_zero(&g_balance_motor);
    if (r == BALANCE_MOTOR_OK || r == BALANCE_MOTOR_QUEUED) {
        debug_cli_printf(cli, "OK zero requested (wait ~100ms for ACK)\r\n");
    } else {
        debug_cli_printf(cli, "ERR: zero request failed (%d)\r\n", (int)r);
    }
}

/* --- pos <pulses> [speed_rpm] [accel] --- */
static void cmd_pos(DebugCli *cli, int argc, char **argv)
{
    if (argc < 2) {
        debug_cli_printf(cli, "ERR: usage: pos <pulses> [speed_rpm] [accel]\r\n");
        return;
    }
    if (!balance_motor_has_zero(&g_balance_motor)) {
        debug_cli_printf(cli, "ERR: zero not set (run 'zero' first)\r\n");
        return;
    }

    int32_t pos    = (int32_t)atoi(argv[1]);
    float   speed  = (argc >= 3) ? (float)atoi(argv[2]) : 100.0f;
    float   accel  = (argc >= 4) ? (float)atoi(argv[3]) : 10.0f;

    if (speed <= 0.0f || speed > 3000.0f) {
        debug_cli_printf(cli, "ERR: speed must be 1-3000 rpm\r\n");
        return;
    }

    BalanceActuatorCommand cmd;
    cmd.position         = (float)pos;
    cmd.speed            = speed;
    cmd.acceleration     = accel;
    cmd.position_limited = false;
    cmd.slew_limited     = false;

    BalanceMotorResult r = balance_motor_submit(&g_balance_motor, &cmd);
    switch (r) {
        case BALANCE_MOTOR_OK:
        case BALANCE_MOTOR_QUEUED:
            debug_cli_printf(cli, "OK moving to %ld at %.0frpm\r\n", (long)pos, speed);
            break;
        case BALANCE_MOTOR_NOT_ZEROED:
            debug_cli_printf(cli, "ERR: zero not set\r\n"); break;
        case BALANCE_MOTOR_LOCKED:
            debug_cli_printf(cli, "ERR: motor locked (run 'status')\r\n"); break;
        case BALANCE_MOTOR_BUSY:
            debug_cli_printf(cli, "ERR: motor busy\r\n"); break;
        default:
            debug_cli_printf(cli, "ERR: submit failed (%d)\r\n", (int)r); break;
    }
}

/* --- vel <rpm> [accel]  (positive=CW, negative=CCW) --- */
static void cmd_vel(DebugCli *cli, int argc, char **argv)
{
    if (argc < 2) {
        debug_cli_printf(cli, "ERR: usage: vel <rpm> [accel]\r\n");
        return;
    }
    int   rpm_signed = atoi(argv[1]);
    uint8_t accel    = (argc >= 3) ? (uint8_t)atoi(argv[2]) : 10U;

    EmmV5Direction dir = (rpm_signed >= 0) ? EMM_V5_DIRECTION_CW : EMM_V5_DIRECTION_CCW;
    uint16_t speed_rpm = (uint16_t)abs(rpm_signed);

    if (speed_rpm > 3000U) {
        debug_cli_printf(cli, "ERR: |rpm| must be <= 3000\r\n");
        return;
    }

    uint8_t frame_buf[EMM_V5_MAX_FRAME_SIZE];
    EmmV5Frame frame = { frame_buf, sizeof(frame_buf), 0 };

    if (emm_v5_encode_velocity(g_balance_motor.config.address,
                               dir, speed_rpm, accel, false, &frame) != EMM_V5_OK) {
        debug_cli_printf(cli, "ERR: encode failed\r\n");
        return;
    }
    BalanceMotorTxResult r = emm_v5_uart_send(
        &g_emm_uart, frame.data, frame.length, 0xF6U, 3U);
    if (r == BALANCE_MOTOR_TX_ACCEPTED) {
        debug_cli_printf(cli, "OK velocity %d rpm (%s)\r\n",
                         abs(rpm_signed),
                         (dir == EMM_V5_DIRECTION_CW) ? "CW" : "CCW");
    } else {
        debug_cli_printf(cli, "ERR: uart busy (%d)\r\n", (int)r);
    }
}

/* --- query: read current encoder position from driver --- */
static void cmd_query(DebugCli *cli, int argc, char **argv)
{
    (void)argc; (void)argv;
    uint8_t frame_buf[EMM_V5_MAX_FRAME_SIZE];
    EmmV5Frame frame = { frame_buf, sizeof(frame_buf), 0 };

    if (emm_v5_encode_position_query(g_balance_motor.config.address, &frame) != EMM_V5_OK) {
        debug_cli_printf(cli, "ERR: encode failed\r\n");
        return;
    }
    /* expected response: addr(1) + 0x36(1) + pos_be(4) + 0x6B(1) = 7 bytes */
    BalanceMotorTxResult r = emm_v5_uart_send(
        &g_emm_uart, frame.data, frame.length, 0x36U, 7U);
    if (r == BALANCE_MOTOR_TX_ACCEPTED) {
        debug_cli_printf(cli, "OK query sent (result printed when ACK arrives)\r\n");
    } else {
        debug_cli_printf(cli, "ERR: uart busy (%d)\r\n", (int)r);
    }
}

/* --- status: print internal state --- */
static void cmd_status(DebugCli *cli, int argc, char **argv)
{
    (void)argc; (void)argv;
    static const char *uart_state_name[] = {
        "IDLE", "ACTIVE", "COMPLETE", "TIMEOUT", "PROTOCOL_ERROR", "HAL_ERROR"
    };
    const char *ustate = (g_emm_uart.state <= EMM_V5_UART_HAL_ERROR)
                       ? uart_state_name[g_emm_uart.state] : "UNKNOWN";

    debug_cli_printf(cli,
        "OK status\r\n"
        "  zero_valid:   %s\r\n"
        "  zero_position:%ld\r\n"
        "  locked:       %s\r\n"
        "  failures:     %u\r\n"
        "  uart_state:   %s\r\n",
        g_balance_motor.zero_valid  ? "true" : "false",
        (long)g_balance_motor.zero_position,
        g_balance_motor.locked      ? "true" : "false",
        (unsigned)g_balance_motor.consecutive_failures,
        ustate);
}

/* --- help --- */
static void cmd_help(DebugCli *cli, int argc, char **argv)
{
    (void)argc; (void)argv;
    debug_cli_printf(cli,
        "Commands:\r\n"
        "  en                  Enable motor\r\n"
        "  dis                 Disable motor\r\n"
        "  pos <p> [v] [a]     Move to position p (pulses), v=rpm(def 100), a=accel(def 10)\r\n"
        "  vel <rpm> [a]       Velocity mode (+CW/-CCW), a=accel(def 10)\r\n"
        "  stop                Emergency stop\r\n"
        "  query               Send position query (async, see result on next ACK)\r\n"
        "  zero                Set current encoder pos as software zero\r\n"
        "  status              Show motor+uart internal state\r\n"
        "  help                This message\r\n");
}

typedef struct {
    const char *name;
    void (*handler)(DebugCli *cli, int argc, char **argv);
} CliCmd;

static const CliCmd s_commands[] = {
    { "en",     cmd_enable  },
    { "dis",    cmd_disable },
    { "pos",    cmd_pos     },
    { "vel",    cmd_vel     },
    { "stop",   cmd_stop    },
    { "query",  cmd_query   },
    { "zero",   cmd_zero    },
    { "status", cmd_status  },
    { "help",   cmd_help    },
    { NULL, NULL }
};

void debug_cli_process_line(DebugCli *cli, const char *line, uint16_t len)
{
    if (len == 0U) return;

    /* Copy to mutable buffer, strip CR/LF */
    char buf[DEBUG_CLI_RX_BUF];
    uint16_t copy_len = (len < DEBUG_CLI_RX_BUF - 1U) ? len : (DEBUG_CLI_RX_BUF - 1U);
    memcpy(buf, line, copy_len);
    buf[copy_len] = '\0';
    /* strip trailing CR/LF */
    for (int i = (int)copy_len - 1; i >= 0; i--) {
        if (buf[i] == '\r' || buf[i] == '\n') buf[i] = '\0'; else break;
    }

    /* Tokenize */
    char *argv[DEBUG_CLI_MAX_ARGC];
    int argc = tokenize(buf, argv, (int)DEBUG_CLI_MAX_ARGC);
    if (argc == 0) return;

    /* Convert command name to lower case */
    for (char *p = argv[0]; *p; p++) *p = (char)tolower((unsigned char)*p);

    /* Dispatch */
    for (const CliCmd *cmd = s_commands; cmd->name != NULL; cmd++) {
        if (strcmp(argv[0], cmd->name) == 0) {
            cmd->handler(cli, argc, argv);
            return;
        }
    }
    debug_cli_printf(cli, "ERR: unknown command '%s' (type 'help')\r\n", argv[0]);
}
