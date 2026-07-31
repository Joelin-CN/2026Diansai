#include "competition_timer.h"

#include "main.h"

#include <stdio.h>

#define BUTTON_DEBOUNCE_SAMPLES  10U  /* 10 × 2 ms = 20 ms */
#define TASK_MIN_NUMBER      2U
#define TASK_MAX_NUMBER      6U
#define BUTTON_COUNT          3U
#define BUTTON_LEFT_MASK     (1UL << 0)
#define BUTTON_RIGHT_MASK    (1UL << 1)
#define BUTTON_CONFIRM_MASK  (1UL << 2)

static volatile CompetitionTimerState timer_state;
static volatile uint8_t active_task;
static volatile uint32_t start_tick;
static volatile uint32_t frozen_elapsed_ms;
static volatile uint32_t button_events;
static volatile uint32_t button_event_tick[BUTTON_COUNT];
static uint8_t button_stable_pressed[BUTTON_COUNT];
static uint8_t button_last_sample[BUTTON_COUNT];
static uint8_t button_stable_count[BUTTON_COUNT];

static uint8_t CompetitionTimer_ReadButton(uint8_t index)
{
  GPIO_PinState level;

  switch (index)
  {
    case 0U:
      level = HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin);
      break;
    case 1U:
      level = HAL_GPIO_ReadPin(KEY2_GPIO_Port, KEY2_Pin);
      break;
    case 2U:
      level = HAL_GPIO_ReadPin(KEY3_GPIO_Port, KEY3_Pin);
      break;
    default:
      return 0U;
  }

  /* External key shorts the signal to GND, so low means pressed. */
  return (level == GPIO_PIN_RESET) ? 1U : 0U;
}

static void CompetitionTimer_PollButtons(void)
{
  uint8_t index;

  for (index = 0U; index < BUTTON_COUNT; ++index)
  {
    const uint8_t pressed = CompetitionTimer_ReadButton(index);

    if (pressed != button_last_sample[index])
    {
      button_last_sample[index] = pressed;
      button_stable_count[index] = 0U;
      continue;
    }

    if (button_stable_count[index] < BUTTON_DEBOUNCE_SAMPLES)
    {
      ++button_stable_count[index];
    }

    if ((button_stable_count[index] >= BUTTON_DEBOUNCE_SAMPLES) &&
        (pressed != button_stable_pressed[index]))
    {
      button_stable_pressed[index] = pressed;
      if (pressed != 0U)
      {
        button_event_tick[index] = HAL_GetTick();
        button_events |= 1UL << index;
      }
    }
  }
}

static uint32_t CompetitionTimer_LimitForTask(uint8_t task_number)
{
  switch (task_number)
  {
    case 2U:
      return 20000U;
    case 3U:
      return 5000U;
    case 4U:
      return 8000U;
    case 5U:
    case 6U:
      return 30000U;
    default:
      return 0U;
  }
}

static bool CompetitionTimer_StartAt(uint8_t task_number, uint32_t tick)
{
  uint32_t primask;

  if ((CompetitionTimer_LimitForTask(task_number) == 0U) ||
      (timer_state == COMP_TIMER_INITIALIZING) ||
      (timer_state == COMP_TIMER_RUNNING))
  {
    return false;
  }

  active_task = task_number;
  frozen_elapsed_ms = 0U;
  if (!CompetitionTimer_OnTaskStartRequested(task_number))
  {
    timer_state = COMP_TIMER_UNAVAILABLE;
    return false;
  }

  primask = __get_PRIMASK();
  __disable_irq();
  start_tick = tick;
  timer_state = COMP_TIMER_RUNNING;
  if (primask == 0U)
  {
    __enable_irq();
  }

  return true;
}

static bool CompetitionTimer_FinishAt(uint8_t task_number, uint32_t tick)
{
  uint32_t elapsed;
  uint32_t primask = __get_PRIMASK();

  __disable_irq();
  if ((timer_state != COMP_TIMER_RUNNING) || (active_task != task_number))
  {
    if (primask == 0U)
    {
      __enable_irq();
    }
    return false;
  }

  elapsed = tick - start_tick;
  frozen_elapsed_ms = elapsed;
  timer_state = COMP_TIMER_FINISHED;
  if (primask == 0U)
  {
    __enable_irq();
  }

  CompetitionTimer_OnTaskFinished(task_number, elapsed);
  return true;
}

static void CompetitionTimer_SelectDelta(int8_t delta)
{
  uint32_t primask;
  uint8_t selected;

  if ((timer_state == COMP_TIMER_INITIALIZING) ||
      (timer_state == COMP_TIMER_RUNNING))
  {
    return;
  }

  selected = active_task;
  if (delta < 0)
  {
    selected = (selected <= TASK_MIN_NUMBER)
                   ? TASK_MAX_NUMBER
                   : (uint8_t)(selected - 1U);
  }
  else
  {
    selected = (selected >= TASK_MAX_NUMBER)
                   ? TASK_MIN_NUMBER
                   : (uint8_t)(selected + 1U);
  }

  primask = __get_PRIMASK();
  __disable_irq();
  active_task = selected;
  frozen_elapsed_ms = 0U;
  timer_state = COMP_TIMER_READY;
  if (primask == 0U)
  {
    __enable_irq();
  }
}

