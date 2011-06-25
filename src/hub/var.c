/**
 * \file
 * \brief Variable get/set backend
 */

#include "seawolf.h"
#include "seawolf_hub.h"

/** Variable storage */
static Dictionary* var_cache = NULL;

/** List of variables which are marked as persistent */
static List* persistent_variables = NULL;

/**
 * Task handle for the thread which flushes the persistent variable database
 */
static Task_Handle db_flush_handle;

/** Lock associated with flush signal */
static pthread_cond_t do_flush = PTHREAD_COND_INITIALIZER;

/**
 * Hub_Var_dbFlusher waits on this condition. It is signaled by
 * Hub_Var_flushPersistent
 */
static pthread_mutex_t flush_lock = PTHREAD_MUTEX_INITIALIZER;

/**
 * Flag associated with the above condition. Both are needed to ensure proper
 * flush request signaling
 */
static int do_flush_flag = 0;

/**
 * \defgroup var Variable DB
 * \brief Routines for accessing the variable database
 * \{
 */

/**
 * \brief Persistent variable database flushing thread
 *
 * This thread runs in the background of the hub and waits for a signal from
 * Hub_Var_flushPersistent to flush the persistent variables databaes. It does
 * this by writing the database to a temporary file and then moving the
 * temporary file over the real database.
 *
 * \return Does not return. This task is killed in Hub_Var_close
 */
static int Hub_Var_dbFlusher(void) {
    FILE* tmp_db_file = NULL;
    const char* db = Hub_Config_getOption("var_db");
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

/**
 * \brief Flush persistent variables to disk
 *
 * Signal to the Hub_Var_dbFlusher task that it should perform a flush of the
 * persistent variables database
 */
static void Hub_Var_flushPersistent(void) {
    do_flush_flag = 1;
    pthread_cond_signal(&do_flush);
}

/**
 * \brief Read the persistent variables database
 *
 * With var_cache already populated by the variable definitons, this will set
 * the values of persistent variables based on those values stored on disk in
 * the variables database
 */
static void Hub_Var_readPersistentValues(void) {
    const char* var_db = Hub_Config_getOption("var_db");
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
        free(var_value);

        if(retval != 1) {
            Hub_Logging_log(ERROR, Util_format("Format error in variable database for variable '%s'", var_name));
            Hub_exitError();
        }

        var = Dictionary_get(var_cache, var_name);
        if(var == NULL) {
            Hub_Logging_log(ERROR, Util_format("Variable '%s' found in database but not present in variable definitions!", var_name));
            Hub_exitError();
        }

        if(!var->persistent) {
            Hub_Logging_log(WARNING, Util_format("Loading value for non-persistent variable '%s' from database", var_name));
        }

        /* Store value from file */
        var->value = value;
    }

    List_destroy(var_names);
    Dictionary_destroy(db);
}

/**
 * \brief Read the variable definitions file
 *
 * Populate the variable cache (var_cache) by reading variable definitions from
 * the variable definitions file specified in the configuration file (var_defs
 * option)
 */
static void Hub_Var_readDefinitions(void) {
    const char* var_defs = Hub_Config_getOption("var_defs");
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
        free(var_def);

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
        new_var->subscribers = List_new();
        
        pthread_rwlock_init(&new_var->lock, NULL);

        /* Save variable to cache */
        Dictionary_set(var_cache, var_name, new_var);

        /* If this variable is persistent put it into the list of persistent
           variables */
        if(persistent) {
            List_append(persistent_variables, new_var->name);
        }
    }

    List_destroy(var_names);
    Dictionary_destroy(defs);
}

/**
 * \brief Initalize the variables subsystem
 *
 * Read variable definitions, load persistent variable values from the variable
 * database and start a background thread to flush the database
 */
void Hub_Var_init(void) {
    Hub_Var_readDefinitions();

    if(List_getSize(persistent_variables)) {
        Hub_Var_readPersistentValues();
        db_flush_handle = Task_background(Hub_Var_dbFlusher);
    }
}

