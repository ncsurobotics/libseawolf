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

static void Hub_Net_removeMarkedClosedClients(void);
static void Hub_Net_initServerSocket(void);

/* List of active clients */
static List* clients = NULL;
static List* closed_clients = NULL;

/** Server socket */
static int svr_sock = -1;

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
 * \brief Remove clients previously marked as closed
 *
 * Remove clients that have been marked closed with a call to
 * Hub_Net_markClientCloesd
 */
static void Hub_Net_removeMarkedClosedClients(void) {
    Hub_Client* client;

    while((client = List_remove(closed_clients, 0)) != NULL) {
        List_remove(clients, List_indexOf(clients, client));

        client->state = CLOSED;
        Hub_Client_clearFilters(client);
        shutdown(client->sock, SHUT_RDWR);
        close(client->sock);

        free(client);
    }
}

/**
 * \brief Mark a client as closed
 *
 * Mark a client as closed. It's resources will be released on the next call to
 * Hub_Net_removeMarkedClosedClients()
 *
 * \param client Mark the given client as closed
 */
void Hub_Net_markClientClosed(Hub_Client* client) {
    client->state = CLOSED;
    List_append(closed_clients, client);
}

void Hub_Net_init(void) {
    clients = List_new();
    closed_clients = List_new();
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

    /* Synchronize exit with mainLoop */
    pthread_mutex_lock(&mainloop_done_lock);

    if(mainloop_running) {
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
    }

    pthread_mutex_unlock(&mainloop_done_lock);

    if(clients) {
        /* Free the clients lists */
        List_destroy(clients);
        List_destroy(closed_clients);
    }
}

List* Hub_Net_getConnectedClients(void) {
    return clients;
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

    /** File descriptor mask passed to select() */
    fd_set fdset_mask_r;
    Comm_Message* client_message;
    Hub_Client* client;
    int client_count;
    int n, i; /* misc */

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
                perror("Error accepting new client");
            } else if(client_count >= MAX_CLIENTS) {
                Hub_Logging_log(ERROR, Util_format("Unable to accept new client connection! Maximum clients (%d) exceeded", MAX_CLIENTS));
                shutdown(client_new, SHUT_RDWR);
                close(client_new);
            } else {
                Hub_Logging_log(DEBUG, "Accepted new client connection");

                /* Set a timeout on receive operations to keep broken client
                   connections from deadlocking the hub */
                setsockopt(client_new, SOL_SOCKET, SO_RCVTIMEO, &client_timeout, sizeof(client_timeout));

                client = malloc(sizeof(Hub_Client));
                client->sock = client_new;
                client->state = UNAUTHENTICATED;
                client->name = NULL;
                client->filters = NULL;
                client->filters_n = 0;

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
                Hub_Process_process(client, client_message);

                /* Destroy client message */
                Comm_Message_destroy(client_message);
            }
        }

        /* Cleanup any clients that have been closed */
        Hub_Net_removeMarkedClosedClients();
    }

    /* Kick all still attached clients */
    for(i = 0; (client = List_get(clients, i)) != NULL; i++) {
        Hub_Client_kick(client, "Hub closing");
    }
    Hub_Net_removeMarkedClosedClients();

    pthread_mutex_lock(&mainloop_done_lock);
    mainloop_running = false;
    shutdown(svr_sock, SHUT_RDWR);
    close(svr_sock);

    pthread_cond_broadcast(&mainloop_done);
    pthread_mutex_unlock(&mainloop_done_lock);
}

/** \} */
