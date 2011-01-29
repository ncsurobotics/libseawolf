
#ifndef __SEAWOLF_HUB_INCLUDE_H
#define __SEAWOLF_HUB_INCLUDE_H

#include "seawolf/mem_pool.h"

#include <stdbool.h>

#define MAX_CLIENTS (FD_SETSIZE - 1)
#define MAX_ERRORS 4

/**
 * Client state
 */
typedef enum {
    /**
     * State is unknown or unset
     */
    UNKNOWN,

    /**
     * Client is connected, but unauthenticated
     */
    UNAUTHENTICATED,

    /**
     * Client is authenticated (full connected)
     */
    CONNECTED,

    /**
     * Client connection is closed
     */
    CLOSED
} Hub_Client_State;

/**
 * Represents a connected client
 */
typedef struct {
    /**
     * Client socket
     */
    int sock;

    /**
     * Current state of the client
     */
    Hub_Client_State state;

    /**
     * Client name
     */
    char* name;

    /**
     * Notify filters
     */
    char** filters;
    
    /**
     * Number of filters
     */
    int filters_n;
} Hub_Client;

/**
 * Internal representation of a variable
 */
typedef struct {
    /**
     * Variable name
     */
    char* name;

    /**
     * Current variable value
     */
    double value;

    /**
     * Default value of the variable as given by the definitions file
     */
    double default_value;

    /**
     * Persistent variable flag
     */
    bool persistent;

    /**
     * Readonly variable flag
     */
    bool readonly;
} Hub_Var;

void Hub_exit(void);
void Hub_exitError(void);
bool Hub_fileExists(const char* file);
int Hub_Process_process(Hub_Client* client, Comm_Message* message);

Comm_Message* Hub_Net_receiveMessage(Hub_Client* client);
int Hub_Net_sendMessage(Hub_Client* client, Comm_Message* message);
void Hub_Net_broadcastMessage(Comm_Message* message);

void Hub_Client_kick(Hub_Client* client, char* reason);
void Hub_Client_close(Hub_Client* client);

void Hub_Net_init(void);
void Hub_Net_preClose(void);
void Hub_Net_close(void);
List* Hub_Net_getConnectedClients(void);
void Hub_Net_markClientClosed(Hub_Client* client);
void Hub_Net_mainLoop(void);

void Hub_Config_init(void);
void Hub_Config_loadConfig(const char* filename);
const char* Hub_Config_getOption(const char* config_key);
void Hub_Config_close(void);

void Hub_Var_init(void);
Hub_Var* Hub_Var_get(const char* name);
int Hub_Var_setValue(const char* name, double value);
void Hub_Var_close(void);

void Hub_Logging_init(void);
void Hub_Logging_log(short log_level, char* msg);
void Hub_Logging_logWithName(char* app_name, short log_level, char* msg);
void Hub_Logging_close(void);

#endif // #ifndef __SEAWOLF_HUB_INCLUDE_H
