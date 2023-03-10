/**
 * @file CLOG.h
 * @author LnAYuTaK (807874484@qq.com)
 * @brief 日志模块
 * @version 0.1
 * @date 2023-03-03
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#pragma once 
#include <string.h>
#include <iostream>
#include <mutex>
enum CLOG_LEVEL
{
    CLOG_LEVEL_INFO,
    CLOG_LEVEL_WARNING,
    CLOG_LEVEL_ERROR
};

#define MX_LOG_BUFFER_SIZE 256
using namespace std;
class CLOG
{
public:
    CLOG(bool bToFile, bool bTerminal,string LogFileName);
    ~CLOG();
    void SetLevel(CLOG_LEVEL nLevel);
    static void CLOGPrint(CLOG_LEVEL nLevel,const char* pcFunc, const int& line, const char* fmt, ...);
    static CLOG*  _log;
private:
    //禁止拷贝构造
    CLOG(const CLOG& rhs) = delete;
    CLOG& operator=(const CLOG& rhs) = delete;
    char* GetCurrentTime();
    unsigned long long GetCurrentThreadId();
private:
    string           _LogFileName;
    bool             _ToFile;              //是否允许写入文件
    bool             _ToTerminal;          //是否允许控制台
    CLOG_LEVEL       _LOGLevel;            //日志级别
    pthread_rwlock_t _RwLock;              //读写锁
    FILE *           _FilePtr;             //文件Ptr
    mutex            _WriteMtx;            //写锁
};
CLOG *  log();

#define LOG_INFO(fmt, args...)      CLOG::CLOGPrint(CLOG_LEVEL_INFO, __FUNCTION__ , __LINE__,fmt,##args)
#define LOG_WARNING(fmt,  args...)  CLOG::CLOGPrint(CLOG_LEVEL_WARNING,__FUNCTION__ , __LINE__,fmt,##args)
#define LOG_ERROR(fmt,  args...)    CLOG::CLOGPrint(CLOG_LEVEL_ERROR, __FUNCTION__ , __LINE__,fmt,##args)

