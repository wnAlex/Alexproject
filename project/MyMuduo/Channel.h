#pragma once
#include"noncopyable.h"
#include"Timestamp.h"


#include<sys/epoll.h>
#include<string>
#include<unordered_map>
#include<functional>
#include<memory>

class EventLoop;
class HttpData;

/*channel理解为通道，里面封装了socketfd以及其感兴趣的事件EPOLLIN,EPOLLOUT等,以及poller返回的具体事件*/
class Channel:private noncopyable{
public:
    using EventCallback = std::function<void()>;
    using ReadEventCallback = std::function<void(Timestamp)>;

    Channel(EventLoop* loop, int fd);
    ~Channel();

    //fd得到poller通知后，处理事件的函数->调用相应的回调方法
    void handleEvent(Timestamp receivetime);

    //设置回调函数对象
    void setReadCallback(ReadEventCallback cb){ readCallback_=std::move(cb);}
    void setWriteCallback(EventCallback cb){ writeCallback_ =std::move(cb);}
    void setCloseCallback(EventCallback cb){ closeCallback_=std::move(cb);}
    void setErrorCallback(EventCallback cb){ errorCallback_=std::move(cb);}

    
    //channel用弱智能指针来监听当前channel是否被remove，tie函数防止channel被手动remove掉时还在执行相应的回调操作
    void tie(const std::shared_ptr<void>&);

    int fd()const {return fd_;}
    int events()const{ return events_;}
    void  set_revents(int revt){revents_=revt;}//epoll监听到相应fd上发生的事件后，会在channel中进行设置


    //设置fd相应的事件状态
    void enableReading(){events_|=kReadEvent; update();}//设置当前fd对于读事件感兴趣，update负责将fd对应的改变更新到epoll中（通过epo_ctl）
    void disableReading(){events_&= ~kReadEvent;update();}
    void enableWriting(){events_|=kWriteEvent;update();}
    void disableWriting(){events_&= ~kWriteEvent;update();}
    void disableAll(){events_ = kNoneEvent;update();}

    //返回当前fd的事件状态
    bool isNoneEvent()const{return events_==kNoneEvent;}//用来获取当前fd是否注册过感兴趣的事件
    bool isWriting()const { return events_&kWriteEvent;}
    bool isReading()const { return events_&kReadEvent;}

    int index(){return index_;}
    void set_index(int idx){index_=idx;}

    EventLoop* ownerLoop(){return loop_;}//当前channel属于哪一个loop
    void remove();//删除channel



private:

    void update();
    void handleEventWithGuard(Timestamp receiveTime);

    //描述sockfd的状态，所感兴趣的事件是啥
    static const int kNoneEvent;
    static const int kReadEvent;
    static const int kWriteEvent;

    EventLoop*loop_;//事件循环
    const int fd_;//poller监听的对象
    int events_;//注册fd感兴趣的事件
    int revents_;//poller返回的具体在fd上发生的事件
    int index_;//表示channel在poller中的状态，与kNew,kAdded,kDeleted对比
    std::weak_ptr<void>tie_;//防止remove掉channel所属的Tcpconnection对象时，其他线程还在调用当前Tcpconnection对象传给channel的回调函数 tie_进行跨线程的对象生存状态的监听
    bool tied_;

    //因为channel通道里面可以获知fd最终发生的事件revents，所以由他负责调用最终的回调函数
    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
    
};

typedef std::shared_ptr<Channel> SP_Channel;