
/* Seawolf libraries */
#include "seawolf.h"
#include "seawolf_hub.h"

#include <signal.h>
#include <unistd.h>

static void Hub_catchSignal(int sig);
static void Hub_close(void);
static void Hub_usage(char* arg0);

void Hub_exitError(void) {
    Hub_Logging_log(INFO, "Terminating hub due to error condition");
    exit(1);
}

static void Hub_catchSignal(int sig) {
    /* Caught signal, exit and properly shut down */
    Hub_Logging_log(CRITICAL, "Signal caught! Shutting down...");
    Hub_exitError();
}

static void Hub_close(void) {
    Hub_Var_close();
    Hub_DB_close();
    Hub_Logging_close();
    
    /* Util is part of the core libseawolf, does not require an _init() call,
       but *does* require a _close() call */
    Util_close();
}

static void Hub_usage(char* arg0) {
    printf("Usage: %s [-h] [-f db]\n", arg0);
}

int main(int argc, char** argv) {
    int opt;
    char* db_file = NULL;

    /* Parse arguments list */
    while((opt = getopt(argc, argv, ":hf:")) != -1) {
        switch(opt) {
        case 'h':
            Hub_usage(argv[0]);
            exit(EXIT_SUCCESS);
            break;
        case 'f':
            db_file = optarg;
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

    /* Use argument as database file */
    if(!db_file) {
        db_file = DEFAULT_DB;
    }

    /* Set the DB file */
    if(Hub_DB_setFile(db_file)) {
        perror(Util_format("Unable to use database file %s", db_file));
        exit(EXIT_FAILURE);
    }
    
    /* Initialize subcomponents */
    if(Hub_DB_init()) {
        perror(Util_format("Failed to initialize database %s", db_file));
        exit(EXIT_FAILURE);
    }

    Hub_Var_init();
    Hub_Logging_init();

    /* Ensure shutdown during normal exit */
    atexit(Hub_close);
    
    /* Run the main network loop */
    Hub_Net_mainLoop();

    return 0;
}
