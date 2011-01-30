/**
 * \file
 * \brief Notification support
 */

#include "seawolf.h"

/** True if the notify component has been initialized */
static bool initialized = false;

/** Queue of buffered, incoming messages */
static Queue* notification_queue = NULL;

/**
 * \defgroup Notify Notifications (broadcast messages)
 * \ingroup Communications
 * \brief Notify related routines
 * \{
 */

/**
 * \brief Initialize notify component
 * \private
 */
void Notify_init() {
    notification_queue = Queue_new();
    initialized = true;
}

/**
 * \brief Input a new message
 * \private
 *
 * Provide a new message for the incoming notification queue
 *
 * \param message New message
 */
void Notify_inputMessage(Comm_Message* message) {
    char* msg = message->components[2];

    if(initialized) {
        Queue_append(notification_queue, strdup(msg));
    }
    Comm_Message_destroy(message);

    int queue_size = Queue_getSize(notification_queue);
    if(queue_size >= 5) {
        Logging_log(CRITICAL, Util_format("Notification queue very long! (%d)", queue_size));
    }
}

/**
 * \brief Get next message
 *
 * Get the next notification message available
 *
 * \param[out] action Pointer into which to store the notification action
 * \param[out] param Pointer into which to store the notification parameter
 */
void Notify_get(char* action, char* param) {
    char* msg;
    char* tmp;

    /* Get the next message */
    msg = Queue_pop(notification_queue, true);

    /* Split message */
    tmp = msg;
    while(*tmp != ' ') {
        tmp++;
    }
    *tmp = '\0';

    /* Copy components */
    if(action != NULL) {
        strcpy(action, msg);
    }
    if(param != NULL) {
        strcpy(param, tmp + 1);
    }

    free(msg);
}

/**
 * \brief Get next message
 *
 * Same as Notify_get() but allocates memory to store the action and parameter
 * internally. The calling function should pass the returned data to
 * Notify_freeNotification() to release the allcoated memory.
 *
 * \return Two strings. [0] is the action, [1] is the parameter
 */
char** Notify_getWithAlloc(void) {
    char** notification = malloc(2 * sizeof(char*));
    char* tmp;
    char* msg;

    /* Read message */
    msg = Queue_pop(notification_queue, true);

    /* Split message */
    tmp = msg;
    while(*tmp != ' ') {
        tmp++;
    }
    *tmp = '\0';

    notification[0] = msg;
    notification[1] = tmp + 1;

    return notification;
}

/**
 * \brief Free a notification
 *
 * Free a notification allocated and returned by Notify_getWithAlloc()
 *
 * \param notification The notification to free. Should be as was returned by
 *   Notify_getWithAlloc().
 */
void Notify_freeNotification(char** notification) {
    free(notification[0]);
    free(notification);
}

/**
 * \brief Send a notification
 *
 * Send out a notification
 *
 * \param action Action component of notification
 * \param param Parameter component of notification
 */
void Notify_send(char* action, char* param) {
    static char* namespace = "NOTIFY";
    static char* command = "OUT";
    Comm_Message* notify_msg = Comm_Message_new(3);

    notify_msg->components[0] = namespace;
    notify_msg->components[1] = command;
    notify_msg->components[2] = __Util_format("%s %s", action, param);

    Comm_sendMessage(notify_msg);
    Comm_Message_destroy(notify_msg);
}

/**
 * \brief Register a new filter
 *
 * Register a new filter with the notification system. Incoming messages must
 * match a filter in order to be returned by Notify_get(). If no filters are
 * active then all messages are rejected.
 *
 * There are three kinds of filters,
 *  - FILTER_MATCH requires the entire message to match
 *  - FILTER_ACTION requires the action of the message to match
 *  - FILTER_PREFIX requires some sequences of characters at the beginning of the message to match
 *
 * All existing filters can be removed by specifying NULL for the filter
 * argument
 *
 * \param filter_type One of FILTER_MATCH, FILTER_ACTION, or FILTER_PREFIX.
 * \param filter The filter text, applied as described by the filter_type
 */
void Notify_filter(Notify_FilterType filter_type, char* filter) {
	Comm_Message* message;
	static char* NOTIFY = "NOTIFY";
	static char* ADD_FILTER = "ADD_FILTER";
	static char* CLEAR_FILTERS = "CLEAR_FILTERS";

    if(filter == NULL) {
    	message = Comm_Message_new(2);
    	message->components[0] = NOTIFY;
    	message->components[1] = CLEAR_FILTERS;
    } else {
    	message = Comm_Message_new(4);
    	message->components[0] = NOTIFY;
    	message->components[1] = ADD_FILTER;
    	message->components[2] = __Util_format("%d", (int) filter_type);
    	message->components[3] = filter;
    }

    Comm_sendMessage(message);
	Comm_Message_destroy(message);
}

/**
 * \brief Close initialize component
 * \private
 */
void Notify_close() {
    char* msg;

    if(initialized) {
        /* Free remaining messages */
        while((msg = Queue_pop(notification_queue, false)) != NULL) {
            free(msg);
        }
        Queue_destroy(notification_queue);

        initialized = false;
    }
}

/** \} */
