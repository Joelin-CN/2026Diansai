/**
 * @file      test_mspm0_adapter.c
 * @brief     Host-compiled unit tests for the MSPM0 DriverLib adapter
 *
 * This file supplies the fake DriverLib function implementations and verifies
 * that the adapter produces the expected SPI / GPIO / timer transactions.
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>

/* Pull in the fake SysConfig / DriverLib header (provides symbols + decls). */
#include "ti_msp_dl_config.h"

/* SUT */
#include "icm42688_mspm0.h"

/* ----------------------------------------------------------------------
 * Fake DriverLib state - inspectable from tests
 * ---------------------------------------------------------------------- */

typedef enum {
    CS_HIGH = 1,
    CS_LOW  = 0
} cs_level_t;

#define CS_LOG_MAX 16
#define TX_LOG_MAX 32

static cs_level_t cs_events[CS_LOG_MAX];
static size_t     cs_event_count;

static uint8_t tx_bytes[TX_LOG_MAX];
static size_t  tx_count;

static uint8_t  fake_rx_byte;       /* next byte returned by receive */
static uint32_t fake_timer_count;   /* value returned by getTimerCount */
static uint32_t last_delay_cycles;  /* last value handed to delay_cycles */
static uint32_t delay_cycles_call_count; /* number of delay_cycles calls */

static void reset_fake_driverlib(void)
{
    memset(cs_events, 0, sizeof(cs_events));
    cs_event_count = 0;
    memset(tx_bytes, 0, sizeof(tx_bytes));
    tx_count = 0;
    fake_rx_byte      = 0xFFU;
    fake_timer_count  = 0U;
    last_delay_cycles = 0U;
    delay_cycles_call_count = 0U;
}

/* ----------------------------------------------------------------------
 * Fake DriverLib implementations
 * ---------------------------------------------------------------------- */

void DL_GPIO_clearPins(GPIO_Regs *port, uint32_t pins)
{
    (void)port;
    (void)pins;
    if (cs_event_count < CS_LOG_MAX) {
        cs_events[cs_event_count++] = CS_LOW;
    }
}

void DL_GPIO_setPins(GPIO_Regs *port, uint32_t pins)
{
    (void)port;
    (void)pins;
    if (cs_event_count < CS_LOG_MAX) {
        cs_events[cs_event_count++] = CS_HIGH;
    }
}

void DL_SPI_transmitDataBlocking8(SPI_Regs *spi, uint8_t data)
{
    (void)spi;
    if (tx_count < TX_LOG_MAX) {
        tx_bytes[tx_count++] = data;
    }
}

uint8_t DL_SPI_receiveDataBlocking8(SPI_Regs *spi)
{
    (void)spi;
    return fake_rx_byte;
}

uint32_t DL_TimerG_getTimerCount(GPTIMER_Regs *timer)
{
    (void)timer;
    return fake_timer_count;
}

void delay_cycles(uint32_t cycles)
{
    last_delay_cycles = cycles;
    delay_cycles_call_count++;
}

/* ----------------------------------------------------------------------
 * Tests
 * ---------------------------------------------------------------------- */

/* Single register read: CS low, addr|0x80, dummy, CS high. */
static void test_single_register_read_transaction(void)
{
    reset_fake_driverlib();
    fake_rx_byte = 0x47U;
    uint8_t v = icm42688_mspm0_comm.read_reg(0x75U);
    if (v != 0x47U) {
        printf("FAIL: read_reg returned 0x%02X, expected 0x47\n", v);
        assert(0);
    }
    if (cs_events[0] != CS_LOW) {
        printf("FAIL: cs_events[0] != CS_LOW\n");
        assert(0);
    }
    if (tx_bytes[0] != 0xF5U) {
        printf("FAIL: tx_bytes[0] = 0x%02X, expected 0xF5\n", tx_bytes[0]);
        assert(0);
    }
    if (cs_events[1] != CS_HIGH) {
        printf("FAIL: cs_events[1] != CS_HIGH\n");
        assert(0);
    }
}

/* Register write: addr & 0x7F (bit 7 cleared), then data. */
static void test_single_register_write_transaction(void)
{
    reset_fake_driverlib();
    icm42688_mspm0_comm.write_reg(0x75U, 0xABU);

    if (cs_events[0] != CS_LOW || cs_events[cs_event_count - 1] != CS_HIGH) {
        printf("FAIL: write CS framing wrong\n");
        assert(0);
    }
    if (tx_bytes[0] != 0x75U) {
        printf("FAIL: write tx_bytes[0] = 0x%02X, expected 0x75 (bit7 cleared)\n",
               tx_bytes[0]);
        assert(0);
    }
    if (tx_bytes[1] != 0xABU) {
        printf("FAIL: write tx_bytes[1] = 0x%02X, expected 0xAB\n", tx_bytes[1]);
        assert(0);
    }
    /* Only one CS-low / CS-high pair. */
    if (cs_event_count != 2) {
        printf("FAIL: write produced %lu CS events, expected exactly 2\n",
               (unsigned long)cs_event_count);
        assert(0);
    }
}

