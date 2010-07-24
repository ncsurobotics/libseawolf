
/* Seawolf libraries */
#include "seawolf.h"
#include "seawolf_hub.h"

#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>

static void Hub_catchSignal(int sig);
static void Hub_close(void);
static void Hub_usage(char* arg0);

void Hub_exitError(void) {
    Hub_Logging_log(INFO, "Terminating hub due to error condition");
    exit(EXIT_FAILURE);
}

bool Hub_fileExists(const char* file) {
    struct stat s;
    return stat(file, &s) != -1;
}

static void Hub_catchSignal(int sig) {
    /* Caught signal, exit and properly shut down */
    Hub_Logging_log(CRITICAL, "Signal caught! Shutting down...");
    Hub_exitError();
}

static void Hub_close(void) {
    Hub_Var_close();
    Hub_Logging_close();
    
    /* Util is part of the core libseawolf, does not require an _init() call,
       but *does* require a _close() call */
    Util_close();
}

static void Hub_usage(char* arg0) {
    printf("Usage: %s [-h] [-c conf]\n", arg0);
}

int main(int argc, char** argv) {
    int opt;
    char* conf_file = NULL;

    /* Parse arguments list */
    while((opt = getopt(argc, argv, ":hc:")) != -1) {
        switch(opt) {
        case 'h':
            Hub_usage(argv[0]);
            exit(EXIT_SUCCESS);
            break;
        case 'c':
            conf_file = optarg;
            break;
        case ':':
            fprintf(stderr, "Option '%c' requires an argument\n", optopt);
            Hub_usage(argv[0]);
            exit(EXIT_FAILURE);
        default:
            /* Invalid option */
            fprintf(stderr, "Invalid option '%c'\n", optopt);
            Hub_usage(argv[0]);
            exit(EXIT_FAILURE);
            break;
        }
    }
    
    /* Please *ignore* SIGPIPE. It will cause the program to close in the case
       of writing to a closed socket. We handle this ourselves. */
    signal(SIGPIPE, SIG_IGN);

    /* Catch common siginals and insure proper shutdown */
    signal(SIGINT, Hub_catchSignal);
    signal(SIGHUP, Hub_catchSignal);
    signal(SIGTERM, Hub_catchSignal);

    /* Use argument as configuration file */
    if(conf_file) {
        Hub_Config_loadConfig(conf_file);
    }

    /* Process configuration file */
    Hub_Config_processConfig();

    Hub_Var_init();
    Hub_Logging_init();

    /* Ensure shutdown during normal exit */
    atexit(Hub_close);
    
    /* Run the main network loop */
    Hub_Net_mainLoop();

    return 0;
}
