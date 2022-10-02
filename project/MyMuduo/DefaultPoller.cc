#include"Poller.h"
#include"EPollPoller.h"

#include<stdlib.h>//getenv的头文件


//defaultpoller.cc属于一个公共的源文件，可以包含poller.h
//当然也可以包含epollpoller.h和pollpoller.h
//但poller.cc是epoll和poll的基类，其源文件包含其派生类的头文件是一种不规范的写法

Poller*Poller::newDefaultPoller(EventLoop *loop){
    if(::getenv("MUDUO_USE_POLL")){
        return nullptr;//生成poll的实例并返回
    }else{
        return new EpollPoller(loop);//生成epoll的实例并返回
    }
}
