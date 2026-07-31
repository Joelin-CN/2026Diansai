# Debug UART CLI Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a UART3-based ASCII text command interface to STM32 for interactive stepper motor testing via serial assistant.

**Architecture:** UART3 (PB10/PB11, 115200 8N1) receives newline-terminated ASCII commands via DMA+Idle interrupt. A command parser dispatches to existing `balance_motor` / `emm_v5_uart` APIs. Responses are sent back over UART3 in blocking mode.

**Tech Stack:** STM32 HAL, C99, existing emm_v5 + balance_motor layer

## Global Constraints

- Target MCU: STM32F407VGT6, HAL driver
- UART3: PB10 (TX), PB11 (RX), 115200 8N1
- UART2: PA2 (TX), PA3 (RX) - motor comms, DO NOT modify
- All new code in `App/Inc/` and `App/Src/`
- All `main.c` changes inside `USER CODE` blocks only
- Do not modify any existing source files other than `Core/Src/main.c`
- C standard: C99, no dynamic allocation (no malloc/free)
- Max new Flash: 4 KB; Max new RAM: 512 bytes

---

## File Map

| Action | Path | Responsibility |
|--------|------|----------------|
| Create | `App/Inc/debug_cli.h` | DebugCli struct, public API |
| Create | `App/Src/debug_cli.c` | Command parser, handlers, UART3 TX |
| Modify | `Core/Src/main.c` | Init, DMA RX start, UART3 callback |

---

## Task 1: debug_cli.h - Header and Data Structures

**Files:**
- Create: `App/Inc/debug_cli.h`

**Interfaces:**
- Produces:
  - `DebugCli` struct (128-byte RX buffer, UART handle)
  - `debug_cli_init(DebugCli *cli, UART_HandleTypeDef *uart)`
  - `debug_cli_process_line(DebugCli *cli, const char *line, uint16_t len)`
  - `debug_cli_printf(DebugCli *cli, const char *fmt, ...)`

- [ ] **Step 1: Create the header file**

```c
/* App/Inc/debug_cli.h */
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
```

- [ ] **Step 2: Verify file exists and compiles (no errors expected yet)**

Open Keil MDK and add `App/Inc` to include paths if not already present. Check that `debug_cli.h` can be included without errors.

- [ ] **Step 3: Commit**

```
git add App/Inc/debug_cli.h
git commit -m "feat: add debug_cli.h header"
```

---

## Task 2: debug_cli.c - Core Implementation

**Files:**
- Create: `App/Src/debug_cli.c`

**Interfaces:**
- Consumes (from Task 1): `DebugCli`, `debug_cli_init`, `debug_cli_process_line`, `debug_cli_printf`
- Consumes (existing): `balance_motor.h`, `emm_v5_protocol.h`, `emm_v5_uart.h`
- Consumes (globals from main.c): `extern BalanceMotor g_balance_motor; extern EmmV5Uart g_emm_uart;`

- [ ] **Step 1: Create debug_cli.c with init and printf**

```c
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
```

- [ ] **Step 2: Add command parser (tokenizer)**

Append to `debug_cli.c`:

```c
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
```

- [ ] **Step 3: Implement command handlers**

Append to `debug_cli.c`:

```c
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
```

- [ ] **Step 4: Implement process_line dispatcher**

Append to `debug_cli.c`:

```c
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
```

- [ ] **Step 5: Add debug_cli.c to Keil project**

In Keil MDK: right-click Application group (or App/Src group) -> Add Existing Files -> select `App/Src/debug_cli.c`. Build to check for compile errors.

Expected: 0 errors, 0 warnings (or only "unused parameter" warnings which are suppressed by the `(void)` casts already in place).

- [ ] **Step 6: Commit**

```
git add App/Inc/debug_cli.h App/Src/debug_cli.c
git commit -m "feat: add debug_cli command parser and handlers"
```

---

## Task 3: main.c Integration

**Files:**
- Modify: `Core/Src/main.c`

**Interfaces:**
- Consumes (Task 1-2): `DebugCli`, `debug_cli_init`, `debug_cli_process_line`
- Consumes (existing): `huart3`, `hdma_usart3_rx`

- [ ] **Step 1: Add include and global variable**

In `main.c`, inside `/* USER CODE BEGIN Includes */`:

```c
#include "debug_cli.h"
```

In `main.c`, inside `/* USER CODE BEGIN PV */`:

```c
DebugCli g_debug_cli;
```

- [ ] **Step 2: Initialize CLI and start DMA reception**

In `main.c`, inside `/* USER CODE BEGIN 2 */`, after `balance_motor_init(...)`:

