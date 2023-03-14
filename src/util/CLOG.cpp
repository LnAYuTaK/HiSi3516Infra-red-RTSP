/**
 * @file CLOG.h
 * @author LnAYuTaK (807874484@qq.com)
 * @brief 
 * @version 0.1
 * @date 2023-03-03
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#include "CLOG.h"
#include <stdarg.h>
#include <fstream>
#include <thread>
#include <sys/time.h>     
#include <time.h>     
#include <sstream>
#include <string.h>

thread_local char __timebuf[64] = {0x00};
CLOG  *CLOG::_log = nullptr;             //读写锁
CLOG::CLOG(bool bToFile , bool bToTerminal, string LogFileName)
                                :_ToFile(bToFile)
                                ,_ToTerminal(bToTerminal)
                                ,_LogFileName(LogFileName)
{ 
    _log = this;
}
/***********************************************************/
CLOG::~CLOG()
{

}
/***********************************************************/
CLOG* log(void)
{
  return CLOG::_log;
}
/***********************************************************/
void CLOG::SetLevel(CLOG_LEVEL nLevel)
{
  _LOGLevel = nLevel;
}
/***********************************************************/
char * CLOG::GetCurrentTime()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm *lt = localtime(&tv.tv_sec);
    snprintf(__timebuf, sizeof(__timebuf) - 1,
            "%d-%02d-%02d %02d:%02d:%02d.%03d", 
            lt->tm_year + 1900,
            lt->tm_mon + 1,
            lt->tm_mday,
            lt->tm_hour,
            lt->tm_min,
            lt->tm_sec,
            (int)(tv.tv_usec / 1000));    
    return __timebuf;
}
/***********************************************************/
unsigned long long  CLOG::GetCurrentThreadId()
{
  ostringstream oss;
  oss <<this_thread::get_id();
  string stid = oss.str();
  unsigned long long tid = stoull(stid);
  return tid;
}
/***********************************************************/
void CLOG::CLOGPrint(CLOG_LEVEL nLevel,const char* pcFunc, const int& line, const char* fmt, ...)
{
  log()->_WriteMtx.lock();
  char buffer[MX_LOG_BUFFER_SIZE];
  memset(buffer,0,MX_LOG_BUFFER_SIZE);
  char levelBuffer[32];
  memset(levelBuffer,0,32);
  switch (nLevel)
  {
  case CLOG_LEVEL_INFO:
    sprintf(levelBuffer,"\033[0m\033[1;32m%s\033[0m"," INFO ");
  break;
  case CLOG_LEVEL_WARNING:
    sprintf(levelBuffer,"\033[0m\033[1;33m%s\033[0m","WARING");
  break;
  case CLOG_LEVEL_ERROR:
    sprintf(levelBuffer,"\033[0m\033[1;31m%s\033[0m","ERROR ");
  break;
  default:
    sprintf(levelBuffer,"\033[0m\033[1;34m%s\033[0m","UNKNOW");
  break;
  }
  int n = sprintf(buffer,"[THREAD:%llu]" "[%s]" "[FUNCTION:%s] [LINE:%d] ",
                  log()->GetCurrentThreadId(),
                  levelBuffer,
                  pcFunc, 
                  line);
  va_list vap;
  va_start(vap, fmt);
  vsnprintf(buffer + n, MX_LOG_BUFFER_SIZE-n, fmt, vap);
  va_end(vap);
  // if(log()->_ToFile)
  // {
  //   ofstream fout(log()->_LogFileName,ios::app);
  //   fout<<buffer;
  //   fout<< "\0\n";
  //   fout.close();
  // }
  if(log()->_ToTerminal)
  {
    fprintf(stdout,"%s\n",buffer);
    fflush(stdout);
  }
  log()->_WriteMtx.unlock();
}
/***********************************************************/