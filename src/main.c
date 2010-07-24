/**
 * \file
 * \brief Core functions
 */

#include "seawolf.h"

#include <signal.h>

/** Registered application name */
static char app_name[256];

/** Library closed/closing */
static bool closed = false;

static void Seawolf_catchSignal(int sig);

/**
 * \defgroup Main Library control
 * \ingroup Core
 * \brief Core routines for libseawolf initialization and management
 * \{
 */

/**
 * \brief Initialize the library
 *
 * Perform all initialization to ready the library for use. Care must be taken
 * when making any calls before this is called
 *
 * \param name Name of the program. This is used in debugging and logging
 */
void Seawolf_init(const char* name) {
    /* Copy name */
    strcpy(app_name, name);


    /* Catch siginals and insure proper shutdown */
    signal(SIGINT, Seawolf_catchSignal);
    signal(SIGHUP, Seawolf_catchSignal);
    signal(SIGTERM, Seawolf_catchSignal);
    signal(SIGPIPE, SIG_IGN);

    /* Ensure shutdown during normal exit */
    atexit(Seawolf_close);

    /* Call all initialization methods. Order here *is* important. Logging
       relies on the database being up for instance */
    Comm_init();
    Var_init();
    Logging_init();
    Notify_init();
    Serial_init();

    /* Log message announcing application launch */
    Logging_log(INFO, "Initialized");
}

/**
 * \brief Load a configuration file
 *
 * Load the options in the given configuration file.
 *
 * The valid configuration options are,
 *  - Comm_server - This option specifies the IP address of hub server (default is 127.0.0.1)
 *  - Comm_port - The port of the hub server (default is 31427)
 *  - Comm_password - The password to authenticate with the hub server using (default is empty)
 *
 * \param filename File to load configuration from
 */
void Seawolf_loadConfig(const char* filename) {
    Dictionary* config;
    List* options;
    char* option;
    char* value;

    config = Config_readFile(filename);

    if(config == NULL) {
        switch(Config_getError()) {
        case CONFIG_EFILEACCESS:
            perror("Failed to open configuration file");
            break;
        case CONFIG_ELINETOOLONG:
            fprintf(stderr, "Line exceeded maximum allowable length at line %d\n", Config_getLineNumber());
            break;
        case CONFIG_EPARSE:
            fprintf(stderr, "Parse error occured on line %d\n", Config_getLineNumber());
            break;
        default:
            fprintf(stderr, "Unknown error occured while reading configuration file\n");
            break;
        }
        Seawolf_exitError();
        exit(EXIT_FAILURE);
    }

    /* Get the list of configuration options */
    options = Dictionary_getKeys(config);

    while(List_getSize(options)) {
        /* Remove the first item from the list */
        option = List_remove(options, 0);
        value = Dictionary_get(config, option);

        /* Check against configuration option */
        if(strcmp(option, "Comm_password") == 0) {
            Comm_setPassword(value);
        } else if(strcmp(option, "Comm_server") == 0) {
            Comm_setServer(value);
        } else if(strcmp(option, "Comm_port") == 0) {
            Comm_setPort(atoi(value));
        } else {
            fprintf(stderr, "Unknown configuration option '%s'\n", option);
        }

        free(value);
    }

    List_destroy(options);
    Dictionary_destroy(config);
}

/**
 * \brief Catch spurious signals
 *
 * Catches signals and shuts down libseawolf properly before exiting
 *
 * \param sig Signal that was caught
 */
static void Seawolf_catchSignal(int sig) {
    /* Caught signal, exit and properly shut down */
    Logging_log(CRITICAL, "Signal caught! Shutting down...");
    Seawolf_exitError();
}

/**
 * \brief Close the library
 *
 * Close the library and free any resources claimed by it
 */
void Seawolf_close(void) {
    /* Only close once */
    if(closed) {
        return;
    }

    closed = true;

    /* Announce closing */
    Logging_log(INFO, "Closing");
    
    Serial_close();
    Logging_close();
    Var_close();
    Comm_close();
    Notify_close();
    Util_close();
}

/**
 * \brief Terminate application due to error
 *
 * Terminate application because of an error condition
 */
void Seawolf_exitError(void) {
    if(closed) {
        return;
    }
    Logging_log(INFO, "Terminating application due to error condition");
    exit(1);
}

/**
 * \brief Terminate application due to normal conditions
 *
 * Terminate application
 */
void Seawolf_exit(void) {
    if(closed) {
        return;
    }

    exit(0);
}

/**
 * \brief True if the library is currently closing
 *
 * True during the closing of the library. This can be used by multithreaded
 * applications to control shutdown procedures
 *
 * \return True if the library is closing, false otherwise
 */
bool Seawolf_closing(void) {
    return closed;
}

/**
 * \brief Get the application name
 *
 * Return the name registered with the library with a call to Seawolf_init()
 *
 * \return The registered application name
 */
char* Seawolf_getName(void) {
    return app_name;
}

/** \} */
