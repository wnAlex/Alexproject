#pragma once

#include<string>

#include"noncopyable.h"

//...表示可变参数列表，__VA_ARGS__在预处理中，会被实际的参数集（实参列表）所替换
#define LOG_INFO(LogmsgFormat,...)\
    do\
    {\
        Logger &logger=Logger::instance();\
        logger.setLogLevel(INFO);\
        char buf[1024]={0};\
        snprintf(buf,1024,LogmsgFormat,##__VA_ARGS__);\
        logger.log(buf);\
    }while(0)
#define LOG_ERROR(LogmsgFormat,...)\
    do\
    {\
        Logger &logger=Logger::instance();\
        logger.setLogLevel(ERROR);\
        char buf[1024]={0};\
        snprintf(buf,1024,LogmsgFormat,##__VA_ARGS__);\
        logger.log(buf);\
    }while(0)
#define LOG_FATAL(LogmsgFormat,...)\
    do\
    {\
        Logger &logger=Logger::instance();\
        logger.setLogLevel(FATAL);\
        char buf[1024]={0};\
        snprintf(buf,1024,LogmsgFormat,##__VA_ARGS__);\
        logger.log(buf);\
        exit(-1);\
    }while(0)

#ifdef MUDEBUG
#define LOG_DEBUG(LogmsgFormat,...)\
    do\
    {\
        Logger &logger=Logger::instance();\
        logger.setLogLevel(DEBUG);\
        char buf[1024]={0};\
        snprintf(buf,1024,LogmsgFormat,##__VA_ARGS__);\
        logger.log(buf);\
    }while(0)
#else
    #define LOG_DEBUG(LogmsgFormat,...)
#endif



//定义日志的级别 
enum LogLevel
{
    INFO, //普通信息
    ERROR,//错误信息
    FATAL,//毁灭性信息
    DEBUG,//调试信息
};

class Logger:noncopyable{
public:
    //获取日志唯一的实例对象
    static Logger &instance();
    //设置日志级别
    void setLogLevel(int level);
    //写日志
    void log(std::string msg);
private:
    int loglevel_;
    Logger(){}
};