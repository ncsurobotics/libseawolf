
#include "seawolf.h"
#include "seawolf_hub.h"

static Dictionary* var_cache = NULL;
static List* persistent_variables = NULL;

static Task_Handle db_flush_handle;
static pthread_cond_t do_flush = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t flush_lock = PTHREAD_MUTEX_INITIALIZER;
static int do_flush_flag = 0;

static int Hub_Var_dbFlusher(void) {
    FILE* tmp_db_file = NULL;
    char* db = Hub_Config_getOption("var_db");
    char* tmp_db = Util_format("%s.0", db);
    int persistent_variable_count = List_getSize(persistent_variables);
    char* var_name;
    Hub_Var* var;

    pthread_mutex_lock(&flush_lock);
    while(true) {
        while(do_flush_flag == 0) {
            pthread_cond_wait(&do_flush, &flush_lock);
        }
        do_flush_flag = 0;
        
        tmp_db_file = fopen(tmp_db, "w");
        if(tmp_db_file == NULL) {
            Hub_Logging_log(ERROR, "Unable to flush database!");
        }
        
        fprintf(tmp_db_file, "# %-18s = %s\n", "VARIABLE", "VALUE");
        for(int i = 0; i < persistent_variable_count; i++) {
            var_name = List_get(persistent_variables, i);
            var = Dictionary_get(var_cache, var_name);
            fprintf(tmp_db_file, "%-20s = %.4f\n", var_name, var->value);
        }
        
        fclose(tmp_db_file);
        rename(tmp_db, db);
    }

    return 0;
}

static void Hub_Var_flushPersistent(void) {
    do_flush_flag = 1;
    pthread_cond_signal(&do_flush);
}

static void Hub_Var_readPersistentValues(void) {
    char* var_db = Hub_Config_getOption("var_db");
    Dictionary* db;
    Hub_Var* var;
    List* var_names;
    char* var_name;
    char* var_value;
    float value;
    int retval;
    FILE* db_file;

    /* Database not given */
    if(var_db == NULL) {
        Hub_Logging_log(ERROR, "No variable database specified in configuration!");
        Hub_exitError();
    }

    /* Database doesn't exist but we'll try to create it */
    if(!Hub_fileExists(var_db)) {
        db_file = fopen(var_db, "w");
        if(db_file == NULL) {
            Hub_Logging_log(ERROR, Util_format("Unable to create variable database: %s", strerror(errno))) ;
            Hub_exitError();
        }
        fclose(db_file);
    }

    /* Read file */
    db = Config_readFile(var_db);

    /* Check for errors while opening the file */
    if(db == NULL) {
        switch(Config_getError()) {
        case CONFIG_EFILEACCESS:
            Hub_Logging_log(WARNING, Util_format("Could not create/open variable database: %s", strerror(errno)));
            break;
        case CONFIG_ELINETOOLONG:
            Hub_Logging_log(CRITICAL, Util_format("Line exceeded maximum allowable length at line %d in %s", Config_getLineNumber(), var_db));
            break;
        case CONFIG_EPARSE:
            Hub_Logging_log(CRITICAL, Util_format("Parse error occured on line %d in %s", Config_getLineNumber(), var_db));
            break;
        default:
            Hub_Logging_log(CRITICAL, "Unknown error occured while reading variable database");
            break;
        }

        /* Bail out and exit */
        Hub_exitError();
    }

    var_names = Dictionary_getKeys(db);
    while(List_getSize(var_names)) {
        var_name = List_remove(var_names, 0);
        var_value = Dictionary_get(db, var_name);

        retval = sscanf(var_value, "%f", &value);
        if(retval != 1) {
            Hub_Logging_log(ERROR, Util_format("Format error in variable database for variable '%s'", var_name));
            Hub_exitError();
        }

        var = Dictionary_get(var_cache, var_name);
        if(var == NULL) {
            Hub_Logging_log(ERROR, Util_format("Variable '%s' found in database but no present in variable definitions!", var_name));
            Hub_exitError();
        }

        if(!var->persistent) {
            Hub_Logging_log(WARNING, Util_format("Loading value for non-persistent variable '%s' from database", var_name));
        }

        /* Store value from file */
        var->value = value;
    }
}

