/**
 * \file
 */

#ifndef __SEAWOLF_TIMER_INCLUDE_H
#define __SEAWOLF_TIMER_INCLUDE_H

#include <time.h>

/**
 * Timer
 * \private
 */
typedef struct {
    /**
     * Starting time
     * \private
     */
    double base;

    /**
     * Time at last delta
     * \private
     */
    double last;
} Timer;

void Timer_init(void);
Timer* Timer_new(void);
double Timer_getDelta(Timer* tm);
double Timer_getTotal(Timer* tm);
void Timer_reset(Timer* tm);
void Timer_destroy(Timer* tm);

#endif // #ifndef __SEAWOLF_TIMER_INCLUDE_H
