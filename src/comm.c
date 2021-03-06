/**
 * \file
 * \brief Low-level communication
 */

#include "seawolf.h"
#include "seawolf/mem_pool.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>

/**
 * \defgroup Comm Low-level communication
 * \ingroup Communications
 * \brief Provides support for sending and receiving messages directly to and from the hub
 * \{
 */

/**
 * \cond Comm_Private
 * \internal
 */

/**
 * Maximum number of consective errors allowed while attempting to receive data
 * from the hub before the application is terminated
 */
#define MAX_RECEIVE_ERROR 5

/**
 * The response set starts out with this many space and will grow by this ammount any time more room is needed
 */
#define RESPONSE_SET_GROW 8

/**
 * Maximum request ID that can be embedded in a message. 16-bits in the packed
 * message are reserved for the request ID allowing values up to this
 */
#define MAX_REQUEST_ID ((uint32_t)0xffff)

/** IP address of server to connect to */
static char* comm_server = NULL;

/** Port of server to connect to */
static uint16_t comm_port = 31427;

/** Password to authenticate using */
static char* auth_password = NULL;

/** The actual socket file descriptor */
static int comm_socket;

/** Task handle for thread that recieves incoming messages */
static Task_Handle receive_thread;

/** Component initialization status */
static bool initialized = false;

/** Signals that the current application has been disconnected from the hub */
static bool hub_shutdown = false;

/** Current size of the response set table */
static size_t response_set_size = RESPONSE_SET_GROW;

/** The response set table itself */
static Comm_Message** response_set = NULL;

/** Specifies whether a response is already pending for a given ID so as to not
    reissue that ID before a response is returned */
static bool* response_pending = NULL;

/** Response set mutex lock */
static pthread_mutex_t response_set_lock = PTHREAD_MUTEX_INITIALIZER;

/** New response available conditional */
static pthread_cond_t new_response = PTHREAD_COND_INITIALIZER;

static void Comm_authenticate(void);
static Comm_PackedMessage* Comm_receivePackedMessage(void);
static int Comm_receiveThread(void);

/**
 * \endcond Comm_Private
 */

/**
 * \brief Initialize the Comm component
 *
 * Initialize the Comm component by connecting a hub server at the configured
 * server and port and attempt to authenticate
 *
 * \private
 */
void Comm_init(void) {
    struct sockaddr_in addr;

    if(comm_server == NULL) {
        Logging_log(CRITICAL, "No Comm_server address is set!");
        Seawolf_exitError();
    }

    /* Build connection address */
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(comm_server);
    addr.sin_port = htons(comm_port);

    /* Create socket */
    comm_socket = socket(AF_INET, SOCK_STREAM, 0);
    if(comm_socket == -1) {
        Logging_log(CRITICAL, __Util_format("Unable to create socket: %s", strerror(errno)));
        Seawolf_exitError();
    }

    /* Connect socket */
    if(connect(comm_socket, (struct sockaddr*) &addr, sizeof(addr))) {
        Logging_log(CRITICAL, __Util_format("Unable to connect to Comm server: %s", strerror(errno)));
        Seawolf_exitError();
    }

    /* Prepare response set */
    response_set = calloc(response_set_size, sizeof(Comm_Message*));
    response_pending = calloc(response_set_size, sizeof(bool));

    /* Run receive thread */
    initialized = true;
    receive_thread = Task_background(&Comm_receiveThread);

    /* Authenticate */
    Comm_authenticate();
}

/**
 * \brief Perform authentication with the hub
 *
 * Authenticate with the hub server using the password specified by a call to
 * Comm_setPassword()
 */
static void Comm_authenticate(void) {
    static char* namespace = "COMM";
    static char* command = "AUTH";

    if(auth_password) {
        Comm_Message* auth_message = Comm_Message_new(3);
        Comm_Message* response;

        auth_message->components[0] = namespace;
        auth_message->components[1] = command;
        auth_message->components[2] = auth_password;

        Comm_assignRequestID(auth_message);
        response = Comm_sendMessage(auth_message);

        if(response == NULL || strcmp(response->components[1], "SUCCESS") != 0) {
            Logging_log(CRITICAL, "Failed to authenticate with hub server!");
        } else {
            MemPool_free(auth_message->alloc);
            MemPool_free(response->alloc);
            return;
        }
    } else {
        Logging_log(CRITICAL, "No Comm_password set. Unable to connect to Comm server");
    }

    Seawolf_exitError();
}

