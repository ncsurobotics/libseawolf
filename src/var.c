/**
 * \file
 * \brief Shared variable support
 */

#include "seawolf.h"

typedef struct {
    float* writeback;

    float last;
    float current;

    bool poked;

    pthread_rwlock_t lock;
} Subscription;

/** If true, then notications are sent out with variable updates */
static bool notify = true;

/** Component initialized */
static bool initialized = false;

/** Read only variable cache */
static Dictionary* ro_cache = NULL;

static Dictionary* subscriptions = NULL;

static bool data_available = false;
static pthread_mutex_t data_available_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t new_data_available = PTHREAD_COND_INITIALIZER;

static pthread_rwlock_t subscriptions_lock = PTHREAD_RWLOCK_INITIALIZER;

static void Var_inputNewValue(char* name, float value);

/**
 * \defgroup Var Shared variable
 * \ingroup Communications
 * \brief Provides functions for setting and retrieving shared variables
 * \{
 */

/**
 * \brief Var component initialization
 * \private
 */
void Var_init(void) {
    ro_cache = Dictionary_new();
    subscriptions = Dictionary_new();
    initialized = true;
}

/**
 * \brief Get a variable
 *
 * Get the value of a variable
 *
 * \param name The variable to retrieve
 * \return The variable value
 */
float Var_get(char* name) {
    static char* namespace = "VAR";
    static char* command = "GET";

    Comm_Message* variable_request;
    Comm_Message* response;
    Subscription* subscription;
    float value;
    float* cached;

    pthread_rwlock_rdlock(&subscriptions_lock); {
        subscription = Dictionary_get(subscriptions, name);
        if(subscription) {
            pthread_rwlock_wrlock(&subscription->lock); {
                subscription->last = subscription->current;
                subscription->poked = false;
            }
            pthread_rwlock_unlock(&subscription->lock);
            value = subscription->current;
        }
    }
    pthread_rwlock_unlock(&subscriptions_lock);
    
    if(subscription) {
        return value;
    }

    cached = Dictionary_get(ro_cache, name);
    if(cached) {
        return (*cached);
    }

    variable_request = Comm_Message_new(3);
    variable_request->components[0] = namespace;
    variable_request->components[1] = command;
    variable_request->components[2] = name;

    Comm_assignRequestID(variable_request);
    response = Comm_sendMessage(variable_request);

    if(strcmp(response->components[1], "VALUE") == 0) {
        value = atof(response->components[3]);
        if(strcmp(response->components[2], "RO") == 0) {
            cached = malloc(sizeof(float));
            (*cached) = value;
            Dictionary_set(ro_cache, name, cached);
        }
    } else {
        Logging_log(ERROR, __Util_format("Invalid variable, '%s'", name));
        value = 0;
    }

    Comm_Message_destroy(response);
    Comm_Message_destroy(variable_request);

    return value;
}

/**
 * \brief Set a variable
 *
 * Set a variable to a given value
 *
 * \param name Variable to set
 * \param value Value to set the variable to
 */
void Var_set(char* name, float value) {
    static char* namespace = "VAR";
    static char* command = "SET";

    Comm_Message* variable_set = Comm_Message_new(4);

    variable_set->components[0] = namespace;
    variable_set->components[1] = command;
    variable_set->components[2] = name;
    variable_set->components[3] = strdup(__Util_format("%.4f", value));

    Comm_sendMessage(variable_set);

    if(notify) {
        Notify_send("UPDATED", name);
    }

    if(Dictionary_get(subscriptions, name)) {
        Var_inputNewValue(name, value);
    }

    free(variable_set->components[3]);
    Comm_Message_destroy(variable_set);
}

int Var_subscribe(char* name) {
    static char* namespace = "WATCH";
    static char* command = "ADD";

    Comm_Message* request = Comm_Message_new(3);
    Subscription* s = malloc(sizeof(Subscription));

    s->writeback = NULL;
    s->last = Var_get(name);
    s->current = s->last;
    s->poked = false;
    pthread_rwlock_init(&s->lock, NULL);

    request->components[0] = namespace;
    request->components[1] = command;
    request->components[2] = name;

    pthread_rwlock_wrlock(&subscriptions_lock); {
        Dictionary_set(subscriptions, name, s);
        Comm_sendMessage(request);
    }
    pthread_rwlock_unlock(&subscriptions_lock);

    Comm_Message_destroy(request);

    return 0;
}

int Var_bind(char* name, float* store_to) {
    Subscription* s;

    Var_subscribe(name);

    pthread_rwlock_rdlock(&subscriptions_lock); {
        s = Dictionary_get(subscriptions, name);
        s->writeback = store_to;
    }
    pthread_rwlock_unlock(&subscriptions_lock);

    (*s->writeback) = s->current;
    
    return 0;
}

