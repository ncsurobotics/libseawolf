/**
 * \file
 * \brief Top level libseawolf include
 */


/**
 * \defgroup Core Core Routines
 * \defgroup Communications Communications
 * \defgroup DataStructures Data Structures
 * \defgroup Hardware Hardware Access
 * \defgroup Utilities Utilities
 */

#ifndef __SEAWOLF_ROOT_INCLUDE_H
#define __SEAWOLF_ROOT_INCLUDE_H

/**
 * Make available POSIX functions
 */
#define _POSIX_C_SOURCE 200112L

/**
 * Make more functions available
 */
#define _XOPEN_SOURCE 600

/* Make sure some standard includes are available */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>

/* Include all Seawolf development headers */
#include "seawolf/comm.h"
#include "seawolf/logging.h"
#include "seawolf/var.h"
#include "seawolf/notify.h"

#include "seawolf/serial.h"
#include "seawolf/ardcomm.h"

#include "seawolf/config.h"
#include "seawolf/util.h"
#include "seawolf/timer.h"
#include "seawolf/task.h"
#include "seawolf/pid.h"

#include "seawolf/stack.h"
#include "seawolf/list.h"
#include "seawolf/queue.h"
#include "seawolf/dictionary.h"

/**
 * \addtogroup Main
 * \{
 */

/** Default location of the configuration file */
#define SEAWOLF_DEFAULT_CONFIG "/etc/seawolf.conf"

/** Maximum length of an application name passed to Seawolf_init() */
#define SEAWOLF_MAX_NAME_LEN 256

/** \} */

void Seawolf_init(const char* name);
void Seawolf_close(void);
void Seawolf_exitError(void);
void Seawolf_exit(void);
bool Seawolf_closing(void);
char* Seawolf_getName(void);
void Seawolf_loadConfig(const char* filename);
void Seawolf_atExit(void (*func)(void));

#endif // #ifndef __SEAWOLF_ROOT_INCLUDE_H
