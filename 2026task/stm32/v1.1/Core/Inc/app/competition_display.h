#ifndef COMPETITION_DISPLAY_H
#define COMPETITION_DISPLAY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "competition_timer.h"

void CompetitionDisplay_Render(const CompetitionTimerSnapshot *snapshot);

#ifdef __cplusplus
}
#endif

#endif /* COMPETITION_DISPLAY_H */
