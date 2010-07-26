/**
 * \file
 * \brief Message logging
 */

#include "seawolf.h"

#include <ctype.h>

/** True if the logging component has be initialized */
static bool initialized = false;

/** Minimum level at which to log messages */
static short min_log_level = NORMAL;

/** Should log messages be duplicated to standard output */
static bool log_stdio = true;

/** String names for log levels */
static char* level_names[] = {"DEBUG",
                              "INFO",
                              "NORMAL",
                              "WARNING",
                              "ERROR",
                              "CRITICAL"};

/**
 * \defgroup Logging Logging
 * \ingroup Communications
 * \brief Provide functions for performing logging for informational and debugging purposes
 * \{
 */

/**
 * \brief Initialize logging 
 * 
 * Initialize the Logging component
 *
 * \private
 */
void Logging_init(void) {
    initialized = true;
}

/**
 * \brief Set the logging threshold
 *
 * Set the threshold for logging to the given level
 *
 * \param level The minimum level to log at
 */
void Logging_setThreshold(short level) {
    min_log_level = level;
}

/**
 * \brief Replicate messages to stdout
 *
 * Specify whether log messages should be replicated to standard output
 *
 * \param do_replicate If true, print log messages to standard output. If false, do not.
 */
void Logging_replicateStdio(bool do_replicate) {
    log_stdio = do_replicate;
}

/**
 * \brief Get the string representation of a log level
 *
 * Get the string representation of a log level
 *
 * \param log_level The log level as given above
 * \return A string representation of the log level
 */
char* Logging_getLevelName(short log_level) {
    return level_names[log_level];
}

/**
 * \brief Get the numerical representation of a log level
 *
 * Get the numerical representation of a log level from the textual
 * representation. This check is case insensitive.
 *
 * \param log_level The log level
 * \return The numerical equivalent or -1 if unknown
 */
short Logging_getLevelFromName(const char* log_level) {
    short level;
    char* level_copy = strdup(log_level);

    /* Convert to all uppercase */
    for(int i = 0; level_copy[i] != '\0'; i++) {
        level_copy[i] = toupper(level_copy[i]);
    }
    
    /* Scan levels */
    for(level = DEBUG; level <= CRITICAL; level++) {
        if(strcmp(Logging_getLevelName(level), level_copy) == 0) {
            free(level_copy);
            return level;
        }
    }
    
    free(level_copy);
    return -1;
}

/**
 * \brief Log a message
 *
 * Log a message. If initialization is complete, the message will be logged
 * through a hub server and may also be printed to standard output depending on
 * the value of the "LogReplicateStdout" variable.
 *
 * \param log_level One of the log levels specified above
 * \param msg The message to log
 */
void Logging_log(short log_level, char* msg) {
    static char* namespace = "LOG";
    char log_level_str[4];

    /* Only log messages with a log level at least as high as min_debug_level */
    if(log_level >= min_log_level) {
        if(initialized) {
            Comm_Message* log_message = Comm_Message_new(4);

            snprintf(log_level_str, 4, "%d", log_level);
            log_message->components[0] = namespace;
            log_message->components[1] = Seawolf_getName();
            log_message->components[2] = log_level_str;
            log_message->components[3] = msg;

            Comm_sendMessage(log_message);
            Comm_Message_destroy(log_message);
        }

        /* Replicate the message to standard output */
        if(log_stdio || !initialized) {
            printf("[%s][%s] %s\n", Seawolf_getName(), level_names[log_level], msg);
        }
    }
}

/**
 * \brief Deinitialize logging
 *
 * Close the logging component
 *
 * \private
 */
void Logging_close(void) {
    initialized = false;
}

/** \} */
