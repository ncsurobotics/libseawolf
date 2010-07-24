
#include "seawolf.h"
#include "seawolf_hub.h"

#include <ctype.h>

typedef struct {
    char* option;
    char* default_value;
} Hub_Config_Option;

static bool Hub_Config_chooseConfigFile(void);

static Dictionary* config = NULL;
static char* hub_config_file = NULL;

/* Available options and their defaults */
static Hub_Config_Option valid_options[] = {{"bind_address"        , "127.0.0.1"       },
                                            {"bind_port"           , "31427"           },
                                            {"password"            , ""                },
                                            {"var_db"              , "seawolf_var.db"  },
                                            {"var_defs"            , "seawolf_var.defs"},
                                            {"log_file"            , ""                },
                                            {"log_replicate_stdout", "1"               },
                                            {"log_level"           , "NORMAL"          }};

void Hub_Config_loadConfig(const char* filename) {
    if(hub_config_file) {
        free(hub_config_file);
    }

    hub_config_file = strdup(filename);
}

/* Choose which configuration file we will use. If a configuration file has been
   specified by Hub_Config_loadConfig then we will always use that
   one. Otherwise, we will check fo the existance of ~/.swhubc and
   /etc/seawolf_hub.conf in that order. If neither of those exist we give up */
static bool Hub_Config_chooseConfigFile(void) {
    char* temp_path = NULL;

    if(hub_config_file == NULL) {
        /* Try ~/.swhubrc */
        if(getenv("HOME")) {
            temp_path = Util_format("%s/.swhubrc", getenv("HOME"));
        }

        /* Try /etc/seawolf_hub.conf */
        if(temp_path == NULL || !Hub_fileExists(temp_path)) {
            temp_path = "/etc/seawolf_hub.conf";
        }

        /* Found neither return with error */
        if(temp_path == NULL || !Hub_fileExists(temp_path)) {
            return false;
        }

        /* Use the found file */
        Hub_Config_loadConfig(temp_path);
    }

    return true;
}

void Hub_Config_processConfig(void) {
    Dictionary* temp_config;
    List* options;
    char* option;
    char* value;
    
    /* Initialize config table with default options */
    config = Dictionary_new();
    for(int i = 0; i < sizeof(valid_options) / sizeof(valid_options[0]); i++) {
        Dictionary_set(config, valid_options[i].option, valid_options[i].default_value);
    }

    /* Locate a configuration file */
    if(Hub_Config_chooseConfigFile() == false) {
        Hub_Logging_log(WARNING, "Could not find configuration file! Continuing with default configuration!");
        return;
    }

    /* Attempt to read the configuration file */
    temp_config = Config_readFile(hub_config_file);

    /* Error handling for Config_readFile */
    if(temp_config == NULL) {
        switch(Config_getError()) {
        case CONFIG_EFILEACCESS:
            Hub_Logging_log(WARNING, Util_format("Failed to open configuration file: %s", strerror(errno)));
            break;
        case CONFIG_ELINETOOLONG:
            Hub_Logging_log(CRITICAL, Util_format("Line exceeded maximum allowable length at line %d", Config_getLineNumber()));
            break;
        case CONFIG_EPARSE:
            Hub_Logging_log(CRITICAL, Util_format("Parse error occured on line %d", Config_getLineNumber()));
            break;
        default:
            Hub_Logging_log(CRITICAL, "Unknown error occured while reading configuration file");
            break;
        }

        /* Bail out and exit */
        Hub_exitError();
    }

    /* Get the list of configuration options in the file */
    options = Dictionary_getKeys(temp_config);

    /* Check each configuration option and if it is valid store it in the real
       configuration table */
    while(List_getSize(options)) {
        /* Remove the first item from the list */
        option = List_remove(options, 0);
        value = Dictionary_get(temp_config, option);

        if(Dictionary_exists(config, option)) {
            /* Valid option, store value */
            Dictionary_set(config, option, value);
        } else {
            Logging_log(WARNING, Util_format("Unknown configuration option '%s'", option));
        }
    }

    List_destroy(options);
    Dictionary_destroy(temp_config);
}

char* Hub_Config_getOption(const char* config_key) {
    char* value = Dictionary_get(config, config_key);
    return value;
}
