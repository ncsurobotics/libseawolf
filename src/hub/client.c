
#include "seawolf.h"
#include "seawolf_hub.h"

static char* COMM = "COMM";
static char* CLOSING = "CLOSING";
static char* KICKING = "KICKING";

/**
 * Create a new client object
 */
Hub_Client* Hub_Client_new(int sock) {
    Hub_Client* client;
    pthread_mutexattr_t recursive_mutex;

    /* Initialize attributes for client send locks */
    pthread_mutexattr_init(&recursive_mutex);
    pthread_mutexattr_settype(&recursive_mutex, PTHREAD_MUTEX_RECURSIVE);

    client = malloc(sizeof(Hub_Client));
    client->sock = sock;
    client->state = UNAUTHENTICATED;
    client->name = NULL;
    client->filters = NULL;
    client->filters_n = 0;
    client->subscribed_vars = List_new();

    pthread_rwlock_init(&client->filter_lock, NULL);
    pthread_rwlock_init(&client->in_use, NULL);
    pthread_mutex_init(&client->lock, &recursive_mutex);

    return client;
}

/**
 * Kick client from the hub
 */
void Hub_Client_kick(Hub_Client* client, char* reason) {
    Comm_Message* message = Comm_Message_new(3);
    message->components[0] = COMM;
    message->components[1] = KICKING;
    message->components[2] = reason;

    pthread_rwlock_rdlock(&client->in_use);
    Hub_Net_markClientClosed(client);
    Hub_Net_sendMessage(client, message);
    pthread_rwlock_unlock(&client->in_use);
    
    Comm_Message_destroy(message);
}

/**
 * Close the clients connection
 */
void Hub_Client_close(Hub_Client* client) {
    Comm_Message* message = Comm_Message_new(2);
    message->request_id = message->request_id;
    message->components[0] = COMM;
    message->components[1] = CLOSING;
    
    Hub_Net_sendMessage(client, message);
    Hub_Net_markClientClosed(client);
    Comm_Message_destroy(message);
}

/**
 * Add a notification filter to a client
 */
void Hub_Client_addFilter(Hub_Client* client, Notify_FilterType type, const char* filter) {
    int i;

    pthread_rwlock_wrlock(&client->filter_lock);
    i = client->filters_n;
    client->filters_n++;
    client->filters = realloc(client->filters, sizeof(char*) * client->filters_n);
    client->filters[i] = malloc(strlen(filter) + 2);

    client->filters[i][0] = (unsigned char) type;
    strcpy(client->filters[i] + 1, filter);
    pthread_rwlock_unlock(&client->filter_lock);
}

/**
 * Remove all client filters
 */
void Hub_Client_clearFilters(Hub_Client* client) {
    if(client->filters == NULL) {
        return;
    }

    pthread_rwlock_wrlock(&client->filter_lock);
    for(int i = 0; i < client->filters_n; i++) {
        free(client->filters[i]);
    }

    free(client->filters);
    client->filters = NULL;
    client->filters_n = 0;
    pthread_rwlock_unlock(&client->filter_lock);
}

/**
 * Check to see if a notification message passes a client's notification filters
 */
bool Hub_Client_checkFilters(Hub_Client* client, Comm_Message* message) {
    assert(strcmp(message->components[0], "NOTIFY") == 0);

    Notify_FilterType type;
    char* filter_body;
    bool r = false;

    pthread_rwlock_rdlock(&client->filter_lock);
    for(int i = 0; i < client->filters_n; i++) {
        type = (Notify_FilterType) client->filters[i][0];
        filter_body = client->filters[i] + 1;

        switch(type) {
        case FILTER_MATCH:
            if(strcmp(message->components[2], filter_body) == 0) {
                r = true;
            }
            break;

        case FILTER_PREFIX:
            for(int j = 0; message->components[2][j] != '\0'; j++) {
                if(filter_body[j] == '\0') {
                    if(message->components[2][j] == ' ') {
                        r = false;
                    } else {
                        break;
                    }
                }
            }
            break;

        case FILTER_ACTION:
            if(strncmp(message->components[2], filter_body, strlen(filter_body)) == 0) {
                r = true;
            }
            break;
        }
    }

    pthread_rwlock_unlock(&client->filter_lock);
    return r;
}