/* Burst read: exactly one CS-low / CS-high pair, addr|0x80, then len dummies. */
static void test_burst_read_single_cs_window(void)
{
    uint8_t buf[6];
    reset_fake_driverlib();
    fake_rx_byte = 0x11U;

    icm42688_mspm0_comm.read_regs(0x1FU, buf, (uint8_t)sizeof(buf));

    /* Exactly one CS low / high pair. */
    if (cs_event_count != 2) {
        printf("FAIL: burst read produced %lu CS events, expected 2\n",
               (unsigned long)cs_event_count);
        assert(0);
    }
    if (cs_events[0] != CS_LOW || cs_events[1] != CS_HIGH) {
        printf("FAIL: burst CS framing wrong\n");
        assert(0);
    }
    /* First TX byte is addr | 0x80, then 6 dummy 0xFF. */
    if (tx_count != 1U + sizeof(buf)) {
        printf("FAIL: burst tx_count = %lu, expected %lu\n",
               (unsigned long)tx_count, (unsigned long)(1U + sizeof(buf)));
        assert(0);
    }
    if (tx_bytes[0] != (uint8_t)(0x1FU | 0x80U)) {
        printf("FAIL: burst tx_bytes[0] = 0x%02X\n", tx_bytes[0]);
        assert(0);
    }
    for (size_t i = 1; i < tx_count; i++) {
        if (tx_bytes[i] != 0xFFU) {
            printf("FAIL: burst dummy tx_bytes[%lu] = 0x%02X\n",
                   (unsigned long)i, tx_bytes[i]);
            assert(0);
        }
    }
    /* All received bytes equal the fake_rx_byte. */
    for (size_t i = 0; i < sizeof(buf); i++) {
        if (buf[i] != 0x11U) {
            printf("FAIL: buf[%lu] = 0x%02X\n",
                   (unsigned long)i, buf[i]);
            assert(0);
        }
    }
}

/* Burst read with NULL buffer or zero length must not toggle CS. */
static void test_burst_read_null_safe(void)
{
    reset_fake_driverlib();
    icm42688_mspm0_comm.read_regs(0x1FU, NULL, 6U);
    if (cs_event_count != 0) {
        printf("FAIL: NULL buffer burst toggled CS\n");
        assert(0);
    }
    icm42688_mspm0_comm.read_regs(0x1FU, (uint8_t *)0x1, 0U);
    if (cs_event_count != 0) {
        printf("FAIL: zero-length burst toggled CS\n");
        assert(0);
    }
}

/* comm_init pulls CS high (idle). */
static void test_comm_init_pulls_cs_high(void)
{
    reset_fake_driverlib();
    icm42688_mspm0_comm.init();
    if (cs_event_count != 1 || cs_events[0] != CS_HIGH) {
        printf("FAIL: comm_init did not pull CS high\n");
        assert(0);
    }
}

/* Timer returns the SysConfig counter verbatim. */
static void test_timer_returns_sysconfig_counter(void)
{
    fake_timer_count = 123456U;
    if (icm42688_mspm0_timer.get_time_us() != 123456U) {
        printf("FAIL: timer get_time_us did not return counter\n");
        assert(0);
    }
}

/* Timer init/start are no-ops (must not crash, no observable side effects). */
static void test_timer_init_start_are_noops(void)
{
    icm42688_mspm0_timer.init();
    icm42688_mspm0_timer.start();
    /* No assertion to make - just exercise them without faulting. */
}

/* delay_ms calls delay_cycles(CPUCLK_FREQ/1000) per ms. */
static void test_delay_ms_uses_cycles_per_ms(void)
{
    reset_fake_driverlib();
    icm42688_mspm0_system.delay_ms(3U);
    if (last_delay_cycles != CPUCLK_FREQ / 1000U) {
        printf("FAIL: delay_ms handed %lu cycles, expected %lu\n",
               (unsigned long)last_delay_cycles,
               (unsigned long)(CPUCLK_FREQ / 1000U));
        assert(0);
    }
    if (delay_cycles_call_count != 3U) {
        printf("FAIL: delay_cycles called %lu times, expected 3\n",
               (unsigned long)delay_cycles_call_count);
        assert(0);
    }
}

/* delay_ms(0) must not call delay_cycles at all. */
static void test_delay_ms_zero_is_noop(void)
{
    reset_fake_driverlib();
    last_delay_cycles = 0xDEADBEEFU;
    icm42688_mspm0_system.delay_ms(0U);
    if (last_delay_cycles != 0xDEADBEEFU) {
        printf("FAIL: delay_ms(0) called delay_cycles\n");
        assert(0);
    }
}

/* ======================================================================
 * Runner
 * ====================================================================== */

typedef struct {
    const char *name;
    void       (*func)(void);
} test_case_t;

int main(void)
{
    test_case_t tests[] = {
        { "single register read transaction",  test_single_register_read_transaction },
        { "single register write transaction", test_single_register_write_transaction },
        { "burst read single CS window",       test_burst_read_single_cs_window },
        { "burst read NULL safe",              test_burst_read_null_safe },
        { "comm_init pulls CS high",           test_comm_init_pulls_cs_high },
        { "timer returns SysConfig counter",   test_timer_returns_sysconfig_counter },
        { "timer init/start are no-ops",       test_timer_init_start_are_noops },
        { "delay_ms uses cycles per ms",       test_delay_ms_uses_cycles_per_ms },
        { "delay_ms(0) is no-op",              test_delay_ms_zero_is_noop },
    };

    int ntests = (int)(sizeof(tests) / sizeof(tests[0]));
    for (int i = 0; i < ntests; i++) {
        tests[i].func();
        printf("  PASS  %s\n", tests[i].name);
    }

    printf("test_mspm0_adapter: PASS\n");
    return 0;
}
