#pragma once

#include"noncopyable.h"
#include"Timestamp.h"
#include"CurrentThread.h"

#include<functional>
#include<vector>
#include<atomic>
#include<memory>
#include<mutex>

class Channel;
class Poller;


//mainReactor只是监听新用户的连接，接收到连接后将其打包为一个channel轮询交给subReactor，工作线程上的一个reactor就是一个eventloop，然后执行具体的工作

//事件循环类，主要包含两大模块：channne poller（epoll的抽象）
class EventLoop:noncopyable{
public:
    using Functor=std::function<void()>;

    EventLoop();
    ~EventLoop();

    void loop();//开启事件循环

    void quit();//退出事件循环

    Timestamp pollReturnTime()const { return pollReturnTime_;}

    //让回调函数cb在当前loop中执行
    void runInLoop(Functor cb);
    //把回调函数cb放入队列中，唤醒loop所在的线程，去执行cb
    void queueInLoop(Functor cb);

    void wakeup();//mainreactor用来唤醒subreactor，即用来唤醒loop所在的线程

    //eventloop的方法->调用poller的方法
    void updateChannel(Channel *channel);
    void removeChannel(Channel *channel);
    bool hasChannel(Channel *channel);

    //用来标识eventloo对象是否在创建他的线程中
    //threadId:创建eventloop的线程id
    //tid():当前线程id
    bool isInloopThread()const {return threadId_==CurrentThread::tid();}

private:
    
    void handleRead();//wakeup用
    void doPendingFunctors();//执行回调函数

    using ChannelList=std::vector<Channel*>;


    /*
        硬件层面：cpu实现了原子操作(指令集：如CAS指令，TAS指令：见csdn收藏夹)来实现单cpu单核下的原子性，同时提供总线锁和缓存锁，保证了多cpu多核下的原子操作
        基于这些原子操作，可以实现悲观锁和乐观锁
        乐观锁：直接基于CAS指令，实现无需wait和wake的乐观锁，并在此基础上封装了atomic类
        悲观锁：基于CAS指令，实现了futex内部的atomic_inc和atomic_dec操作，然后加入wait和wake机制，实现了悲观锁，并在futex基础上封装了mutex互斥锁和信号量，基于mutex可以再实现条件变量

    */
    std::atomic_bool looping_;//原子操作，通过底层CAS实现
                
    std::atomic_bool quit_;//标识退出loop循环
    const pid_t threadId_;//记录当前loop所在线程的id   pid_t最终就是个int类型 
    Timestamp pollReturnTime_;//poller返回发生事件的channels的时间点
    std::unique_ptr<Poller>poller_;//通过智能指针动态管理资源，管理epoll -> RALL

    int wakeupFd_;//系统调用eventfd的返回值 当mainloop获取一个新用户的连接后，通过轮询算法将channel分配给subloop，此时subloop可能都在阻塞，可以通过该成员唤醒subloop处理channel
    std::unique_ptr<Channel>wakeupChannel_;//将wakeupfd以及其感兴趣的事件打包成为一个channel

    ChannelList activeChannels_;//eventloop中所管理的是所有发生事件的channel
    

    std::atomic_bool callingPendingFunctors_;//标识当前loop是否有需要执行的回调操作，有的话都在ector里面放着
    std::vector<Functor>pendingFunctors_;//存储loop需要执行的所有的回调操作
    std::mutex mutex_;//互斥锁，用来保护上面vector容器的线程安全操作

};