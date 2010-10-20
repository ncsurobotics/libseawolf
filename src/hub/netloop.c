/**
 * \file
 * \brief Request processing loop
 */

#include "seawolf.h"
#include "seawolf_hub.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>

static void Hub_Net_removeClient(Hub_Client* client);
static void Hub_Net_broadcast(Comm_Message* message);
static void Hub_Net_initServerSocket(void);

/* List of active clients */
static List* clients = NULL;

/** Server socket */
static int svr_sock = 0;

/** Server socket bind address */
struct sockaddr_in svr_addr;

/** Flag to keep Hub_mainLoop running */
static bool run_mainloop = true;

/** Flag used by Hub_mainLoop to signal its completion */
static bool mainloop_running = false;

/** Condition to wait for when waiting for Hub_mainLoop to complete */
static pthread_cond_t mainloop_done = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t mainloop_done_lock = PTHREAD_MUTEX_INITIALIZER;

/**
 * \defgroup netloop Net loop
 * \brief Main hub request loop and support routines
 * \{
 */

/**
 * \brief Remove a client
 *
 * Remove a client from the client list and close the associated socket
 *
 * \param client The client to remove
 */
static void Hub_Net_removeClient(Hub_Client* client) {
    int index = List_indexOf(clients, client);

    shutdown(client->sock, SHUT_RDWR);
    List_remove(clients, index);
    client->state = CLOSED;

    if(client->kick_reason) {
        free(client->kick_reason);
    }
    free(client);
}

/**
 * \brief Broadcast a message to all clients
 *
 * Send the given message to all connected clients
 *
 * \param message The message to broadcast
 */
static void Hub_Net_broadcast(Comm_Message* message) {
    Comm_PackedMessage* packed_message = Comm_packMessage(message);
    Hub_Client* client;
    int client_count = List_getSize(clients);

    for(int i = 0; i < client_count; i++) {
        client = List_get(clients, i);
        if(client->state == CONNECTED) {
            if(Hub_Net_sendPackedMessage(client, packed_message) < 0) {
                /* Failed to send, shutdown client */
                Hub_Logging_log(DEBUG, "Client disconnected, shutting down client");
                client->state = DEAD;
            }
        }
    }
    Comm_PackedMessage_destroy(packed_message);
}

/**
 * \brief Initialize the sever socket
 *
 * Perform all required initialization of the server socket so that it is
 * prepared to being accepting connections
 */
