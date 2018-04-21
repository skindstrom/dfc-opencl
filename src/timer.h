#ifndef DFC_TIMING_H
#define DFC_TIMING_H

#include <stdint.h>

#define TIMER_PREPROCESSING 1
#define TIMER_EXECUTION 2
#define TIMER_WRITE_TO_DEVICE 3
#define TIMER_READ_FROM_DEVICE 4

#ifdef __cplusplus
extern "C" {
#endif

void startTimer(int timer);
void stopTimer(int timer);
void resetTimer(int timer);

double readTimerMs(int timer);

#ifdef __cplusplus
}
#endif


#endif