/**
 * \brief Receive a packed message from the hub socket
 *
 * Receive a message from the hub and return at Comm_PackedMessage object
 * representing this received object
 *
 * \return A new Comm_PackedMessage object
 */
static Comm_PackedMessage* Comm_receivePackedMessage(void) {
    Comm_PackedMessage* packed_message;
    uint16_t total_data_size;
    int n;

    n = recv(comm_socket, &total_data_size, sizeof(uint16_t), MSG_WAITALL|MSG_PEEK);
    if(n != sizeof(uint16_t)) {
        return NULL;
    }

    total_data_size = ntohs(total_data_size);
    packed_message = Comm_PackedMessage_new();
    packed_message->length = total_data_size + COMM_MESSAGE_PREFIX_LEN;
    packed_message->data = MemPool_reserve(packed_message->alloc, packed_message->length);

    n = recv(comm_socket, packed_message->data, packed_message->length, MSG_WAITALL);
    if(n != packed_message->length) {
        MemPool_free(packed_message->alloc);
        return NULL;
    }

    return packed_message;
}

/**
 * \brief Message receive loop
 *
 * Spawned by Comm_init() to receive incoming messages and process/queue them
 *
 * \return Returns 0 when shutting down (after a call to Comm_close())
 */
static int Comm_receiveThread(void) {
    Comm_PackedMessage* packed_message;
    Comm_Message* message;
    unsigned short error_count = 0;

    while(initialized) {
        packed_message = Comm_receivePackedMessage();

        /* Receive error */
        if(packed_message == NULL) {
            if(Seawolf_closing()) {
                /* Library is closing and we've already been disconnected from
                   the hub. Specify that the hub is gone and exit the main
                   loop */
                hub_shutdown = true;
                break;
            }

            error_count++;
            if(error_count > MAX_RECEIVE_ERROR) {
                hub_shutdown = true;
                Logging_log(CRITICAL, "Excessive read errors (lost connection to hub), terminating!");
                Seawolf_exitError();

                /* It's possible for Seawolf_exitError to have been called
                   elsewhere which presents a race condition. If
                   Seawolf_exitError returns then ensure we exit the loop */
                break;
            }

            continue;
        }

        /* Received good packet, reset error count */
        error_count = 0;

        /* Unpack message */
        message = Comm_unpackMessage(packed_message);

        if(message->request_id != 0) {
            pthread_mutex_lock(&response_set_lock);
            response_set[message->request_id] = message;
            pthread_cond_broadcast(&new_response);
            pthread_mutex_unlock(&response_set_lock);
        } else if(strcmp(message->components[0], "NOTIFY") == 0) {
            /* Inbound notification */
            Notify_inputMessage(message);
        } else if(strcmp(message->components[0], "WATCH") == 0) {
            /* Inbound variable subscription udpdate */
            Var_inputMessage(message);
        } else if(strcmp(message->components[0], "COMM") == 0) {
            if(strcmp(message->components[1], "KICKING") == 0) {
                hub_shutdown = true;
                Logging_log(ERROR, __Util_format("I've been kicked: %s", message->components[2]));
                Seawolf_exitError();
            }
            MemPool_free(message->alloc);
        } else {
            /* Unknown, unsolicited message */
            MemPool_free(message->alloc);
        }
    }

    /* Wake up any stuck Comm_sendMessage call */
    pthread_mutex_lock(&response_set_lock);
    pthread_cond_broadcast(&new_response);
    pthread_mutex_unlock(&response_set_lock);

    return 0;
}

/**
 * \brief Send a message to the hub
 *
 * Send a message given as a Comm_Message to the connected hub after
 * packing. If a response is expected, block until the response is received and
 * return it.
 *
 * \param message A pointer to a Comm_Message representing the message to be
 * sent
 * \return If a response is expected, block until the response is available and
 * return the unpacked response. Otherwise, return NULL
 */
Comm_Message* Comm_sendMessage(Comm_Message* message) {
    static pthread_mutex_t send_lock = PTHREAD_MUTEX_INITIALIZER;
    Comm_PackedMessage* packed_message;
    Comm_Message* response = NULL;
    int n;

    if(hub_shutdown) {
        return NULL;
    }

    /* Pack message */
    packed_message = Comm_packMessage(message);

    /* Send data */
    pthread_mutex_lock(&send_lock);
    n = send(comm_socket, packed_message->data, packed_message->length, 0);
    pthread_mutex_unlock(&send_lock);

    /* Send error */
    if(n < 0) {
        hub_shutdown = true;
        Logging_log(CRITICAL, "Unable to send message (lost connection to hub), terminating!");
        Seawolf_exitError();
        return NULL;
    }

    /* Expect a response and wait for it */
    if(message->request_id != 0) {
        pthread_mutex_lock(&response_set_lock);
        while(response_set[message->request_id] == NULL) {
            /* Woken up during shutdown. Return NULL */
            if(hub_shutdown) {
                return NULL;
            }

            pthread_cond_wait(&new_response, &response_set_lock);
        }

        response = response_set[message->request_id];
        response_pending[message->request_id] = false;
        response_set[message->request_id] = NULL;

        pthread_mutex_unlock(&response_set_lock);
    }

    return response;
}

