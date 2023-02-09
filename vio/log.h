#ifndef MY_LOG_H
#define MY_LOG_H

#include <stdio.h> 
#include <string.h> 
#include <sys/time.h>
#include <pthread.h>



char logfiledirname[256];
pthread_rwlock_t rwlock;

int logfileinit(int data_dirname);
int printlog(char *line);

#endif // MY_LOG