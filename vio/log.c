#include "log.h"


int logfileinit(int data_dirname){
    struct timeval now;
    gettimeofday(&now, NULL);
    sprintf(logfiledirname, "/sd/log/log_data%d_%d.txt", data_dirname ,now.tv_sec);

    int err = pthread_rwlock_init(&rwlock, NULL);
        if(err){
                printf("init thread_rwlock failed \n");
                return -1;
        }
    
    return 1;


}

int printlog(char *line)
{
  //FILE *fpout;
  //pthread_rwlock_wrlock(&rwlock);
  //fpout = fopen(logfiledirname, "a");
  //fprintf(fpout, "%s", line);
  //fclose(fpout);
  //pthread_rwlock_unlock(&rwlock);
  printf(line);
  return 1;
}

int logfileclose(){
    //销毁锁
    pthread_rwlock_destroy(&rwlock);
}