/**
 * \brief Assign a ID for a request message
 *
 * If a message is to be sent and requires a responses than a request ID must be
 * assigned to the message
 *
 * \param message The message to assign an ID to
 */
void Comm_assignRequestID(Comm_Message* message) {
    static uint32_t last_id = 1;

    pthread_mutex_lock(&response_set_lock);

    message->request_id = last_id;
    while(response_pending[message->request_id] == true) {
        message->request_id = (message->request_id % (response_set_size - 1)) + 1;

        /* Every available ID is taken, make space for more and make the
           response ID the next available one */
        if(message->request_id == last_id && response_set_size + RESPONSE_SET_GROW < MAX_REQUEST_ID) {
            last_id = response_set_size;
            message->request_id = last_id;

            response_set = realloc(response_set, sizeof(Comm_Message*) * (response_set_size + RESPONSE_SET_GROW));
            response_pending = realloc(response_pending, sizeof(bool) * (response_set_size + RESPONSE_SET_GROW));

            memset(response_set + response_set_size, 0, RESPONSE_SET_GROW * sizeof(Comm_Message*));
            memset(response_pending + response_set_size, 0, RESPONSE_SET_GROW * sizeof(bool));

            response_set_size += RESPONSE_SET_GROW;
        }
    }

    response_set[message->request_id] = NULL;
    response_pending[message->request_id] = true;

    pthread_mutex_unlock(&response_set_lock);
}

/**
 * \brief Pack a message
 *
 * Return a packed message constructed from the given message
 *
 * \param message The message to packe
 * \return The packed equivalent of message
 */
Comm_PackedMessage* Comm_packMessage(Comm_Message* message) {
    Comm_PackedMessage* packed_message = Comm_PackedMessage_newWithAlloc(message->alloc);
    size_t total_data_length = 0;
    size_t* component_lengths = MemPool_reserve(message->alloc, sizeof(size_t) * message->count);
    char* buffer;
    int i;

    /* Add length of each message and space for a null terminator for each */
    for(i = 0; i < message->count; i++) {
        component_lengths[i] = strlen(message->components[i]) + 1;
        total_data_length += component_lengths[i];
    }

    /* Store message information */
    packed_message->length = total_data_length + COMM_MESSAGE_PREFIX_LEN;
    packed_message->data = MemPool_reserve(message->alloc, packed_message->length);

    /* Build packed message header */
    ((uint16_t*)packed_message->data)[0] = htons(total_data_length);
    ((uint16_t*)packed_message->data)[1] = htons(message->request_id);
    ((uint16_t*)packed_message->data)[2] = htons(message->count);

    /* Copy message components */
    buffer = packed_message->data + COMM_MESSAGE_PREFIX_LEN;
    for(i = 0; i < message->count; i++) {
        memcpy(buffer, message->components[i], component_lengths[i]);
        buffer += component_lengths[i];
    }

    return packed_message;
}

/**
 * \brief Unpack a message
 *
 * Unpack and return the given packed message. The returned message can be freed
 * with a call to Comm_Message_destroyUnpacked()
 *
 * \param packed_message A packed message to unpack
 * \return The unpacked message
 */
Comm_Message* Comm_unpackMessage(Comm_PackedMessage* packed_message) {
    Comm_Message* message = Comm_Message_newWithAlloc(packed_message->alloc, 0);
    size_t data_length = ntohs(((uint16_t*)packed_message->data)[0]);

    /* Build message meta information */
    message->request_id = ntohs(((uint16_t*)packed_message->data)[1]);
    message->count = ntohs(((uint16_t*)packed_message->data)[2]);

    if(message->count == 0) {
        message->components = NULL;
        return message;
    }

    message->components = MemPool_reserve(message->alloc, sizeof(char*) * message->count);

    /* Extract components -- we allocate all the space to the first and use the
       rest of the elements as indexes */
    message->components[0] = MemPool_reserve(message->alloc, data_length);
    memcpy(message->components[0], packed_message->data + COMM_MESSAGE_PREFIX_LEN, data_length);

    /* Point the rest of the components into the space allocated to the first */
    for(int i = 1; i < message->count; i++) {
        message->components[i] = message->components[i-1] + strlen(message->components[i-1]) + 1;
    }

    return message;
}

