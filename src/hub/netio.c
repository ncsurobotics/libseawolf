
/* Local includes */
#include "seawolf.h"
#include "seawolf_hub.h"

/* Networking includes */
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>

Comm_Message* Hub_Net_receiveMessage(Hub_Client* client) {
    Comm_PackedMessage* packed_message;
    Comm_Message* message;
    uint16_t total_data_size;
    size_t received;
    int n;

    packed_message = Comm_PackedMessage_new();
    packed_message->length = COMM_MESSAGE_PREFIX_LEN;
    packed_message->data = malloc(packed_message->length);

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
    packed_message->data = realloc(packed_message->data, packed_message->length);

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
            
    /* Unpack message and destroy packed message */
    message = Comm_unpackMessage(packed_message);
    Comm_PackedMessage_destroy(packed_message);
    
    return message;

 receive_error:
    Hub_Logging_log(ERROR, "Error receiving data (lost connection to client). Closing connection");
    Comm_PackedMessage_destroy(packed_message);
    client->state = DEAD;
    return NULL;

}

int Hub_Net_sendPackedMessage(Hub_Client* client, Comm_PackedMessage* packed_message) {
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

int Hub_Net_sendMessage(Hub_Client* client, Comm_Message* message) {
    Comm_PackedMessage* packed_message = Comm_packMessage(message);
    int n = -1;

    /* Send packed message */
    n = Hub_Net_sendPackedMessage(client, packed_message);
    
    /* Destroy sent message */
    Comm_PackedMessage_destroy(packed_message);

    return n;
}

void Hub_Net_responseDestroy(Comm_Message* response) {
    for(int i = 0; i < response->count; i++) {
        free(response->components[i]);
    }
    Comm_Message_destroy(response);
}
