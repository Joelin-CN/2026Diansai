# STM32 FreeRTOS Interrupt Priority Configuration Guide

**Project:** STM32F407 Smart Car
**Date:** 2026-07-30
**FreeRTOS Version:** v10.x

## Table of Contents

1. [FreeRTOS Priority Rules](#freertos-priority-rules)
2. [Current Configuration](#current-configuration)
3. [Safety Guidelines](#safety-guidelines)
4. [Troubleshooting](#troubleshooting)

---

## FreeRTOS Priority Rules

### Hardware Configuration

- **MCU:** STM32F407 (Cortex-M4F)
- **Priority Bits:** 4 bits (0-15, where 0 is highest)
- **Priority Group:** NVIC_PRIORITYGROUP_4 (4 bits preemption, 0 bits subpriority)

### FreeRTOS Configuration

```c
configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY = 5
configPRIO_BITS = 4
```

### Priority Zones

```
Priority 0 ┐
Priority 1 │  ⚠️  CRITICAL ZONE (Above FreeRTOS boundary)
Priority 2 │  - NOT masked by taskENTER_CRITICAL()
Priority 3 │  - CANNOT call ANY FreeRTOS API
Priority 4 ┘  - Use ONLY for time-critical ISRs

Priority 5 ──  🔴 FreeRTOS Syscall Boundary

Priority 6 ┐
Priority 7 │
  ...      │  ✅ SAFE ZONE (Protected by FreeRTOS)
Priority 14│  - Can call FreeRTOS API safely
Priority 15┘  - May be blocked by taskENTER_CRITICAL()
```

### Critical Rule

**❌ If your ISR priority is 0-4:**
- You CANNOT call ANY FreeRTOS API
- No `osDelay()`, `osSemaphoreAcquire()`, `osMessageQueuePut()`, etc.
- No `xSemaphoreGive()`, `xQueueSend()`, `vTaskNotifyGive()`, etc.
- No HAL functions that internally use RTOS (e.g., HAL with RTOS mutex)
- Violation causes HardFault or undefined behavior

**✅ If your ISR priority is 5-15:**
- You CAN safely call FreeRTOS API (use FromISR variants)
- Your ISR may be delayed by `taskENTER_CRITICAL()` sections
- Worst-case latency = longest critical section duration

---

## Current Configuration

### Interrupt Priority Table

| IRQ Source | Priority | RTOS API? | Peripheral | Purpose | Notes |
|------------|----------|-----------|------------|---------|-------|
| **USART2** | **3** | ❌ NO | PA2/PA3 | IR Sensor (125Hz) | ⚠️ See [USART2 Special Case](#usart2-special-case) |
| UART4 | 5 | ✅ Yes | PC10/PC11 | Reserved | Standard config |
| UART5 | 5 | ✅ Yes | PC12/PD2 | printf() debug | Standard config |
| USART3 | 5 | ✅ Yes | PB10/PB11 | General UART | Standard config |
| USART6 | 5 | ✅ Yes | PC6/PC7 | General UART | Standard config |
| DMA1_Stream0-7 | 5 | ✅ Yes | DMA1 | UART/SPI DMA | Standard config |
| DMA2_Stream1,6 | 5 | ✅ Yes | DMA2 | UART DMA | Standard config |
| EXTI0-4 | 5 | ✅ Yes | Various | Key buttons | Standard config |
| EXTI9_5 | 5 | ✅ Yes | PE8 | IMU INT1 | Standard config |
| TIM6_DAC | 15 (typical) | ✅ Yes | TIM6 | HAL timebase | Lowest priority |
| PendSV | 15 | ✅ Yes | Cortex-M4 | FreeRTOS scheduler | Lowest priority |
| SysTick | 15 | ✅ Yes | Cortex-M4 | FreeRTOS tick | Lowest priority |

### USART2 Special Case

**Why Priority 3?**

USART2 handles the 8-way infrared sensor module with strict timing requirements:
- **Baud rate:** 115200 bps
- **Frame rate:** 125 Hz (8ms period)
- **Frame size:** ~59 bytes
- **Byte interval:** 87 μs

If the ISR is delayed beyond 87μs, the UART hardware triggers an Overrun Error (ORE) and loses data.

**Design Decision:**
- Use Priority 3 (above FreeRTOS boundary) to ensure low-latency response
- Sacrifice RTOS API access for guaranteed data integrity
- Current implementation is verified safe (no RTOS calls)

**Configured in:**
- `Core/Src/usart.c:367` - HAL MSP initialization
- See detailed warning in `Core/Src/stm32f4xx_it.c:360-410`

**Alternative Considered:**
- Priority 6 would allow RTOS calls but risks ORE under heavy `taskENTER_CRITICAL()` usage
- Would need measurement: if longest critical section < 50μs, Priority 6 is safe

---

## Safety Guidelines

### For Priority 0-4 (Critical ISRs)

**DO:**
- ✅ Direct register reads/writes
- ✅ Simple memory operations (buffers, flags)
- ✅ Call functions that only do register/memory access
- ✅ Clear interrupt flags

**DON'T:**
- ❌ Call ANY FreeRTOS API (os*, x*, v*)
- ❌ Call HAL functions with `_IT` or `_DMA` suffix
- ❌ Call `printf()` (uses UART5 which may have mutex)
- ❌ Use `taskENTER_CRITICAL()` / `taskEXIT_CRITICAL()` (no effect here)
- ❌ Access shared resources without atomic operations

**Template:**
```c
/**
 * @brief MyPeripheral interrupt handler
 * 
 * ⚠️  CRITICAL ISR - Priority 3 (ABOVE FreeRTOS boundary)
 * 
 * RESTRICTIONS:
 * - DO NOT call ANY FreeRTOS API
 * - DO NOT call HAL functions with _IT/_DMA suffix
 * - DO NOT call printf() or functions that use RTOS
 * 
 * CODE REVIEW REQUIRED FOR ANY MODIFICATION
 */
void MyPeripheral_IRQHandler(void)
{
    // Safe: direct register access
    uint8_t data = MY_PERIPHERAL->DR;
    
    // Safe: write to volatile buffer
    g_rx_buffer[g_rx_index++] = data;
    
    // Safe: set atomic flag
    g_data_ready = true;
}
```

### For Priority 5-15 (Standard ISRs)

**DO:**
- ✅ All operations allowed in Priority 0-4
- ✅ Call FreeRTOS API (use FromISR variants)
- ✅ Use HAL IT/DMA functions
- ✅ Signal semaphores, queues, notifications

**Template:**
```c
void Standard_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    // Process interrupt
    uint8_t data = UART->DR;
    
    // Signal RTOS task
    xSemaphoreGiveFromISR(g_data_semaphore, &xHigherPriorityTaskWoken);
    
    // Trigger context switch if needed
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
```

### When to Use Priority 0-4

Only use when **ALL** of these are true:
1. ISR must execute within microseconds
2. Timing is so critical that FreeRTOS blocking is unacceptable
3. The ISR can be implemented with only register/memory operations
4. You have measured that Priority 5-15 causes data loss

**Most peripherals should use Priority 5-15.**

---

## Troubleshooting

### Symptom: HardFault in ISR

**Possible Cause:** Calling FreeRTOS API from Priority 0-4 ISR

**Solution:**
1. Check ISR priority: `HAL_NVIC_GetPriority()`
2. If < 5, either:
   - Change priority to 5 or higher, OR
   - Remove all RTOS API calls

### Symptom: Data Loss / Overrun Errors

**Possible Cause:** ISR delayed by FreeRTOS critical sections

**Solution:**
1. Measure critical section duration
2. If > 50% of byte interval, consider Priority 0-4
3. Or optimize critical sections to be shorter

### Symptom: ISR Never Fires

**Possible Cause:** Interrupt not enabled or wrong priority

**Solution:**
```c
// Check if interrupt is enabled
uint32_t enabled = NVIC->ISER[IRQn / 32] & (1 << (IRQn % 32));

// Check priority
uint32_t priority = HAL_NVIC_GetPriority(IRQn);
```

### Symptom: Configuration Conflicts

**Possible Cause:** Multiple locations setting the same IRQ priority

**Solution:**
- Set priority ONCE in HAL MSP Init (`Core/Src/xxx.c`)
- Do NOT set again in module init functions
- Document the configuration location

---

## Configuration Checklist

When adding a new interrupt:

- [ ] Determine required response time
- [ ] Choose priority zone (0-4 or 5-15)
- [ ] If 0-4: Add warning comments and restrictions
- [ ] If 5-15: Use FromISR API variants
- [ ] Configure in HAL MSP Init (usart.c, tim.c, etc.)
- [ ] Document in this guide's priority table
- [ ] Test under load (critical sections active)

---

## References

- [FreeRTOS Cortex-M Port](https://www.freertos.org/RTOS-Cortex-M3-M4.html)
- [STM32F4 NVIC Documentation](https://www.st.com/resource/en/programming_manual/pm0214-stm32-cortexm4-mcus-and-mpus-programming-manual-stmicroelectronics.pdf)
- Project logs:
  - `logs/2026-07-30_ir_sensor_fix_implementation.md`
  - `logs/2026-07-30_ir_sensor_root_cause_analysis.md`

---

**Document Version:** 1.0  
**Last Updated:** 2026-07-30  
**Maintainer:** System Architecture Team