void CompetitionTimer_Init(void)
{
  uint8_t index;

  timer_state = COMP_TIMER_INITIALIZING;
  active_task = 2U;
  start_tick = 0U;
  frozen_elapsed_ms = 0U;
  button_events = 0U;
  for (index = 0U; index < BUTTON_COUNT; ++index)
  {
    const uint8_t pressed = CompetitionTimer_ReadButton(index);
    button_event_tick[index] = 0U;
    button_stable_pressed[index] = pressed;
    button_last_sample[index] = pressed;
    button_stable_count[index] = 0U;
  }
}

void CompetitionTimer_MarkReady(void)
{
  uint8_t index;
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  if (timer_state == COMP_TIMER_INITIALIZING)
  {
    /*
     * Never replay a key edge that happened while the IMU was calibrating.
     * The operator must press after READY is visible.
     */
    button_events = 0U;
    for (index = 0U; index < BUTTON_COUNT; ++index)
    {
      const uint8_t pressed = CompetitionTimer_ReadButton(index);
      button_event_tick[index] = 0U;
      button_stable_pressed[index] = pressed;
      button_last_sample[index] = pressed;
      button_stable_count[index] = 0U;
    }
    timer_state = COMP_TIMER_READY;
  }
  if (primask == 0U)
  {
    __enable_irq();
  }
}

void CompetitionTimer_Process(void)
{
  uint32_t events;
  uint32_t confirm_tick;
  uint32_t primask = __get_PRIMASK();

  CompetitionTimer_PollButtons();

  __disable_irq();
  events = button_events;
  confirm_tick = button_event_tick[2];
  button_events = 0U;
  if (primask == 0U)
  {
    __enable_irq();
  }

  /*
   * Menu semantics:
   * KEY1 = previous task, KEY2 = next task, KEY3 = confirm/start.
   * All menu keys are ignored while the vehicle is running.
   */
  if ((timer_state == COMP_TIMER_INITIALIZING) ||
      (timer_state == COMP_TIMER_RUNNING))
  {
    return;
  }

  if ((events & BUTTON_LEFT_MASK) != 0U)
  {
    CompetitionTimer_SelectDelta(-1);
    printf("[KEY] K1 previous -> Task %u\r\n", (unsigned)active_task);
  }
  if ((events & BUTTON_RIGHT_MASK) != 0U)
  {
    CompetitionTimer_SelectDelta(1);
    printf("[KEY] K2 next -> Task %u\r\n", (unsigned)active_task);
  }
  if ((events & BUTTON_CONFIRM_MASK) != 0U)
  {
    printf("[KEY] K3 confirm Task %u\r\n", (unsigned)active_task);
    (void)CompetitionTimer_StartAt(active_task, confirm_tick);
  }
}

bool CompetitionTimer_Start(uint8_t task_number)
{
  return CompetitionTimer_StartAt(task_number, HAL_GetTick());
}

bool CompetitionTimer_Finish(uint8_t task_number)
{
  return CompetitionTimer_FinishAt(task_number, HAL_GetTick());
}

bool CompetitionTimer_Fail(uint8_t task_number)
{
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  if ((timer_state != COMP_TIMER_RUNNING) || (active_task != task_number))
  {
    if (primask == 0U)
    {
      __enable_irq();
    }
    return false;
  }
  frozen_elapsed_ms = HAL_GetTick() - start_tick;
  timer_state = COMP_TIMER_FAULT;
  if (primask == 0U)
  {
    __enable_irq();
  }
  return true;
}

void CompetitionTimer_GetSnapshot(CompetitionTimerSnapshot *snapshot)
{
  CompetitionTimerState state;
  uint8_t task_number;
  uint32_t elapsed;
  uint32_t limit;
  uint32_t primask;

  if (snapshot == NULL)
  {
    return;
  }

  primask = __get_PRIMASK();
  __disable_irq();
  state = timer_state;
  task_number = active_task;
  elapsed = (state == COMP_TIMER_RUNNING)
                ? (HAL_GetTick() - start_tick)
                : frozen_elapsed_ms;
  if (primask == 0U)
  {
    __enable_irq();
  }

  limit = CompetitionTimer_LimitForTask(task_number);
  snapshot->task_number = task_number;
  snapshot->state = state;
  snapshot->elapsed_ms = elapsed;
  snapshot->limit_ms = limit;
  snapshot->within_limit =
      (state == COMP_TIMER_FINISHED) && (elapsed <= limit);
}

void CompetitionTimer_OnButtonInterrupt(uint16_t gpio_pin)
{
  /* Kept as a compatibility hook; menu keys now use deterministic polling. */
  (void)gpio_pin;
}

__weak bool CompetitionTimer_OnTaskStartRequested(uint8_t task_number)
{
  (void)task_number;
  return true;
}

__weak void CompetitionTimer_OnTaskFinished(uint8_t task_number,
                                            uint32_t elapsed_ms)
{
  (void)task_number;
  (void)elapsed_ms;
}

void HAL_GPIO_EXTI_Callback(uint16_t gpio_pin)
{
  CompetitionTimer_OnButtonInterrupt(gpio_pin);
}
