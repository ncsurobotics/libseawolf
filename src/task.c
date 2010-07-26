/**
 * \file
 * \brief Task scheduling and backgrounding
 */

#include "seawolf.h"

#include <stdarg.h>
#include <pthread.h>
#include <unistd.h>

/**
 * Arguments passed to Task_callWrapper()
 * \private
 */
struct WrapperArgs {
    /**
     * The function to be called
     * \private
     */
    int (*func)(void);
    
    /**
     * Return value of func() stored here
     * \private
     */
    int return_value;
    
    /**
     * Should Task_callWrapper free this structure
     * \private
     */
    bool free;
};

/**
 * Arguments passed to Task_watcher()
 * \private
 */
struct WatcherArgs {
    /**
     * Thread to watch
     * \private
     */
    pthread_t* dependant;

    /**
     * How long to wait
     * \private
     */
    double timeout;
};

static void* Task_callWrapper(void* _args);
static void* Task_watcher(void* _args);

/**
 * \defgroup Task Task scheduling and management
 * \ingroup Utilities
 * \brief Utilities for scheduling tasks and performing simple multitasking
 * \{
 */

/**
 * Maximum number of arguments to the Task_spawnApplication
 */
#define MAX_ARGS 31

/**
 * \brief Call wrapper for task calls
 * \private
 *
 * Given a WrapperArgs, call WrapperArgs->func, store the return value in
 * WrapperArgs->return_value and possible free the WrapperArgs
 *
 * \param _args A WrapperArgs pointer cast to void*
 * \return NULL
 */
static void* Task_callWrapper(void* _args) {
    struct WrapperArgs* args = (struct WrapperArgs*) _args;
    struct WrapperArgs args_copy;

    if(args->free) {
        args_copy = (*args);
        free(args);
        args_copy.func();
    } else {
        args->return_value = args->func();
    }

    return NULL;
}

/**
 * \brief Call wrapper for watchdog
 * \private
 *
 * _args is cast to a WatcherArgs pointer (args). This function sleeps, and if
 * it is not canceled before returning from the sleep call, cancels the thread
 * given by args->dependant
 *
 * \param _args A WatcherArgs pointer cast to void*
 * \return NULL
 */
static void* Task_watcher(void* _args) {
    struct WatcherArgs* args = (struct WatcherArgs*)_args;
    Util_usleep(args->timeout);
    pthread_cancel(*args->dependant);
    return NULL;
}

/**
 * \brief Run a function with a timeout
 *
 * Watchdog on a function call. Returns WATCHDOG_TIMEOUT on timeout
 *
 * \param timeout Number of seconds before returning WATCHDOG_TIMEOUT if the
 * function does not return sooner
 * \param func Function to call under the watchdog.
 * \return Returns the return value of the function or WATCHDOG_TIMEOUT in the
 * case of timeout
 */
int Task_watchdog(double timeout, int (*func)(void)) {
    pthread_t func_th;
    pthread_t watchdog_th;
    struct WrapperArgs func_args = {func, WATCHDOG_TIMEOUT, false};
    struct WatcherArgs watchdog_args = {&func_th, timeout};

    pthread_create(&func_th, NULL, Task_callWrapper, &func_args);
    pthread_create(&watchdog_th, NULL, Task_watcher, &watchdog_args);

    /* Wait for main function - may be canceled by watcher */
    pthread_join(func_th, NULL);
    
    /* Cancel and wait for watcher */
    pthread_cancel(watchdog_th);
    pthread_join(watchdog_th, NULL);

    return func_args.return_value;
}

/**
 * \brief Spawn a function in a new thread
 *
 * Run a new thread to run the given function in and return without waiting for
 * completion
 * 
 * \param func The function to spawn
 * \return A Task handle object
 */
Task_Handle Task_background(int (*func)(void)) {
    pthread_t func_th;
    struct WrapperArgs* func_args = malloc(sizeof(struct WrapperArgs));

    func_args->func = func;
    func_args->return_value = 0;
    func_args->free = true;

    pthread_create(&func_th, NULL, Task_callWrapper, func_args);
    return func_th;
}

/**
 * \brief Spawn an external program
 *
 * Spawn an external application and run asychronously with the current
 * application i.e. it returns almost immediately
 *
 * \param path Application to be run
 * \param args A NULL terminated list of program arguments
 * \return -1 on failure, otherwise the PID of spawned application is returned
 */
pid_t Task_spawnApplication(char* path, char* args, ...) {
    int pid;
    va_list ap;
    char* argv[MAX_ARGS + 1];

    /* Build arguments array */
    argv[0] = path;
    argv[1] = NULL;

    va_start(ap, args);
    for(int i = 1; i < MAX_ARGS && args != NULL; i++) {
        argv[i] = args;
        argv[i+1] = NULL;
        args = va_arg(ap, char*);
    }
    va_end(ap);

    /* Run program */
    pid = fork();
    if(pid == 0) {
        /* Replace current process with the given application */
        execv(path, argv);
        
        /* Should *not* happen */
        fprintf(stderr, "Application %s failed to spawn!\n", path);
        exit(EXIT_FAILURE);
    }

    return pid;
}

/**
 * \brief Kill a running task
 *
 * Kill a task previously spawned by Task_background()
 *
 * \param task The Task handle associated with the task to kill
 */
void Task_kill(Task_Handle task) {
    pthread_cancel(task);
    pthread_join(task, NULL);
}

/**
 * \brief Wait for a task to terminate
 *
 * Wait for a task to terminate
 *
 * \param task The task handle associated with the task to wait for
 */
void Task_wait(Task_Handle task) {
    pthread_join(task, NULL);
}

/** \} */
