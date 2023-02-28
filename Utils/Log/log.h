#ifndef MY_LOG_H
#define MY_LOG_H
#include <stdio.h> 
#include <string.h> 
#include <sys/time.h>
#include <pthread.h>

typedef enum {
    LOG_NONE,     
    LOG_ERROR,    
    LOG_WARN,     
    LOG_INFO,    
    LOG_DEBUG,    
    LOG_VERBOSE     
} log_level_t;

//LOG LEVEL
#define LOG_LOCAL_LEVEL         5

int   log_timestamp(void);
void log_write(log_level_t level, const char* tag, const char* format, ...) __attribute__ ((format (printf, 3, 4)));

#define LOG_FORMAT(letter, format)  LOG_COLOR_ ## letter #letter " (%d) %s: " format LOG_RESET_COLOR "\n"

#define LOGE( tag, format, ... )  if (LOG_LOCAL_LEVEL >= LOG_ERROR)   { log_write(LOG_ERROR,   tag, LOG_FORMAT(E, format), log_timestamp(), tag, ##__VA_ARGS__); }
#define LOGW( tag, format, ... )  if (LOG_LOCAL_LEVEL >= LOG_WARN)    { log_write(LOG_WARN,    tag, LOG_FORMAT(W, format), log_timestamp(), tag, ##__VA_ARGS__); }
#define LOGI( tag, format, ... )  if (LOG_LOCAL_LEVEL >= LOG_INFO)    { log_write(LOG_INFO,    tag, LOG_FORMAT(I, format), log_timestamp(), tag, ##__VA_ARGS__); }
#define LOGD( tag, format, ... )  if (LOG_LOCAL_LEVEL >= LOG_DEBUG)   { log_write(LOG_DEBUG,   tag, LOG_FORMAT(D, format), log_timestamp(), tag, ##__VA_ARGS__); }
#define LOGV( tag, format, ... )  if (LOG_LOCAL_LEVEL >= LOG_VERBOSE) { log_write(LOG_VERBOSE, tag, LOG_FORMAT(V, format), log_timestamp(), tag, ##__VA_ARGS__); }

char logfiledirname[256];
pthread_rwlock_t rwlock;
int logfileinit(int data_dirname);
int printlog(char *line);


#endif // MY_LOG