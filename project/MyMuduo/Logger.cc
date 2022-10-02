#include"Logger.h"
#include"Timestamp.h"

#include<iostream>
//获取日志唯一的实例对象
//单例模式是为了保证全局只有一个该类的实例，且类自身提供一个访问该实例的方法，外界无法通过任何方式创建该类的实例
//故该类构造函数为私有，且提供的接口函数为static（不需要实例化对象即可调用），同时方法内生成static对象，是为了保证全局唯一，并且返回引用
Logger &Logger::instance(){
    //声明一个静态对象
    static Logger logger;
    return logger;
}
//设置日志级别
void Logger::setLogLevel(int level){
    loglevel_=level;
}
//写日志 格式：  [级别信息] time ： msg
void Logger::log(std::string msg){
    switch (loglevel_)
    {
    case INFO:
        std::cout<<"[INFO]";
        break;
    case ERROR:
        std::cout<<"[ERROR]";
        break;
    case FATAL:
        std::cout<<"[FATAL]";
        break;
    case DEBUG:
        std::cout<<"[DEBUG]";
        break;       
    default:
        break;
    }
    //打印时间和msg
    std::cout<<Timestamp::now().toString()<<":"<<msg<<std::endl;
}