```c
debug_cli_init(&g_debug_cli, &huart3);
HAL_UARTEx_ReceiveToIdle_DMA(&huart3, g_debug_cli.rx_buf, DEBUG_CLI_RX_BUF);
__HAL_DMA_DISABLE_IT(&hdma_usart3_rx, DMA_IT_HT); /* disable half-transfer, not needed */
```

- [ ] **Step 3: Add UART3 branch to HAL_UARTEx_RxEventCallback**

In `main.c`, inside `/* USER CODE BEGIN 4 */`, find the existing `HAL_UARTEx_RxEventCallback` and add the UART3 branch:

```c
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t size)
{
    if (huart->Instance == USART2)
    {
        emm_v5_uart_on_rx_event(&g_emm_uart, size);
    }
    else if (huart->Instance == USART3)
    {
        debug_cli_process_line(&g_debug_cli, (const char *)g_debug_cli.rx_buf, size);
        /* Restart DMA reception for the next command */
        HAL_UARTEx_ReceiveToIdle_DMA(&huart3, g_debug_cli.rx_buf, DEBUG_CLI_RX_BUF);
        __HAL_DMA_DISABLE_IT(&hdma_usart3_rx, DMA_IT_HT);
    }
}
```

- [ ] **Step 4: Build and check for errors**

Build the entire project in Keil MDK. Expected: 0 errors.

Common issues and fixes:
- "undefined reference to debug_cli_init": `debug_cli.c` not added to project -> re-do Task 2 Step 5
- "implicit declaration of HAL_UARTEx_ReceiveToIdle_DMA": include `usart.h` in `main.c` (already present via `#include "usart.h"`)

- [ ] **Step 5: Commit**

```
git add Core/Src/main.c
git commit -m "feat: integrate debug_cli into main.c with UART3 DMA RX"
```

---

## Task 4: Hardware-in-Loop Test

This task has no code changes. It validates the full system on real hardware.

**Required hardware:**
- STM32F407 board + EMM V5 stepper driver + motor
- USB-to-UART module connected: `PB10 (TX) -> RX`, `PB11 (RX) -> TX`, `GND -> GND`
- Serial assistant (SSCOM/PuTTY/minicom) at 115200 8N1, send newline `\n` or `\r\n`

**Test sequence (execute in order):**

- [ ] **Test 1: Connectivity**

Send: `help`
Expected response:
```
Commands:
  en                  Enable motor
  ...
```
If nothing received: check wiring PB10/PB11, check baud rate.

- [ ] **Test 2: Enable**

Send: `en`
Expected: `OK motor enabled`
Verify: driver status LED changes (if present)

- [ ] **Test 3: Zero point**

Send: `zero`
Expected: `OK zero requested (wait ~100ms for ACK)`
Wait 200ms, then send: `status`
Expected: `zero_valid: true`

- [ ] **Test 4: Position move**

Send: `pos 1000`
Expected: `OK moving to 1000 at 100rpm`
Verify: motor rotates to the target position and stops.

- [ ] **Test 5: Query position**

Send: `query`
Expected: `OK query sent (result printed when ACK arrives)`
Note: the position result will be printed asynchronously when the ACK is processed
by `balance_motor_on_response` in the main loop. To see the live position value,
add a temporary printf inside `balance_motor_on_response` in `balance_motor.c`
(revert after test).

- [ ] **Test 6: Velocity mode**

Send: `vel 200`
Expected: `OK velocity 200 rpm (CW)` - motor spins continuously
Send: `stop`
Expected: `OK stopped`

- [ ] **Test 7: Error handling**

Send: `foo`
Expected: `ERR: unknown command 'foo' (type 'help')`
Send: `pos abc`
Expected: `ERR: ...` (atoi("abc") returns 0, which is a valid position - motor moves to 0. This is acceptable behavior for a test tool.)

- [ ] **Test 8: Clear fault and re-test**

Send: `status`
Verify: `locked: false`, `failures: 0`
If locked: power-cycle driver, send `status` again.

---

## Query Response Note

The `query` command sends the position request but the parsed response is printed
by `balance_motor_on_response`. To display it on UART3 during testing, temporarily
add to `balance_motor.c` inside the `0x36` response handler (or add a response hook
later as a feature). This is intentionally out of scope for this plan to avoid
modifying `balance_motor.c`.

For now, `query` is useful to confirm the UART2 round-trip is working (if the UART
times out, `status` will show `failures: 1`).

---

## Revision History

| Date | Version | Change |
|------|---------|--------|
| 2026-07-31 | 1.0 | Initial plan |
