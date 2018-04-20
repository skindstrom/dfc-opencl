#include "timer.h"

#include <sys/time.h>

#define MAX_TIMERS 10

static struct timeval start[MAX_TIMERS];
static struct timeval stop[MAX_TIMERS];
static double elapsedTime[MAX_TIMERS];

void startTimer(int timer) { gettimeofday(&start[timer], 0); }

void stopTimer(int timer) {
  gettimeofday(&stop[timer], 0);
  elapsedTime[timer] +=
      1000 * ((double)(double)(stop[timer].tv_sec - start[timer].tv_sec) +
              1.0e-6 * (stop[timer].tv_usec - start[timer].tv_usec));
}

void resetTimer(int timer) { elapsedTime[timer] = 0; }

double readTimerMs(int timer) { return elapsedTime[timer]; }