/**
 * \brief Create a new message
 *
 * Create a new message with space for the given number of components. Space is
 * only allocated for the char pointers to the components, not to the
 * components themselves. Space for the components should be allocated and freed
 * separately.
 *
 * \param alloc The MemPool allocation to allocate space for this message from
 * \param component_count The number of components to make space for. If component_count is 0, no allocation is done
 * \return A new message
 */
Comm_Message* Comm_Message_newWithAlloc(MemPool_Alloc* alloc, unsigned int component_count) {
    Comm_Message* message = MemPool_reserve(alloc, sizeof(Comm_Message));

    message->request_id = 0;
    message->count = component_count;
    message->components = NULL;
    message->alloc = alloc;

    if(component_count) {
        message->components = MemPool_reserve(alloc, sizeof(char*) * component_count);
    }

    return message;
}

/**
 * \brief Create a new message
 *
 * Allocate and return a new message using a new allocation
 *
 * \param component_count The number of components to make space for
 * \return The newly allocated message
 * \see Comm_Message_newWithAlloc
 */
Comm_Message* Comm_Message_new(unsigned int component_count) {
    MemPool_Alloc* alloc = MemPool_alloc();
    return Comm_Message_newWithAlloc(alloc, component_count);
}

/**
 * \brief Create a new packed message object
 *
 * Return a new, emtpy packed message. No space is allocated to store data and
 * this should be allocated separately.
 *
 * \param alloc The MemPool allocation to allocate space for this message from
 * \return A new packed message object
 */
Comm_PackedMessage* Comm_PackedMessage_newWithAlloc(MemPool_Alloc* alloc) {
    Comm_PackedMessage* packed_message = MemPool_reserve(alloc, sizeof(Comm_PackedMessage));

    packed_message->length = 0;
    packed_message->data = NULL;
    packed_message->alloc = alloc;

    return packed_message;
}

/**
 * \brief Create a new packed message object
 *
 * Return a new packed message object allocated from a new MemPool allocation
 *
 * \return A new packed message object
 * \see Comm_PackedMessage_newWithAlloc
 */
Comm_PackedMessage* Comm_PackedMessage_new(void) {
    MemPool_Alloc* alloc = MemPool_alloc();
    return Comm_PackedMessage_newWithAlloc(alloc);
}

/**
 * \brief Set the hub password
 *
 * Set the password to use when authenticating with the hub
 *
 * \param password The password to authenticate with
 */
void Comm_setPassword(const char* password) {
    auth_password = strdup(password);
}

/**
 * \brief Set the server to connect to
 *
 * Specify the server to connect to as an IP address given as a string
 *
 * \param server The IP address of the server to connect to given as a string
 */
void Comm_setServer(const char* server) {
    comm_server = strdup(server);
}

/**
 * \brief Set the hub server port
 *
 * Specify the port to connect to when connceting to the hub
 *
 * \param port The port number to connect to
 */
void Comm_setPort(uint16_t port) {
    comm_port = port;
}

/**
 * \brief Destroy a message
 *
 * Free all memory associated with a message and any packed message derived from
 * this message
 *
 * \param message The message to free
 */
void Comm_Message_destroy(Comm_Message* message) {
    MemPool_free(message->alloc);
}

/**
 * \brief Close the Comm component
 *
 * Close the Comm component; close all connections, free memory, etc.
 *
 * \private
 */
void Comm_close(void) {
    Comm_Message* message;

    /* This check is necessary if an error condition is reached in Comm_init */
    if(initialized) {
        if(!hub_shutdown) {
            message = Comm_Message_new(2);
            message->components[0] = MemPool_strdup(message->alloc, "COMM");
            message->components[1] = MemPool_strdup(message->alloc, "SHUTDOWN");
            Comm_assignRequestID(message);

            Comm_sendMessage(message);
            MemPool_free(message->alloc);
        }

        shutdown(comm_socket, SHUT_RDWR);
        Task_wait(receive_thread);

        free(response_set);
        free(response_pending);

        initialized = false;
    }

    if(comm_server) {
        free(comm_server);
    }

    if(auth_password) {
        free(auth_password);
    }
}

/** \} */
