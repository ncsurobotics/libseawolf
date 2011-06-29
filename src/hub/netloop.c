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

/* If left uncommnented the hub will handle clients with threads. Otherwise
   clients requests will be processed by a single thread and select will be used
   to select clients ready for data transfer */
#define USE_THREADS

static int Hub_Net_removeMarkedClosedClients(void);
static void Hub_Net_initServerSocket(void);

/* List of active clients */
static List* clients = NULL;
static Queue* closed_clients = NULL;

/* Global clients lock */
static pthread_mutex_t global_clients_lock = PTHREAD_MUTEX_INITIALIZER;

static pthread_mutex_t remove_client_lock = PTHREAD_MUTEX_INITIALIZER;

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

#ifdef USE_THREADS
/** Task handle of thread that destroys clients */
static Task_Handle close_clients_thread;

/** Since clients are closed in a dedicated thread, the Queue_pop calls should block */
bool blocking_close_clients = true;
#else
bool blocking_close_clients = false;
#endif

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
static int Hub_Net_removeMarkedClosedClients(void) {
    Hub_Client* client;
    Hub_Var* subscription;

    /* NULL pushed to the queue after all clients have been disconnected
       during shutdown */
    while((client = Queue_pop(closed_clients, blocking_close_clients)) != NULL) {
        /* Immediately close the socket. The client can not longer generate requests */
        shutdown(client->sock, SHUT_RDWR);
        close(client->sock);
        
        /* Remove client from clients list */
        Hub_Net_acquireGlobalClientsLock();
        List_remove(clients, List_indexOf(clients, client));
        Hub_Net_releaseGlobalClientsLock();

#ifdef USE_THREADS
        /* Wait for the client thread to terminate */
        pthread_join(client->thread, NULL);
#endif
        
        /* With the client thread dead and the client removed from the client
           list the client is inaccessible at this point. Only threads which
           acquired a reference before the client was closed may still have
           access to it */

        /* Remove client variable subscriptions. Once this completes the var
           module has no access to this client. We use List_get instead of
           List_remove because the deleteSubscriber call does the remove for
           us */
        while((subscription = List_get(client->subscribed_vars, 0)) != NULL) {
            Hub_Var_deleteSubscriber(client, subscription->name);
        }
        
        /* Clear client filters */
        Hub_Client_clearFilters(client);
        
#ifdef USE_THREADS
        /* Wait for any thread which holds an in_use read lock on the client to
           complete. No more threads could possibly try to acquire a read lock
           so we're just waiting for existing locks to expire */
        pthread_rwlock_wrlock(&client->in_use);
#endif

        /* The client is completely removed and unused. Safe to free backing memory */
        free(client);
    }

    return 0;
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
    pthread_mutex_lock(&remove_client_lock);
    if(client->state != CLOSED) {
        client->state = CLOSED;
        Queue_append(closed_clients, client);
    }
    pthread_mutex_unlock(&remove_client_lock);
}

void Hub_Net_init(void) {
    clients = List_new();
    closed_clients = Queue_new();

}

#ifdef USE_THREADS
/**
 * \brief Wait for all client threads to die
 *
 * Wait for all client threads to shutdown and be destroyed
 */
static void Hub_Net_joinClientThreads(void) {
    Task_wait(close_clients_thread);
}
#endif

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
    /* Synchronize exit with mainLoop */
    pthread_mutex_lock(&mainloop_done_lock);

    if(mainloop_running) {
        /* Instruct mainLoop to terminate */
        Hub_Net_preClose();

#ifndef USE_THREADS
        /* Attempt to connect a socket to the address of the server socket which
           should cause the blocking select call to wake up */
        int tmp_sock = socket(AF_INET, SOCK_STREAM, 0);
        if(connect(tmp_sock, (struct sockaddr*) &svr_addr, sizeof(svr_addr))) {
            Hub_Logging_log(ERROR, "Unable to complete graceful shutdown!");
            Hub_exitError();
        }
#endif
        
        /* Now wait for Hub_mainLoop to terminate */
        while(mainloop_running) {
            pthread_cond_wait(&mainloop_done, &mainloop_done_lock);
        }

#ifndef USE_THREADS
        /* Close the temporary socket */
        shutdown(tmp_sock, SHUT_RDWR);
        close(tmp_sock);
#endif
    }

    pthread_mutex_unlock(&mainloop_done_lock);

    if(clients) {
        /* Free the clients lists */
        List_destroy(clients);
        Queue_destroy(closed_clients);
    }
}

/**
 * \brief Get connected clients
 *
 * Get a list of connected clients. Access to the list should be proteced by
 * calls to Hub_Net_acquireGlobalClientsLock and
 * Hub_Net_releaseGlobalClientsLock
 */
List* Hub_Net_getConnectedClients(void) {
    return clients;
}

/**
 * \brief Acquire the clients list lock
 *
 * Acquire a global lock on the clients list. This lock should only be held for
 * a short amount of time if necessary.
 */
