/**
 * \file
 * \brief Request processing
 */

#include "seawolf.h"
#include "seawolf_hub.h"

static int Hub_Process_comm(Comm_Message* message, Comm_Message** response, Hub_Client* client);
static int Hub_Process_notify(Comm_Message* message, Comm_Message** response);
static int Hub_Process_log(Comm_Message* message, Comm_Message** response);
static int Hub_Process_var(Comm_Message* message, Comm_Message** response, Hub_Client* client);

/**
 * \defgroup Process Process
 * \brief Message processing
 * \{
 */

/**
 * \brief Process a message with the COMM prefix
 *
 * Process a message related to core Comm functions. Connection establishment,
 * shutdown, authentication, etc.
 *
 * \param message The received message
 * \param response Pointer to a response pointer which will be allocated
 * \param client The client which sent the receive message
 * \return Return value specifies the scope of the response (RESPOND_TO_SENDER,
 * RESPOND_TO_ALL, etc.)
 */
static int Hub_Process_comm(Comm_Message* message, Comm_Message** response, Hub_Client* client) {
    char* supplied_password = NULL;
    const char* actual_password;

    if(message->count == 3 && strcmp(message->components[1], "AUTH") == 0) {
        actual_password = Hub_Config_getOption("password");
        supplied_password = message->components[2];

        if(actual_password == NULL) {
            Hub_Logging_log(ERROR, "No password set! Refusing to authenticate clients!");
            return RESPOND_TO_NONE;
        }

        *response = Comm_Message_new(2);
        (*response)->request_id = message->request_id;
        (*response)->components[0] = strdup("COMM");

        if(strcmp(supplied_password, actual_password) == 0) {
            (*response)->components[1] = strdup("SUCCESS");
            client->state = CONNECTED;
        } else {
            (*response)->components[1] = strdup("FAILURE");
            client->state = KICKING;
            client->kick_reason = strdup("Authentication failure");
        }

        return RESPOND_TO_SENDER;
    } else if(message->count == 2 && strcmp(message->components[1], "SHUTDOWN") == 0) {
        *response = Comm_Message_new(2);
        (*response)->request_id = message->request_id;
        (*response)->components[0] = strdup("COMM");
        (*response)->components[1] = strdup("CLOSING");
        client->state = PARTING;
        return RESPOND_TO_SENDER;
    }

    return RESPOND_TO_NONE;
}

/**
 * \brief Process an incoming notification
 *
 * Process a received notification message by rebroadcasting the notification
 * to all connected clients
 *
 * \param message The received message
 * \param response The response it stored here
 * \return Response action
 */
static int Hub_Process_notify(Comm_Message* message, Comm_Message** response) {
    if(message->count != 3 || strcmp(message->components[1], "OUT") != 0) {
        return RESPOND_TO_NONE;
    }

    *response = Comm_Message_new(3);
    (*response)->components[0] = strdup("NOTIFY");
    (*response)->components[1] = strdup("IN");
    (*response)->components[2] = strdup(message->components[2]);

    return RESPOND_TO_ALL;
}

/**
 * \brief Process a log message
 *
 * Process a log message requesting a message be centrally logged
 *
 * \param message The recieved messsage
 * \param response Pointer to a location to allocate and store a response
 * \return Response action
 */
static int Hub_Process_log(Comm_Message* message, Comm_Message** response) {
    if(message->count != 4) {
        return RESPOND_TO_NONE;
    }

    Hub_Logging_logWithName(message->components[1], atoi(message->components[2]), message->components[3]);
    return RESPOND_TO_NONE;
}

/**
 * \brief Process a variable message
 *
 * Process a message setting or getting a variable
 *
 * \param message The received message
 * \param response Pointer to a location to allocate and store a respones
 * \param client Pointer the the client initiating the request
 * \return Response action
 */
static int Hub_Process_var(Comm_Message* message, Comm_Message** response, Hub_Client* client) {
    Hub_Var* var;
    int n;

    if(message->count == 3 && strcmp(message->components[1], "GET") == 0) {
        var = Hub_Var_get(message->components[2]);
        if(var == NULL) {
            Hub_Logging_log(ERROR, Util_format("Get attempted on not-existant variable '%s'", message->components[2]));
            client->state = KICKING;
            client->kick_reason = strdup("Invalid variable access");
            return RESPOND_TO_NONE;
        } else {
            *response = Comm_Message_new(4);
            (*response)->request_id = message->request_id;
            (*response)->components[0] = strdup("VAR");
            (*response)->components[1] = strdup("VALUE");

            if(var->readonly) {
                (*response)->components[2] = strdup("RO");
            } else {
                (*response)->components[2] = strdup("RW");
            }

            (*response)->components[3] = strdup(Util_format("%f", var->value));
            return RESPOND_TO_SENDER;
        }
    } else if(message->count == 4 && strcmp(message->components[1], "SET") == 0) {
        n = Hub_Var_setValue(message->components[2], atof(message->components[3]));
        if(n == -1) {
            Hub_Logging_log(ERROR, Util_format("Set attempted on not-existant variable '%s'", message->components[2]));
            client->state = KICKING;
            client->kick_reason = strdup("Invalid variable access");
        } else if(n == -2) {
            Hub_Logging_log(ERROR, Util_format("Set attempted on read-only variable '%s'", message->components[2]));
            client->state = KICKING;
            client->kick_reason = strdup("Invalid variable access");
        }

        return RESPOND_TO_NONE;
    } else {
        /* Invalid request */
    }

    return RESPOND_TO_NONE;
}

/**
 * \brief Process a request
 *
 * Process an incoming message from a client
 *
 * \param message Received message
 * \param response Pointer to a location to allocate and store a possible response
 * \param client Client which the message was received from
 * \return Response action
 */
int Hub_Process_process(Comm_Message* message, Comm_Message** response, Hub_Client* client) {
    int respond_to = RESPOND_TO_NONE;
    *response = NULL;

    if(strcmp(message->components[0], "COMM") == 0) {
        respond_to = Hub_Process_comm(message, response, client);
    } else if(client->state == CONNECTED) {
        if(strcmp(message->components[0], "NOTIFY") == 0) {
            respond_to = Hub_Process_notify(message, response);
        } else if(strcmp(message->components[0], "VAR") == 0) {
            respond_to = Hub_Process_var(message, response, client);
        } else if(strcmp(message->components[0], "LOG") == 0) {
            respond_to = Hub_Process_log(message, response);
        }
    }

    return respond_to;
}

/** \} */
