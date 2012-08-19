/**
 * \file
 * \brief Timer
 */

#include "seawolf.h"

static double get_monotonic_seconds(void);

#ifdef __SW_Darwin__

# include <mach/mach_time.h>

static mach_timebase_info_data_t timebase;

static double get_monotonic_seconds(void) {
    uint64_t now = mach_absolute_time();
    double nanoseconds = ((double) now * timebase.numer) / timebase.denom;
    return nanoseconds * 1e-9;
}

#else

# include <time.h>

static double get_monotonic_seconds(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return ((double) now.tv_sec) + (((double) now.tv_nsec) * 1e-9);
}

#endif

/**
 * \defgroup Timer Timer
 * \ingroup Utilities
 * \brief Timers for calculating total time and time delays
 * \{
 */

void Timer_init(void) {
#ifdef __SW_Darwin__
    mach_timebase_info(&timebase);
#endif
}

/**
 * \brief Return a new Timer object
 *
 * Return a new timer object
 *
 * \return A new timer
 */
Timer* Timer_new(void) {
    Timer* tm = malloc(sizeof(Timer));
    if(tm == NULL) {
        return NULL;
    }

    /* Rest the timer */
    Timer_reset(tm);

    return tm;
}

/**
 * \brief Get a time delta
 *
 * Return the time delta in seconds since the last call to Timer_getDelta() or
 * since the Timer was created
 *
 * \param tm The timer to get the delta for
 * \return Seconds since the timer being created or the last call to
 * Timer_getDelta()
 */
double Timer_getDelta(Timer* tm) {
    double now = get_monotonic_seconds();
    double diff = now - tm->last;
    tm->last = now;

    return diff;
}

/**
 * \brief Get total time delay
 *
 * Get the time delta in seconds since the Timer was created or last reset
 *
 * \param tm The timer to get the delay for
 * \return Seconds since timer reset or timer creation
 */
double Timer_getTotal(Timer* tm) {
    return get_monotonic_seconds() - tm->base;
}

/**
 * \brief Reset the timer
 *
 * Reset the timer's base time
 *
 * \param tm The timer to reset
 */
void Timer_reset(Timer* tm) {
    /* Store the time into base and copy to last */
    tm->last = tm->base = get_monotonic_seconds();
}

/**
 * \brief Destroy the timer
 *
 * Free the memory associated with the timer
 *
 * \param tm The timer to free
 */
void Timer_destroy(Timer* tm) {
    free(tm);
}

/** \} */
