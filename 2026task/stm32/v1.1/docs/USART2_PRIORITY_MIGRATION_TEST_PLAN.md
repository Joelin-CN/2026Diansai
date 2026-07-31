# USART2 Priority Migration Test Plan
# From Priority 3 → Priority 6

**Objective:** Validate that USART2 can safely operate at Priority 6 (within FreeRTOS boundary) without data loss.

**Current Status:** Priority 3 (working, but limits future RTOS API usage)  
**Target Status:** Priority 6 (allows RTOS API calls)

---

## Prerequisites

Before attempting migration:

1. **Measure Critical Section Duration**
   - Instrument `encoder_hw_bridge.c` critical section
   - Use GPIO toggle + oscilloscope OR cycle counter
   - Record maximum duration

2. **Decision Criteria**
   ```
   Max Critical Section < 50 μs  → Safe to migrate
   Max Critical Section > 80 μs  → Keep Priority 3
   ```

---

## Phase 1: Critical Section Measurement

### Method A: GPIO Toggle (Recommended)

```c
// In encoder_hw_bridge.c
void Encoder_GetCountsAtomic(int32_t *left, int32_t *right)
{
    HAL_GPIO_WritePin(DEBUG_GPIO_Port, DEBUG_Pin, GPIO_PIN_SET);  // Start marker
    
    taskENTER_CRITICAL();
    *left = g_encoder_left_count;
    *right = g_encoder_right_count;
    taskEXIT_CRITICAL();
    
    HAL_GPIO_WritePin(DEBUG_GPIO_Port, DEBUG_Pin, GPIO_PIN_RESET);  // End marker
}
```

**Measurement:**
- Use oscilloscope on DEBUG_Pin
- Run vehicle in loop tracking mode for 5 minutes
- Record maximum pulse width
- Add 20% safety margin

### Method B: Cycle Counter (Software)

```c
void Encoder_GetCountsAtomic(int32_t *left, int32_t *right)
{
    uint32_t start = DWT->CYCCNT;
    
    taskENTER_CRITICAL();
    *left = g_encoder_left_count;
    *right = g_encoder_right_count;
    taskEXIT_CRITICAL();
    
    uint32_t cycles = DWT->CYCCNT - start;
    uint32_t us = cycles / 168;  // 168 MHz clock
    
    if (us > g_max_critical_us) {
        g_max_critical_us = us;
    }
}
```

**Enable DWT counter in main():**
```c
CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
DWT->CYCCNT = 0;
DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
```

---

## Phase 2: Code Modification

### Step 1: Create Test Branch

```bash
git checkout -b test/usart2-priority6
```

### Step 2: Modify Priority

**File:** `Core/Src/usart.c:367`

```diff
  /* USART2 interrupt Init */
- // ⚠️  Priority 3 is HIGHER than FreeRTOS syscall boundary (5)
- HAL_NVIC_SetPriority(USART2_IRQn, 3, 0);
+ // Priority 6 - within FreeRTOS boundary (allows RTOS API calls)
+ // Migration test: verify no ORE errors under load
+ HAL_NVIC_SetPriority(USART2_IRQn, 6, 0);
```

### Step 3: Add Diagnostic Counters

**File:** `Core/Src/stm32f4xx_it.c`

```c
// At file scope
static uint32_t g_usart2_ore_count = 0;
static uint32_t g_usart2_rx_count = 0;

void USART2_IRQHandler(void)
{
    if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_RXNE)) {
        uint8_t received_byte = (uint8_t)(huart2.Instance->DR & 0xFF);
        g_usart2_rx_count++;  // Count received bytes
        
        IrUartSensor_RxByte(received_byte);
        IrUartDiag_CountRxByte(received_byte);
        IrRawCapture_RxByte(received_byte);
    }
    
    if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_ORE)) {
        g_usart2_ore_count++;  // Count overrun errors
        __HAL_UART_CLEAR_OREFLAG(&huart2);
    }
    
    // ... rest of handler
}

// Add getter functions
uint32_t USART2_GetOreCount(void) { return g_usart2_ore_count; }
uint32_t USART2_GetRxCount(void) { return g_usart2_rx_count; }
```

### Step 4: Add Monitoring Task

```c
// In freertos.c or control_app.c
void MonitorUSART2(void)
{
    static uint32_t last_ore = 0;
    static uint32_t last_rx = 0;
    
    uint32_t ore = USART2_GetOreCount();
    uint32_t rx = USART2_GetRxCount();
    
    if (ore != last_ore) {
        printf("[ERROR] USART2 ORE detected! Total: %lu\r\n", ore);
    }
    
    uint32_t rx_rate = rx - last_rx;  // Bytes per second (if called every 1s)
    printf("[USART2] RX rate: %lu bytes/s (expect ~7375)\r\n", rx_rate);
    
    last_ore = ore;
    last_rx = rx;
}
```

