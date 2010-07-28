
#include "seawolf.h"
#include "seawolf_hub.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>

#define TIME_BUFFER_SIZE 64

static bool initialized = false;
static FILE* log_file = NULL;
static int log_file_fd = STDOUT_FILENO;
static char time_buffer[TIME_BUFFER_SIZE];
static short min_log_level = DEBUG;
static bool replicate_stdout = false;

void Hub_Logging_init(void) {
    char* path;
    short tmp;

    /* Retrieve log level */
    tmp = Logging_getLevelFromName(Hub_Config_getOption("log_level"));
    if(tmp != -1) {
        min_log_level = tmp;
    }

    /* Replicate messages to standard output */
    replicate_stdout = Config_truth(Hub_Config_getOption("log_replicate_stdout"));

    path = strdup(Hub_Config_getOption("log_file"));
    Util_strip(path);

    if(strcmp(path, "") != 0) {
        log_file_fd = open(path, O_RDWR|O_SYNC|O_CREAT|O_APPEND, S_IRUSR|S_IWUSR);
        if(log_file_fd == -1) {
            Hub_Logging_log(ERROR, Util_format("Could not open log file: %s", strerror(errno)));
            log_file_fd = STDOUT_FILENO;
        }
    } else {
        Hub_Logging_log(INFO, "No log file specified. Using standard output");
    }

    log_file = fdopen(log_file_fd, "a");
    if(log_file == NULL) {
        Hub_Logging_log(ERROR, Util_format("Unable to associated log file descriptor with file handle: %s", strerror(errno)));
    }

    free(path);
    initialized = true;
}

void Hub_Logging_log(short log_level, char* msg) {
    if(log_level >= min_log_level) {
        Hub_Logging_logWithName("Hub", log_level, msg);
    }
}

void Hub_Logging_logWithName(char* app_name, short log_level, char* msg) {
    time_t t;

    time(&t);
    strftime(time_buffer, TIME_BUFFER_SIZE, "%H:%M:%S", localtime(&t));

    if(!initialized || (replicate_stdout && log_file_fd != STDOUT_FILENO)) {
        printf("[%s][%s][%s] %s\n", time_buffer, app_name, Logging_getLevelName(log_level), msg);
    }

    if(initialized) {
        fprintf(log_file, "[%s][%s][%s] %s\n", time_buffer, app_name, Logging_getLevelName(log_level), msg);
        fflush(log_file);
    }
}

void Hub_Logging_close(void) {
    if(log_file) {
        fflush(log_file);
        fclose(log_file);
        close(log_file_fd);
    }
}