void Var_unsubscribe(char* name) {
    static char* namespace = "WATCH";
    static char* command = "DEL";

    Comm_Message* request = Comm_Message_new(3);
    Subscription* s;

    request->components[0] = namespace;
    request->components[1] = command;
    request->components[2] = name;

    Comm_sendMessage(request);
    Comm_Message_destroy(request);

    s = Dictionary_get(subscriptions, name);
    if(s == NULL) {
        /* We probably never were subscribed. We'll likely be booted by the hub
           shortly */
        return;
    }

    pthread_rwlock_wrlock(&subscriptions_lock); {
        Dictionary_remove(subscriptions, name);
        free(s);
    }
    pthread_rwlock_unlock(&subscriptions_lock);
}

void Var_unbind(char* name) {
    Var_unsubscribe(name);
}

bool Var_stale(char* name) {
    Subscription* s;
    bool stale;

    pthread_rwlock_rdlock(&subscriptions_lock); {
        s = Dictionary_get(subscriptions, name);
        if(s == NULL) {
            Logging_log(CRITICAL, Util_format("Subscription call on unsubscribed variable '%s'", name));
            Seawolf_exitError();
        }

        pthread_rwlock_rdlock(&s->lock); {
            stale = s->poked && (s->last != s->current);
        }
        pthread_rwlock_unlock(&s->lock);
    }
    pthread_rwlock_unlock(&subscriptions_lock);

    return stale;
}

bool Var_poked(char* name) {
    Subscription* s;
    bool poked;

    pthread_rwlock_rdlock(&subscriptions_lock); {
        s = Dictionary_get(subscriptions, name);
        if(s == NULL) {
            Logging_log(CRITICAL, Util_format("Subscription call on unsubscribed variable '%s'", name));
            Seawolf_exitError();
        }

        pthread_rwlock_rdlock(&s->lock); {
            poked = s->poked;
        }
        pthread_rwlock_unlock(&s->lock);
    }
    pthread_rwlock_unlock(&subscriptions_lock);

    return poked;
}

void Var_touch(char* name) {
    Subscription* s;

    pthread_rwlock_rdlock(&subscriptions_lock); {
        s = Dictionary_get(subscriptions, name); 
        if(s == NULL) {
            Logging_log(CRITICAL, Util_format("Subscription call on unsubscribed variable '%s'", name));
            Seawolf_exitError();
        }

        pthread_rwlock_wrlock(&s->lock); {
            s->poked = false;
            s->last = s->current;
        }
        pthread_rwlock_unlock(&s->lock);
    }
    pthread_rwlock_unlock(&subscriptions_lock);
}

void Var_sync(void) {
    pthread_mutex_lock(&data_available_lock); {
        while(data_available == false) {
            pthread_cond_wait(&new_data_available, &data_available_lock);
        }
        data_available = true;
    }
    pthread_mutex_unlock(&data_available_lock);
}

static void Var_inputNewValue(char* name, float value) {
    Subscription* s;

    /* OH NOES!!!

       IT'S THE LOCK-NEST MONSTER!!! */

    pthread_rwlock_rdlock(&subscriptions_lock); {
        s = Dictionary_get(subscriptions, name);
        if(s != NULL) {
            pthread_rwlock_wrlock(&s->lock); {
                s->last = s->current;
                s->current = value;
                s->poked = true;

                if(s->writeback) {
                    (*s->writeback) = s->current;
                }
            }
            pthread_rwlock_unlock(&s->lock);
        }

        pthread_mutex_lock(&data_available_lock); {
            pthread_cond_broadcast(&new_data_available);
            data_available = true;
        }
        pthread_mutex_unlock(&data_available_lock);
    }
    pthread_rwlock_unlock(&subscriptions_lock);
}

void Var_inputMessage(Comm_Message* message) {
    float value;

    if(message->count == 3) {
        value = atof(message->components[2]);
        Var_inputNewValue(message->components[1], value);
    }

    Comm_Message_destroy(message);
}

/**
 * \brief Control auto-notifications
 *
 * If set to true then notifications in the form Notify_send("UPDATED", name)
 * will be sent every time a Var_set() call is made. If false then no such
 * notifications will be automatically set.
 *
 * \param autonotify If true, notifications are sent whenever Var_set is
 * called. If false, no notifications are sent
 */
void Var_setAutoNotify(bool autonotify) {
    notify = autonotify;
}

/**
 * \brief Close the Var component
 * \private
 */
void Var_close(void) {
    if(initialized) {
        List* keys = Dictionary_getKeys(ro_cache);
        int n = List_getSize(keys);
        for(int i = 0; i < n; i++) {
            free(Dictionary_get(ro_cache, List_get(keys, i)));
        }

        List_destroy(keys);
        Dictionary_destroy(ro_cache);
        initialized = false;
    }
}

/** \} */
