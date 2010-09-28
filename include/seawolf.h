/**
 * \file
 * \brief Top level libseawolf include
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

/* Make some standard includes available */
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Include all Seawolf development headers */
#include "seawolf/ardcomm.h"
#include "seawolf/comm.h"
#include "seawolf/config.h"
#include "seawolf/dictionary.h"
#include "seawolf/list.h"
#include "seawolf/logging.h"
#include "seawolf/notify.h"
#include "seawolf/pid.h"
#include "seawolf/queue.h"
#include "seawolf/serial.h"
#include "seawolf/stack.h"
#include "seawolf/task.h"
#include "seawolf/timer.h"
#include "seawolf/util.h"
#include "seawolf/var.h"

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
