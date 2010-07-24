/**
 * \file
 * \brief Configuration file loading
 */

#include "seawolf.h"

#include <ctype.h>

/**
 * \defgroup Config Configuration
 * \ingroup Utilities
 * \brief Support for reading configuration files
 *
 * This module provides generic support for reading configuration files. It is
 * used internally by libseawolf and the hub to read their own respective
 * configuration files. The format of a configuration file is quite simple:
 * whitespace is ignored except within configuration option values, comments
 * begin with a '#' and extend to the end of the line. Lines specifying
 * configuration values take the form,
 *
 * &lt;option&gt; = &lt;value&gt;
 *
 * \{
 */

/** Hold the error code in the event that Config_readFile errors */
static int config_errno = 0;

/** Holds the current line number while parsing and can be used in the event of
    a parsing error to point the user to the problematic line */
static int config_lineno = 0;

/**
 * \brief Read a configuration file into a Dictionaryx
 *
 * Parse the configuration file as described above and store all the key/value
 * pairs into a Dictionary which is returned.
 *
 * \param filename The file to read
 * \return A Dictionary mapping the key/value pairs or NULL if an error occurs
 */
Dictionary* Config_readFile(const char* filename) {
    FILE* config_file = fopen(filename, "r");
    Dictionary* config = NULL;
    char* option;
    char* value;
    int i, tmp = 0;
    char line[MAX_LINE];
    bool comment;

    /* Reset config_errno to success */
    config_errno = CONFIG_SUCCESS;
    config_lineno = 0;

    /* Configuration file opened alright? */
    if(config_file == NULL) {
        config_errno = CONFIG_EFILEACCESS;
        goto config_error;
    }

    /* Create the dictionary object */
    config = Dictionary_new();

    /* Read in line by line until EOF to get each configuration option */
    while(true) {
        line[0] = '\0';

        /* Read until we find a non empty line */
        while(strcmp(line, "") == 0) {
            i = 0;
            comment = false;

            /* Read line */
            while(true) {
                tmp = fgetc(config_file);

                /* End of input/line */
                if(tmp == EOF || tmp == '\n') {
                    break;
                }

                /* Start of a comment */
                if(tmp == '#') {
                    comment = true;
                }

                /* If we're in a comment discard characters */
                if(!comment) {
                    line[i++] = tmp;
                }

                /* Ran out of space. Line too long */
                if(i >= MAX_LINE) {
                    config_errno = CONFIG_ELINETOOLONG;
                    goto config_error;
                }
            }
            config_lineno++;
            line[i] = '\0';

            /* Strip white space from entire line */
            Util_strip(line);

            /* If the line is empty and we have reached the end of file then
               return */
            if(tmp == EOF && strcmp(line, "") == 0) {
                fclose(config_file);
                return config;
            }
        }

        /* Split the line */
        option = value = line;
        for(i = 0; line[i] != '\0'; i++) {
            if(line[i] == '=') {
                line[i] = '\0';
                value = line + i + 1;
                break;
            }
        }
        
        /* Never found '=' in the line */
        if(option == value) {
            config_errno = CONFIG_EPARSE;
            goto config_error;
        }
        
        /* Strip spaces from components */
        Util_strip(option);
        Util_strip(value);
        
        /* Store the pair */
        Dictionary_set(config, option, strdup(value));
    }

 config_error:
    if(config != NULL) {
        List* options = Dictionary_getKeys(config);
        char* option;

        /* Free memory allocated to config values */
        while(List_getSize(options)) {
            option = List_remove(options, 0);
            free(Dictionary_get(config, option));
        }

        List_destroy(options);
        Dictionary_destroy(config);
    }
    
    if(config_file != NULL) {
        fclose(config_file);
    }

    return NULL;
}

/**
 * \brief Get the error code of the last run of Config_readFile()
 *
 * This returns the exit status of the last call to Config_readFile(). This can
 * be used to determine in what way the call failed.
 *
 * \return The exit status of the last call to Config_readFile()
 */
int Config_getError(void) {
    return config_errno;
}

/**
 * \brief Get the line number
 * 
 * Get the line number at the end of the last call to Config_readFile(). This can
 * be used to provide useful feedback in the event of a parsing error.
 *
 * \return The ending line number of the last call to Config_readFile()
 */
int Config_getLineNumber(void) {
    return config_lineno;
}

/**
 * \brief Test the truth value of a string
 *
 * Perform a case insensitive test of the given string to determine whether it
 * corresponds to a true or false configuration value. Values considered true
 * are '1', 'true', 'yes', and 'on'. All other inputs are considered false.
 *
 * \param value The string to check
 * \return The truth value of the string
 */
bool Config_truth(const char* value) {
    char* value_copy = strdup(value);
    bool retval;
    
    /* Convert to lower case */
    for(int i = 0; value_copy[i] != '\0'; i++) {
        value_copy[i] = tolower(value_copy[i]);
    }
    
    /* Check for common values */
    retval = ((strcmp(value_copy, "1") == 0) ||
              (strcmp(value_copy, "true") == 0) ||
              (strcmp(value_copy, "yes") == 0) ||
              (strcmp(value_copy, "on") == 0));
    
    free(value_copy);
    return retval;
}

/** \} */