---

## Phase 3: Testing Protocol

### Test 1: Idle Test (2 minutes)

**Setup:**
- Vehicle stationary
- IR sensor streaming data

**Monitor:**
- ORE count (must be 0)
- RX rate (~125 frames/s * 59 bytes = 7375 bytes/s)

**Pass Criteria:**
- ORE count = 0
- RX rate = 7000-8000 bytes/s

### Test 2: Normal Operation (5 minutes)

**Setup:**
- Vehicle running loop tracking
- All sensors active
- Control loop at 500 Hz

**Monitor:**
- ORE count (must be 0)
- Frame parse success rate
- Tracking performance

**Pass Criteria:**
- ORE count = 0
- Frame parse rate > 99%
- No tracking anomalies

### Test 3: Stress Test (10 minutes)

**Setup:**
- Continuous loop tracking
- printf() every 100ms (stress UART5)
- Full load on all peripherals

**Monitor:**
- ORE count over time
- System responsiveness

**Pass Criteria:**
- ORE count = 0 throughout
- No system lockups
- No tracking failures

### Test 4: Long Duration (30 minutes)

**Setup:**
- Extended tracking operation
- Typical competition load

**Monitor:**
- ORE accumulation
- Overall stability

**Pass Criteria:**
- ORE count = 0
- No degradation over time

---

## Phase 4: Validation Metrics

### Success Criteria (ALL must pass)

| Metric | Target | Measured | Status |
|--------|--------|----------|--------|
| Max Critical Section | < 50 μs | ___ μs | ⬜ |
| ORE Count (Idle) | 0 | ___ | ⬜ |
| ORE Count (Normal) | 0 | ___ | ⬜ |
| ORE Count (Stress) | 0 | ___ | ⬜ |
| ORE Count (30min) | 0 | ___ | ⬜ |
| Frame Parse Rate | > 99% | ___% | ⬜ |
| Tracking Performance | No regression | Pass/Fail | ⬜ |

### If ANY Test Fails

1. Document failure mode
2. Revert to Priority 3
3. Update audit report with findings
4. Keep Priority 3 as permanent configuration

### If ALL Tests Pass

1. Merge test branch to main
2. Update documentation:
   - Remove Priority 3 warnings
   - Update interrupt priority table
   - Note migration date in changelog
3. Close this test plan

---

## Phase 5: Post-Migration Benefits

After successful migration to Priority 6:

### New Capabilities Unlocked

```c
// USART2_IRQHandler can now safely call:

// 1. Signal processing completion
void USART2_IRQHandler(void)
{
    if (frame_complete) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(g_ir_frame_ready, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

// 2. Queue data directly
xQueueSendFromISR(g_ir_data_queue, &frame, &xHigherPriorityTaskWoken);

// 3. Notify tasks
vTaskNotifyGiveFromISR(g_ir_task_handle, &xHigherPriorityTaskWoken);
```

### Architectural Improvements

- Cleaner separation: ISR signals, task processes
- Reduced main loop polling
- More responsive system
- Easier to add features (e.g., frame timestamp)

---

## Rollback Plan

If issues discovered after deployment:

```bash
# Quick revert
git revert HEAD

# Or manual fix
# In Core/Src/usart.c:367
HAL_NVIC_SetPriority(USART2_IRQn, 3, 0);  // Back to Priority 3
```

Rebuild and reflash immediately.

---

## Measurement Template

```
=== USART2 Priority 6 Migration Test Results ===

Date: ___________
Tester: ___________
Firmware Version: ___________

Phase 1: Critical Section Measurement
- Measurement method: [ ] GPIO [ ] Cycle Counter
- Max duration: _______ μs
- Safety margin (20%): _______ μs
- Decision: [ ] Proceed [ ] Abort

Phase 3: Testing Results
Test 1 (Idle, 2min):
  - ORE count: _______
  - RX rate: _______ bytes/s
  - Status: [ ] PASS [ ] FAIL

Test 2 (Normal, 5min):
  - ORE count: _______
  - Frame parse rate: _______%
  - Status: [ ] PASS [ ] FAIL

Test 3 (Stress, 10min):
  - ORE count: _______
  - System stability: [ ] Stable [ ] Issues
  - Status: [ ] PASS [ ] FAIL

Test 4 (Long, 30min):
  - ORE count: _______
  - Performance degradation: [ ] None [ ] Observed
  - Status: [ ] PASS [ ] FAIL

Overall Result: [ ] ALL PASS - MIGRATE [ ] ANY FAIL - REVERT

Notes:
_____________________________________________________________
_____________________________________________________________
_____________________________________________________________

Signed: _______________  Date: _______________
```

---

**Test Plan Version:** 1.0  
**Created:** 2026-07-30  
**Status:** Ready for execution  
**Estimated Duration:** 1-2 hours (including measurement and testing)
