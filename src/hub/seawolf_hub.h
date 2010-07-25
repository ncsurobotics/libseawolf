
#ifndef __SEAWOLF_HUB_INCLUDE_H
#define __SEAWOLF_HUB_INCLUDE_H

#define MAX_CLIENTS 128
#define MAX_ERRORS 8

#define RESPOND_TO_NONE 0
#define RESPOND_TO_SENDER 1
#define RESPOND_TO_ALL 2
#define SHUTDOWN_SENDER 3

struct Hub_Var_s {
    char* name;
    double value;
    double default_value;
    bool persistent;
    bool readonly;
};

typedef struct Hub_Var_s Hub_Var;

void Hub_exitError(void);
bool Hub_fileExists(const char* file);
void Hub_Net_mainLoop(void);
int Hub_Process_process(Comm_Message* message, Comm_Message** response, bool* authenticated);

void Hub_Config_loadConfig(const char* filename);
void Hub_Config_processConfig(void);
char* Hub_Config_getOption(const char* config_key);
void Hub_Config_close(void);

void Hub_Var_init(void);
Hub_Var* Hub_Var_get(const char* name);
int Hub_Var_set(const char* name, double value);
void Hub_Var_close(void);

void Hub_Logging_init(void);
void Hub_Logging_log(short log_level, char* msg);
void Hub_Logging_logWithName(char* app_name, short log_level, char* msg);
void Hub_Logging_close(void);

#endif // #ifndef __SEAWOLF_HUB_INCLUDE_H