/**
 * \brief Return a variable structure
 *
 * Return the variable structure for the given variable name    
 *
 * \param name The variable to retreive
 * \return A pointer to the structure representing the variable internally or
 * NULL if the variable could not be found
 */
Hub_Var* Hub_Var_get(const char* name) {
    Hub_Var* var = Dictionary_get(var_cache, name);
    return var;
}

/**
 * \brief Set a variable value
 *
 * Set the value of the specified variable to the given value. If the variable
 * is persistent then the variable database will be flushed.
 *
 * \param name The variable to set
 * \param value New value for the variable
 * \return Return 0 on success. If the variable does not exist -1 will be
 * returned. If the variable is readonly then -2 will be returned.
 */
int Hub_Var_setValue(const char* name, double value) {
    static char* watch_0 = "WATCH";
    char value_str[32];

    Hub_Var* var = Dictionary_get(var_cache, name);
    Hub_Client* subscriber;
    Comm_Message* message;
    Comm_PackedMessage* packed;

    if(var == NULL) {
        return -1;
    }
    
    if(var->readonly) {
        return -2;
    }

    pthread_rwlock_wrlock(&var->lock);
    var->value = value;
    if(var->persistent) {
        Hub_Var_flushPersistent();
    }
    pthread_rwlock_unlock(&var->lock);

    /* Don't waste time building the message if there are no subscribers */
    if(List_getSize(var->subscribers) == 0) {
        return 0;
    }

    message = Comm_Message_new(3);
    message->components[0] = watch_0;
    message->components[1] = (char*) name;
    message->components[2] = value_str;

    pthread_rwlock_rdlock(&var->lock);
    snprintf(value_str, sizeof(value_str), "%f", var->value);

    packed = Comm_packMessage(message);
    for(int i = 0; (subscriber = List_get(var->subscribers, i)) != NULL; i++) {
        Hub_Net_sendPackedMessage(subscriber, packed);
    }
    pthread_rwlock_unlock(&var->lock);

    Comm_Message_destroy(message);

    return 0;
}

int Hub_Var_addSubscriber(Hub_Client* client, const char* name) {
    Hub_Var* var = Dictionary_get(var_cache, name);

    if(var == NULL) {
        return -1;
    }

    pthread_rwlock_wrlock(&var->lock);
    List_append(var->subscribers, client);
    pthread_rwlock_unlock(&var->lock);

    List_append(client->subscribed_vars, var);

    return 0;
}

int Hub_Var_deleteSubscriber(Hub_Client* client, const char* name) {
    Hub_Var* var = Dictionary_get(var_cache, name);
    int i;

    if(var == NULL) {
        return -1;
    }

    pthread_rwlock_wrlock(&var->lock);
    i = List_indexOf(var->subscribers, client);
    if(i == -1) {
        return -1;
    }

    List_remove(var->subscribers, i);
    pthread_rwlock_unlock(&var->lock);

    i = List_indexOf(client->subscribed_vars, var);
    if(i == -1) {
        return -2;
    }

    List_remove(client->subscribed_vars, i);

    return 0;
}

/**
 * \brief Close the variable subsystem
 *
 * Free memory and close background threads associated with this subsystem
 */
void Hub_Var_close(void) {
    List* var_names;
    char* var_name;
    Hub_Var* var;

    if(persistent_variables) {
        Task_kill(db_flush_handle);
        Task_wait(db_flush_handle);
        List_destroy(persistent_variables);
    }

    if(var_cache) {
        var_names = Dictionary_getKeys(var_cache);
        while(List_getSize(var_names)) {
            var_name = List_remove(var_names, 0);
            var = Dictionary_get(var_cache, var_name);

            free(var->name);
            free(var);
        }

        List_destroy(var_names);
        Dictionary_destroy(var_cache);
    }
}

/** \} */
