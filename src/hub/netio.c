/**
 * \file
 * \brief Network IO
 */

#include "seawolf.h"
#include "seawolf_hub.h"

#include <arpa/inet.h>
#include <poll.h>
#include <sys/socket.h>

/**
 * \defgroup netio Network IO
 * \brief Message IO routines
 * \{
 */

static int Hub_Net_sendPackedMessage(Hub_Client* client, Comm_PackedMessage* packed_message);

/**
 * \brief Receive a message from the given client
 *
 * Receive a message which has arrived from the given client. In the event of
 * an error NULL will be returned and the client state may be changed to DEAD.
 *
 * \param client The client to read the message from
 * \return The unpacked message or NULL if an error occured
 */
Comm_Message* Hub_Net_receiveMessage(Hub_Client* client) {
    Comm_PackedMessage* packed_message;
    Comm_Message* message;
    uint16_t total_data_size;
    size_t received;
    int n;

    packed_message = Comm_PackedMessage_new();
    packed_message->length = COMM_MESSAGE_PREFIX_LEN;
    packed_message->data = MemPool_reserve(packed_message->alloc, packed_message->length);

    /* Read in first two bytes without removing them from the socket
       buffer. Theses bytes give us the overall message length. */
    received = 0;
    while(received < COMM_MESSAGE_PREFIX_LEN) {
        n = recv(client->sock, packed_message->data + received, COMM_MESSAGE_PREFIX_LEN - received, 0);
        if(n <= 0) {
            goto receive_error;
        }
        received += n;
    }

    /* Convert to short */
    total_data_size = ntohs(((uint16_t*)packed_message->data)[0]);
    packed_message->length += total_data_size;
    
    /* Reserve the rest of the space */
    MemPool_reserve(packed_message->alloc, total_data_size);

    received = 0;
    while(received < total_data_size) {
        n = recv(client->sock,
                 packed_message->data + COMM_MESSAGE_PREFIX_LEN + received,
                 total_data_size - received, 0);
        if(n <= 0) {
            goto receive_error;
        }
        received += n;
    }

    /* Unpack message */
    message = Comm_unpackMessage(packed_message);
    return message;

 receive_error:
    Hub_Logging_log(ERROR, "Error receiving data (lost connection to client). Closing connection");
    MemPool_free(packed_message->alloc);
    Hub_Net_markClientClosed(client);

    return NULL;
}

/**
 * \brief Send a packed message
 *
 * Send a message which has already been packed
 *
 * \param client Client to send the message to
 * \param packed_message The packed message to send
 * \return The number of bytes sent or -1 in the even of an error
 */
static int Hub_Net_sendPackedMessage(Hub_Client* client, Comm_PackedMessage* packed_message) {
    struct pollfd fd = {.fd = client->sock, .events = POLLOUT};
    int n = -1;

    /* Check if data can be sent without blocking */
    poll(&fd, 1, 0);
    if(fd.revents & POLLOUT) {
        /* Send data */
        n = send(client->sock, packed_message->data, packed_message->length, 0);
    } else {
        /* Socket not ready to accept data */
        Hub_Logging_log(ERROR, "Unable to write data to full network socket");
        return -1;
    }

    return n;
}

/**
 * \brief Send a message
 *
 * Pack the given message and then send the packed message using
 * Hub_Net_sendPackedMessage
 *
 * \param client Client to send the message to
 * \param message The message to pack and send
 * \return The number of bytes sent or -1 in the even of an error
 */
int Hub_Net_sendMessage(Hub_Client* client, Comm_Message* message) {
    Comm_PackedMessage* packed_message = Comm_packMessage(message);
    int n = -1;

    /* Send packed message */
    n = Hub_Net_sendPackedMessage(client, packed_message);

    return n;
}

/**
 * \brief Broadcast a message
 *
 * Efficiently send a message to all connected clients. The message is packed
 * once and then sent to each client
 *
 * \param message The message to broadcast
 */
void Hub_Net_broadcastMessage(Comm_Message* message) {
    Comm_PackedMessage* packed_message = Comm_packMessage(message);
    List* clients = Hub_Net_getConnectedClients();
    int client_count = List_getSize(clients);
    Hub_Client* client;

    for(int i = 0; i < client_count; i++) {
        client = List_get(clients, i);
        if(client->state == CONNECTED) {
            if(Hub_Net_sendPackedMessage(client, packed_message) < 0) {
                /* Failed to send, shutdown client */
                Hub_Logging_log(DEBUG, "Client disconnected, shutting down client");
                Hub_Net_markClientClosed(client);
            }
        }
    }
}

void Hub_Net_broadcastNotification(Comm_Message* message) {
    Comm_PackedMessage* packed_message = Comm_packMessage(message);
    List* clients = Hub_Net_getConnectedClients();
    int client_count = List_getSize(clients);
    Hub_Client* client;

    for(int i = 0; i < client_count; i++) {
        client = List_get(clients, i);
        if(client->state == CONNECTED && Hub_Client_checkFilters(client, message)) {
            if(Hub_Net_sendPackedMessage(client, packed_message) < 0) {
                /* Failed to send, shutdown client */
                Hub_Logging_log(DEBUG, "Client disconnected, shutting down client");
                Hub_Net_markClientClosed(client);
            }
        }
    }
}

/** \} */