static void Hub_Var_readDefinitions(void) {
    char* var_defs = Hub_Config_getOption("var_defs");
    Dictionary* defs;
    Hub_Var* new_var;
    List* var_names;
    char* var_name;
    char* var_def;
    float default_value;
    int persistent, readonly;
    int retval;
    
    if(var_defs == NULL || !Hub_fileExists(var_defs)) {
        Hub_Logging_log(ERROR, "Could not open variable definitions file. Is it specified in the configuration file?");
        Hub_exitError();
    }

    /* Read file */
    defs = Config_readFile(var_defs);

    /* Check for errors while opening the file */
    if(defs == NULL) {
        switch(Config_getError()) {
        case CONFIG_EFILEACCESS:
            Hub_Logging_log(WARNING, Util_format("Could not open variable definitions file: %s", strerror(errno)));
            break;
        case CONFIG_ELINETOOLONG:
            Hub_Logging_log(CRITICAL, Util_format("Line exceeded maximum allowable length at line %d in %s", Config_getLineNumber(), var_defs));
            break;
        case CONFIG_EPARSE:
            Hub_Logging_log(CRITICAL, Util_format("Parse error occured on line %d in %s", Config_getLineNumber(), var_defs));
            break;
        default:
            Hub_Logging_log(CRITICAL, "Unknown error occured while reading variable definitions");
            break;
        }

        /* Bail out and exit */
        Hub_exitError();
    }

    /* Populate variable cache */
    var_cache = Dictionary_new();
    persistent_variables = List_new();

    var_names = Dictionary_getKeys(defs);
    while(List_getSize(var_names)) {
        var_name = List_remove(var_names, 0);
        var_def = Dictionary_get(defs, var_name);

        retval = sscanf(var_def, "%f , %d , %d", &default_value, &persistent, &readonly);
        if(retval != 3) {
            Hub_Logging_log(ERROR, Util_format("Format error in variable definition for variable '%s'", var_name));
            Hub_exitError();
        }

        if(!(persistent == 0 || persistent == 1)) {
            Hub_Logging_log(ERROR, Util_format("Value for persistent in variable definition for '%s' should be 0 or 1", var_name));
            Hub_exitError();
        }

        if(!(readonly == 0 || readonly == 1)) {
            Hub_Logging_log(ERROR, Util_format("Value for persistent in variable definition for '%s' should be 0 or 1", var_name));
            Hub_exitError();
        }

        /* Good variable */
        new_var = malloc(sizeof(Hub_Var));
        new_var->name = strdup(var_name);
        new_var->default_value = default_value;
        new_var->value = default_value;
        new_var->persistent = persistent;
        new_var->readonly = readonly;

        /* Save variable to cache */
        Dictionary_set(var_cache, var_name, new_var);

        /* If this variable is persistent put it into the list of persistent
           variables */
        if(persistent) {
            List_append(persistent_variables, new_var->name);
        }
    }
}

void Hub_Var_init(void) {
    var_cache = Dictionary_new();
    Hub_Var_readDefinitions();

    if(List_getSize(persistent_variables)) {
        Hub_Var_readPersistentValues();
        db_flush_handle = Task_background(Hub_Var_dbFlusher);
    }
}

Hub_Var* Hub_Var_get(const char* name) {
    Hub_Var* var = Dictionary_get(var_cache, name);
    return var;
}

int Hub_Var_set(const char* name, double value) {
    Hub_Var* var = Dictionary_get(var_cache, name);
    if(var == NULL) {
        return -1;
    }

    if(var->readonly) {
        return -2;
    }

    var->value = value;
    if(var->persistent) {
        Hub_Var_flushPersistent();
    }

    return 0;
}

void Hub_Var_close(void) {
    List* keys = Dictionary_getKeys(var_cache);
    int count = List_getSize(keys);
    char* key;
    Hub_Var* var;

    for(int i = 0; i < count; i++) {
        key = List_get(keys, i);
        var = Dictionary_get(var_cache, key);
        free(var->name);
        free(var);
    }

    List_destroy(keys);
    Dictionary_destroy(var_cache);
}
