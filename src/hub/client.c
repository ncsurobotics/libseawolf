
#include "seawolf.h"
#include "seawolf_hub.h"

static char* COMM = "COMM";
static char* CLOSING = "CLOSING";
static char* KICKING = "KICKING";

/**
 * Kick client from the hub
 */
void Hub_Client_kick(Hub_Client* client, char* reason) {
    Comm_Message* message = Comm_Message_new(3);
    message->components[0] = COMM;
    message->components[1] = KICKING;
    message->components[2] = reason;

    Hub_Net_sendMessage(client, message);
    Hub_Net_markClientClosed(client);
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

/*void Hub_Client_addFilter(Hub_Client* client, const char* filter) {
    
}*/
