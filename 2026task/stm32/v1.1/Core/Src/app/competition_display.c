#include "competition_display.h"

#include "oled.h"

#include <stdio.h>

void CompetitionDisplay_Render(const CompetitionTimerSnapshot *snapshot)
{
  char top_line[22];
  char time_text[12];
  uint32_t seconds;
  uint32_t milliseconds;
  uint8_t limit_seconds;

  if (snapshot == NULL)
  {
    return;
  }

  seconds = snapshot->elapsed_ms / 1000U;
  milliseconds = snapshot->elapsed_ms % 1000U;
  limit_seconds = (uint8_t)(snapshot->limit_ms / 1000U);
  if (seconds > 99U)
  {
    seconds = 99U;
    milliseconds = 999U;
  }

  (void)snprintf(top_line,
                 sizeof(top_line),
                 "TASK %u LIMIT %uS",
                 snapshot->task_number,
                 limit_seconds);
  (void)snprintf(time_text,
                 sizeof(time_text),
                 "%02lu.%03lu",
                 (unsigned long)seconds,
                 (unsigned long)milliseconds);

  OLED_Clear();
  OLED_DrawText(0U, 0U, top_line);

  switch (snapshot->state)
  {
    case COMP_TIMER_INITIALIZING:
      OLED_DrawText(0U, 11U, "CALIBRATING...");
      break;
    case COMP_TIMER_RUNNING:
      OLED_DrawText(0U, 11U, "RUNNING");
      break;
    case COMP_TIMER_FINISHED:
      OLED_DrawText(0U,
                    11U,
                    snapshot->within_limit ? "DONE PASS" : "DONE OVER");
      break;
    case COMP_TIMER_UNAVAILABLE:
      OLED_DrawText(0U, 11U, "NOT IMPLEMENTED");
      break;
    case COMP_TIMER_FAULT:
      OLED_DrawText(0U, 11U, "FAULT - CHECK CAR");
      break;
    case COMP_TIMER_READY:
    default:
      OLED_DrawText(0U, 11U, "SELECT THEN K3");
      break;
  }

  OLED_DrawTextScaled(22U, 24U, time_text, 2U);

  if (snapshot->state == COMP_TIMER_INITIALIZING)
  {
    OLED_DrawText(0U, 51U, "WAIT ABOUT 1-2 SEC");
  }
  else if (snapshot->state == COMP_TIMER_READY)
  {
    OLED_DrawText(0U, 44U, "K1:<  K2:>");
    OLED_DrawText(0U, 55U, "K3:CONFIRM START");
  }
  else if (snapshot->state == COMP_TIMER_RUNNING)
  {
    OLED_DrawText(0U, 51U, "KEYS LOCKED");
  }
  else if (snapshot->state == COMP_TIMER_FINISHED)
  {
    OLED_DrawText(0U, 51U, "K1/K2 SEL K3 RUN");
  }
  else if (snapshot->state == COMP_TIMER_UNAVAILABLE)
  {
    OLED_DrawText(0U, 51U, "K1/K2 SELECT");
  }
  else
  {
    OLED_DrawText(0U, 51U, "PRESS OTHER KEY");
  }
}
