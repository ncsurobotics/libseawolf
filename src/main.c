/**
 * \file
 * \brief Core functions
 */

#include "seawolf.h"

#include <signal.h>

/** Wrapper for a function to be called at exit. ISO C forbids functions to be
    cast to void* so this little trick help us get around that limitation */
typedef struct {
    void (*func)(void);
} Seawolf_AtExitWrapper;

/** Registered application name */
static char app_name[SEAWOLF_MAX_NAME_LEN];

/** Library closed/closing */
static bool closed = false;

/** Path to the configuration file */
static char* seawolf_config_file = NULL;

/** Queue of functions to call on exit */
static Queue* at_exit = NULL;

static void Seawolf_catchSignal(int sig);
static void Seawolf_processConfig(void);

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
 * when making any calls before this is called.
 *
 * One of the first tasks of Seawolf_init() is to read a configuration file. If
 * no configuration file has been specified by a call to Seawolf_loadConfig()
 * then the default of /etc/seawolf.conf is used. The environment variable
 * SW_CONFIG can also be used to specify a configuration file, and if it is set,
 * take precedence over a file specified by Seawolf_loadConfig(). This allows
 * for alternate configuration files to be easily specified during testing.
 *
 * \param name Name of the program. This is used in debugging and logging
 */
void Seawolf_init(const char* name) {
    /* Copy name */
    strncpy(app_name, name, SEAWOLF_MAX_NAME_LEN);
    app_name[SEAWOLF_MAX_NAME_LEN - 1] = '\0';

    /* Catch siginals and insure proper shutdown */
    signal(SIGINT, Seawolf_catchSignal);
    signal(SIGHUP, Seawolf_catchSignal);
    signal(SIGTERM, Seawolf_catchSignal);
    signal(SIGPIPE, SIG_IGN);

    /* Choose configuration file */
    if(getenv("SW_CONFIG")) {
        Logging_log(NORMAL, "Using configuration file specified in SW_CONFIG environment variable");
        Seawolf_loadConfig(getenv("SW_CONFIG"));
    } else if(seawolf_config_file == NULL) {
        Logging_log(NORMAL, Util_format("Falling back to default config file at %s", SEAWOLF_DEFAULT_CONFIG));
        Seawolf_loadConfig(SEAWOLF_DEFAULT_CONFIG);
    }

    /* Process the configuration file */
    Seawolf_processConfig();

    /* Ensure shutdown during exit */
    atexit(Seawolf_close);

    Notify_init(); /* Initialize Notify before Comm since Comm may otherwise
                      forward notification messages to it before it's ready for
                      them */
    Comm_init();
    Var_init();
    Logging_init();
    Serial_init();

    /* Log message announcing application launch */
    Logging_log(INFO, "Initialized");
}

/**
 * \brief Specify a configuration file
 *
 * Load the options in the given configuration file when Seawolf_init() is called
 *
 * The valid configuration options are,
 *  - comm_server - This option specifies the IP address of hub server (default is 127.0.0.1)
 *  - comm_port - The port of the hub server (default is 31427)
 *  - comm_password - The password to authenticate with the hub server using (default is empty)
 *  - log_level - The lowest priority of log messages to log. Should be one of DEBUG, INFO, NORMAL, WARNING, ERROR, or CRITICAL (default is NORMAL)
 *  - log_replicate_stdout - Replicate log messages to standard output (default is true)
 *
 * \param filename File to load configuration from
 */
void Seawolf_loadConfig(const char* filename) {
    if(seawolf_config_file) {
        free(seawolf_config_file);
    }

    seawolf_config_file = strdup(filename);
}

/**
 * \brief Process the configuration file specified by Seawolf_loadConfig()
 *
 * Seawolf_init() calls this function to process the configuration file set by
 * Seawolf_loadConfig() or the default configuration file if one is not
 * specified.
 */
static void Seawolf_processConfig(void) {
    Dictionary* config;
    List* options;
    char* option;
    char* value;
    short level;

    config = Config_readFile(seawolf_config_file);

    if(config == NULL) {
        switch(Config_getError()) {
        case CONFIG_EFILEACCESS:
            Logging_log(CRITICAL, Util_format("Failed to open configuration file: %s", strerror(errno)));
            break;
        case CONFIG_ELINETOOLONG:
            Logging_log(CRITICAL, Util_format("Line exceeded maximum allowable length at line %d", Config_getLineNumber()));
            break;
        case CONFIG_EPARSE:
            Logging_log(CRITICAL, Util_format("Parse error occured on line %d", Config_getLineNumber()));
            break;
        default:
            Logging_log(CRITICAL, "Unknown error occured while reading configuration file");
            break;
        }
        Seawolf_exitError();

        /* Ensure we exit if Seawolf_exitError falls through */
        exit(EXIT_FAILURE);
    }

    /* Get the list of configuration options */
    options = Dictionary_getKeys(config);

    while(List_getSize(options)) {
        /* Remove the first item from the list */
        option = List_remove(options, 0);
        value = Dictionary_get(config, option);

        /* Check against configuration option */
        if(strcmp(option, "comm_password") == 0) {
            Comm_setPassword(value);
        } else if(strcmp(option, "comm_server") == 0) {
            Comm_setServer(value);
        } else if(strcmp(option, "comm_port") == 0) {
            Comm_setPort(atoi(value));
        } else if(strcmp(option, "log_level") == 0) {
            level = Logging_getLevelFromName(value);
            if(level == -1) {
                Logging_log(ERROR, Util_format("Invalid logging level '%s'", value));
            } else {
                Logging_setThreshold(level);
            }
        } else if(strcmp(option, "log_replicate_stdout") == 0) {
            Logging_replicateStdio(Config_truth(value));
        } else {
            Logging_log(WARNING, Util_format("Unknown configuration option '%s'", option));
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
 * \brief Register a function to be called on exit
 *
 * Register the given function to be called when the library is shutting
 * down. Multiple functions can be registered and will be called in FIFO order.
 *
 * \param func The function to call. Should take no arguments and return void
 */
void Seawolf_atExit(void (*func)(void)) {
    Seawolf_AtExitWrapper* wrapper;

    /* It is likely that atExit may be called before Seawolf_init so we'll
       initialize the queue here ourselves */
    if(at_exit == NULL) {
        at_exit = Queue_new();
    }

    wrapper = malloc(sizeof(Seawolf_AtExitWrapper));
    wrapper->func = func;

    Queue_append(at_exit, wrapper);
}

/**
 * \brief Close the library
 *
 * Close the library and free any resources claimed by it
 */
void Seawolf_close(void) {
    Seawolf_AtExitWrapper* wrapper;

    /* Only close once */
    if(closed) {
        return;
    }

    closed = true;

    /* Run atExit Queue */
    if(at_exit) {
        while(Queue_getSize(at_exit)) {
            wrapper = Queue_pop(at_exit, false);
            wrapper->func();
            free(wrapper);
        }
        Queue_destroy(at_exit);
    }

    if(seawolf_config_file) {
        free(seawolf_config_file);
    }

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
 * Return the name passed to Seawolf_init()
 *
 * \return The registered application name
 */
char* Seawolf_getName(void) {
    return app_name;
}

/** \} */
