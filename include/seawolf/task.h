/**
 * \file
 */

#ifndef __SEAWOLF_TASK_INCLUDE_H
#define __SEAWOLF_TASK_INCLUDE_H

#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>

/**
 * Task handle used to refer to background tasks
 * \private
 */
typedef pthread_t Task_Handle;

int Task_watchdog(int (*func)(void), double timeout, int* retval);
Task_Handle Task_background(int (*func)(void));
pid_t Task_spawnApplication(char* path, char* args, ...);
void Task_kill(Task_Handle task);
void Task_wait(Task_Handle task);

#endif // #ifndef __SEAWOLF_TASK_INCLUDE_H