void Hub_Net_acquireGlobalClientsLock(void) {
    pthread_mutex_lock(&global_clients_lock);
}

/**
 * \brief Release the clients list lock
 *
 * Release the global lock on the clients list. This lock should only be held
 * for a short amount of time if necessary.
 */
void Hub_Net_releaseGlobalClientsLock(void) {
    pthread_mutex_unlock(&global_clients_lock);
}

#ifdef USE_THREADS
/**
 * \brief Client connection thread
 *
 * Handle requests from a single client until the client closes
 *
 * \param _client Pointer to a Hub_Client structure
 * \return Always returns NULL
 */
static void* Hub_Net_clientThread(void* _client) {
    Hub_Client* client = (Hub_Client*) _client;
    Comm_Message* client_message;
 
    while(client->state != CLOSED) {
        /* Read message from the client  */
        client_message = Hub_Net_receiveMessage(client);

        if(client_message == NULL) {
            Hub_Net_markClientClosed(client);
            break;
        }

        /* Process message */
        Hub_Process_process(client, client_message);

        /* Destroy client message */
        Comm_Message_destroy(client_message);
    }

    return NULL;
}
#endif // #ifdef USE_THREADS

/**
 * \brief Attempt to accept a new client connection
 *
 * Attempt to accept a new client connection
 *
 * \param client_new New client socket
 */
static void Hub_Net_acceptClient(int client_new) {
#ifndef USE_THREADS
    /* Used to set the SO_RCVTIMEO (receive timeout) socket option on client
       sockets. 250 milliseconds */
    const struct timeval client_timeout = {.tv_sec = 0, .tv_usec = 250 * 1000};
#endif

    Hub_Client* client;

    if(List_getSize(clients) >= MAX_CLIENTS) {
        Hub_Logging_log(ERROR, Util_format("Unable to accept new client connection! Maximum clients (%d) exceeded", MAX_CLIENTS));
        shutdown(client_new, SHUT_RDWR);
        close(client_new);
        return;
    }

    Hub_Logging_log(DEBUG, "Accepted new client connection");

    /* Set a timeout on receive operations to keep broken client
       connections from deadlocking the hub */
#ifndef USE_THREADS
    setsockopt(client_new, SOL_SOCKET, SO_RCVTIMEO, &client_timeout, sizeof(client_timeout));
#endif

    client = Hub_Client_new(client_new);

    Hub_Net_acquireGlobalClientsLock();
    List_append(clients, client);
    Hub_Net_releaseGlobalClientsLock();

#ifdef USE_THREADS
    /* Start client thread */
    pthread_create(&client->thread, NULL, Hub_Net_clientThread, client);
#endif
}

/**
 * \brief Hub main loop
 *
 * Main loop which processes client requests and handles all client connections
 */
void Hub_Net_mainLoop(void) {
#ifndef USE_THREADS
    /** File descriptor mask passed to select() */
    fd_set fdset_mask_r;

    Comm_Message* client_message;
    int client_count, n;
#endif

    /* Temporary storage for new client connections until a Hub_Client structure
       can be allocated for them */
    int client_new = 0;

    /* Temporary variables */
    Hub_Client* client;
    int i;

    /* Create and ready the server socket */
    Hub_Net_initServerSocket();

    /* Begin accepting connections */
    Hub_Logging_log(INFO, "Accepting client connections");

    /* Main loop is now running */
    mainloop_running = true;

#ifdef USE_THREADS
    /* Spawn thread to remove clients after they are marked closed */
    close_clients_thread = Task_background(Hub_Net_removeMarkedClosedClients);
#endif

    /* Start sending/recieving messages */
    while(run_mainloop) {
#ifdef USE_THREADS
        client_new = accept(svr_sock, NULL, 0);

        if(run_mainloop == false ) {
            break;
        }

        if(client_new < 0) {
            Hub_Logging_log(ERROR, "Error accepting new client connection");
            continue;
        }

        Hub_Net_acceptClient(client_new);

#else

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
           wake up the select call which is why this additional check is placed
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
            Hub_Net_acceptClient(client_new);
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

        Hub_Net_removeMarkedClosedClients();
#endif
    }

    /* Kick all still attached clients */
    Hub_Net_acquireGlobalClientsLock();
    for(i = 0; (client = List_get(clients, i)) != NULL; i++) {
        Hub_Client_kick(client, "Hub closing");
    }
    Hub_Net_releaseGlobalClientsLock();

    /* Ensure removeMarkedClosedClients stops blocking to exit */
    Queue_append(closed_clients, NULL);

    /* Wait for all client threads to complete */
#ifdef USE_THREADS
    Hub_Net_joinClientThreads();
#else
    Hub_Net_removeMarkedClosedClients();
#endif

    /* Signal loop as ended */
    pthread_mutex_lock(&mainloop_done_lock);
    mainloop_running = false;
    shutdown(svr_sock, SHUT_RDWR);
    close(svr_sock);

    pthread_cond_broadcast(&mainloop_done);
    pthread_mutex_unlock(&mainloop_done_lock);
}
/** \} */
