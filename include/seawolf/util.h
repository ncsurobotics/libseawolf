/**
 * \file
 */

#ifndef __SEAWOLF_UTIL_INCLUDE_H
#define __SEAWOLF_UTIL_INCLUDE_H

/**
 * \addtogroup Util
 * \{
 */

/**
 * Return the max of a and b
 *
 * \param a first value
 * \param b second value
 * \return The max of a and b
 */
#define Util_max(a, b) (((a) > (b)) ? (a) : (b))

/**
 * Return the min of a and b
 *
 * \param a first value
 * \param b second value
 * \return The min of a and b
 */
#define Util_min(a, b) (((a) < (b)) ? (a) : (b))

/**
 * Bound a value between a maximum and minimum
 *
 * \param a Lower bound
 * \param x The value to bound
 * \param b Upper bound
 * \return x if a <= x <= b, a if x < a, and b if x > b
 */
#define Util_inRange(a, x, b) (((x) < (a)) ? (a) : (((x) > (b)) ? (b) : (x)))

/** \} */

void Util_usleep(double s);

char* Util_format(char* format, ...);
char* __Util_format(char* format, ...);

void Util_strip(char* buffer);
int Util_split(const char* buffer, char split, char* p1, char* p2);

void Util_close(void);

#endif // #ifndef __SEAWOLF_UTIL_INCLUDE_H