static void Hub_Net_initServerSocket(void) {
    /* Used to set the SO_REUSEADDR socket option on the server socket */
    const int reuse = 1;

    /* Initialize the connection structure to bind the the correct port on all
       interfaces */
    svr_addr.sin_family = AF_INET;
    svr_addr.sin_addr.s_addr = inet_addr(Hub_Config_getOption("bind_address"));
    svr_addr.sin_port = htons(atoi(Hub_Config_getOption("bind_port")));

    /* Create the socket */
    svr_sock = socket(AF_INET, SOCK_STREAM, 0);
    if(svr_sock == -1) {
        Hub_Logging_log(CRITICAL, Util_format("Error creating socket: %s", strerror(errno)));
        Hub_exitError();
    }

    /* Allow localhost address reuse. This allows us to restart the hub after it
       unexpectedly dies and leaves a stale socket */
    setsockopt(svr_sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    /* Bind the socket to the server port/address */
    if(bind(svr_sock, (struct sockaddr*) &svr_addr, sizeof(svr_addr)) == -1) {
        Hub_Logging_log(CRITICAL, Util_format("Error binding socket: %s", strerror(errno)));
        Hub_exitError();
    }

    /* Start listening */
    if(listen(svr_sock, MAX_CLIENTS)) {
        Hub_Logging_log(CRITICAL, Util_format("Error setting socket to listen: %s", strerror(errno)));
        Hub_exitError();
    }
}

/**
 * \brief Perform sychronous pre-shutdown for signal handlers
 *
 * Perform synchronous pre-shutdown for signal handlers. When a signal is
 * recieved and goes unprocessed, Hub_mainLoop will consider this an error
 * condition. Signal handlers can call this before starting a asychronous
 * shutdown to avoid this issue.
 */
void Hub_Net_preClose(void) {
    /* Set main loop to terminate */
    run_mainloop = false;
}

/**
 * \brief Close the net subsystem
 *
 * Perform a controlled shutdown of the net subsystem. Free all associated
 * memory and properly shutdown all associated sockets
 */
void Hub_Net_close(void) {
    int tmp_sock;

    /* If the main loop is not yet running then we don't have much cleanup to
       do. If there is a server socket attempt to close it and then simply
       return */
    if(!mainloop_running) {
        if(svr_sock > 0) {
            close(svr_sock);
        }

        return;
    }

    /* Synchronize exit with mainLoop */
    pthread_mutex_lock(&mainloop_done_lock);

    /* Instruct mainLoop to terminate */
    Hub_Net_preClose();

    /* Attempt to connect a socket to the address of the server socket which
       should cause the blocking select call to wake up */
    tmp_sock = socket(AF_INET, SOCK_STREAM, 0);
    if(connect(tmp_sock, (struct sockaddr*) &svr_addr, sizeof(svr_addr))) {
        Hub_Logging_log(ERROR, "Unable to complete graceful shutdown!");
        Hub_exitError();
    }

    /* Now wait for Hub_mainLoop to terminate */
    while(mainloop_running) {
        pthread_cond_wait(&mainloop_done, &mainloop_done_lock);
    }

    /* Close the temporary socket */
    shutdown(tmp_sock, SHUT_RDWR);
    close(tmp_sock);

    pthread_mutex_unlock(&mainloop_done_lock);
}

/**
 * \brief Hub main loop
 *
 * Main loop which processes client requests and handles all client connections
 */
void Hub_Net_mainLoop(void) {

    /* Used to set the SO_RCVTIMEO (receive timeout) socket option on client
       sockets. 250 milliseconds */
    const struct timeval client_timeout = {.tv_sec = 0, .tv_usec = 250 * 1000};

    /* Temporary storage for new client connections until a Hub_Client structure
       can be allocated for them */
    int client_new = 0;

    /* Default reason provided for a kick if one is not provided */
    char* default_kick = "kicked";

    /* Reason provided on shutdown for kick */
    char* shutdown_kick = "Hub closing";

    /** File descriptor mask passed to select() */
    fd_set fdset_mask_r;
    Comm_Message* client_message;
    Comm_Message* response;
    Comm_PackedMessage* closing_packed;
    Comm_Message* kick_message;
    Hub_Client* client;
    int client_count;
    int action;
    int n, i; /* misc */

    /* Create clients list */
    clients = List_new();

    /* Store partial kicked message */
    kick_message = Comm_Message_new(3);
    kick_message->components[0] = strdup("COMM");
    kick_message->components[1] = strdup("KICKING");
    kick_message->components[2] = NULL;

    /* Create prepacked closing message */
    response = Comm_Message_new(2);
    response->components[0] = strdup("COMM");
    response->components[1] = strdup("CLOSING");
    closing_packed = Comm_packMessage(response);
    Hub_Net_responseDestroy(response);

    /* Create and ready the server socket */
    Hub_Net_initServerSocket();

    /* Begin accepting connections */
    Hub_Logging_log(INFO, "Accepting client connections");

    /* Main loop is now running */
    mainloop_running = true;

    /* Start sending/recieving messages */
    while(run_mainloop) {
        /* Save a copy of the current number of clients */
        client_count = List_getSize(clients);

        /* Zero of the file descriptor set */
        FD_ZERO(&fdset_mask_r);

        /* Add the server socket to the set */
        FD_SET(svr_sock, &fdset_mask_r);

        /* Add each client to the set */
        for(i = 0; i < client_count; i++) {
            client = List_get(clients, i);
            FD_SET(client->sock, &fdset_mask_r);
        }

        /* Perform the select call, return -1 is an error, and 0 means no
           results */
        n = select(FD_SETSIZE, &fdset_mask_r, NULL, NULL, NULL);

        /* When the hub is closing Hub_Net_close will set run_mainloop to 0 and
           then attempt to open a connection to the server socket. This will
           wake up the select call which is why this addtional check is placed
           here */
        if(!run_mainloop) {
            break;
        }

        /* Check for errors from the select call */
        if(n < 0) {
            if(run_mainloop) {
                Hub_Logging_log(ERROR, Util_format("Error selecting from descriptors, attempting to continue: %s", strerror(errno)));
            }
            continue;
        } else if(n == 0) {
            /* Select returned no active sockets. Ignore it and try again */
            continue;
        }

        /* Accept a new connection when the server socket is set */
        if(FD_ISSET(svr_sock, &fdset_mask_r)) {
            client_new = accept(svr_sock, NULL, 0);

            if(client_new < 0) {
                Hub_Logging_log(ERROR, "Error accepting new client connection");
            } else if(client_count >= MAX_CLIENTS) {
                Hub_Logging_log(ERROR, Util_format("Unable to accept new client connection! Maximum clients (%d) exceeded", MAX_CLIENTS));
                shutdown(client_new, SHUT_RDWR);
            } else {
                Hub_Logging_log(DEBUG, "Accepted new client connection");

                /* Set a timeout on receive operations to keep broken client
                   connections from deadlocking the hub */
                setsockopt(client_new, SOL_SOCKET, SO_RCVTIMEO, &client_timeout, sizeof(client_timeout));

                client = malloc(sizeof(Hub_Client));
                client->sock = client_new;
                client->state = UNAUTHENTICATED;
                client->name = NULL;
                client->kick_reason = NULL;

                List_append(clients, client);
            }

            client_new = 0;
        }

        /* Check for incoming data */
        for(i = 0; i < client_count; i++) {
            client = List_get(clients, i);

            if(FD_ISSET(client->sock, &fdset_mask_r)) {
                /* Read message from the client  */
                client_message = Hub_Net_receiveMessage(client);
                if(client_message == NULL) {
                    /* It appears the client has died. Skip it and move on */
                    continue;
                }

                /* Process message */
                action = Hub_Process_process(client_message, &response, client);

                if(action == RESPOND_TO_SENDER || (action == SHUTDOWN_SENDER && response != NULL)) {
                    n = Hub_Net_sendMessage(client, response);
                    if(n < 0 && client->state != DEAD) {
                        /* Failed to send, shutdown client */
                        Hub_Logging_log(DEBUG, "Client disconnected, shutting down client");
                        client->state = DEAD;
                    }
                } else if(action == RESPOND_TO_ALL) {
                    Hub_Net_broadcast(response);
                }

                /* Destroy client message */
                Comm_Message_destroyUnpacked(client_message);

                /* Destroy response */
                if(response) {
                    Hub_Net_responseDestroy(response);
                }
            }
        }

        /* Remove closed clients */
        i = 0;
        while((client = List_get(clients, i)) != NULL) {
            if(client->state == PARTING) {
                /* Client is closing on good terms */
                Hub_Logging_log(INFO, "Shutting down client");
                Hub_Net_sendPackedMessage(client, closing_packed);
                Hub_Net_removeClient(client);
            } else if(client->state == KICKING) {
                Hub_Logging_log(INFO, "Kicking client");
                if(client->kick_reason) {
                    kick_message->components[2] = client->kick_reason;
                } else {
                    kick_message->components[2] = default_kick;
                }

                Hub_Net_sendMessage(client, kick_message);
                Hub_Net_removeClient(client);
            } else if(client->state == DEAD) {
                /* Client has died or can't otherwise be nicely
                   disconnected. Just shutdown the socket and remove from the
                   clients list */
                Hub_Net_removeClient(client);
            } else {
                i++;
            }
        }
    }

    /* Cleanup */
    kick_message->components[2] = shutdown_kick;
    Hub_Net_broadcast(kick_message);
    while((client = List_get(clients, 0)) != NULL) {
        Hub_Net_removeClient(client);
    }

    List_destroy(clients);

    free(kick_message->components[0]);
    free(kick_message->components[1]);

    Comm_PackedMessage_destroy(closing_packed);
    Comm_Message_destroy(kick_message);

    pthread_mutex_lock(&mainloop_done_lock);
    mainloop_running = false;
    pthread_cond_broadcast(&mainloop_done);
    pthread_mutex_unlock(&mainloop_done_lock);
}

/** \} */
