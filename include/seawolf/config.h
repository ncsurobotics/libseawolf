/**
 * \file
 */

#ifndef __SEAWOLF_CONFIG_INCLUDE_H
#define __SEAWOLF_CONFIG_INCLUDE_H

#include "seawolf/dictionary.h"

/**
 * \addtogroup Config
 * \{
 */

/**
 * Maximum length of a line in the configuration file
 */
#define MAX_LINE 512

/**
 * Config file read successfully
 */
#define CONFIG_SUCCESS 0

/**
 * The length of a line exceeded MAX_LINE
 */
#define CONFIG_ELINETOOLONG -1

/**
 * An error occured while opening the file
 */
#define CONFIG_EFILEACCESS -2

/**
 * A parse error occured. See Config_getLineNumber() to determine the erroneous
 * line
 */
#define CONFIG_EPARSE -3

/** \} */

Dictionary* Config_readFile(const char* filename);
int Config_getError(void);
int Config_getLineNumber(void);

#endif // #ifndef __SEAWOLF_CONFIG_INCLUDE_H
