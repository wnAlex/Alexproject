#pragma once
#include"noncopyable.h"
#include"Timestamp.h"

#include<vector>
#include<unordered_map>

class Channel;
class EventLoop;


class Poller:noncopyable{
public:
    using ChannelList=std::vector<Channel*>;

    Poller(EventLoop*loop);
    virtual ~Poller();

    //给所有IO复用保留统一的接口
    virtual Timestamp poll(int timeoutMs,ChannelList *activeChannels)=0;//等价于eooll_wait
    virtual void updateChannel(Channel *channel)=0;//epoll_ctl
    virtual void removeChannel(Channel *channel)=0;//epoll_ctl

    //判断参数channel是否在当前的Poller中
    bool hasChannel(Channel*channel)const;

    //Eventloop可以通过该接口获取默认的IO复用的具体实现，是poll还是epoll
    static Poller*newDefaultPoller(EventLoop *loop);
protected:
    //map的key:sockfd,map的value:sockfd所属的channel通道 epoll返回fd，然后通过map可以快速找到fd对应的channel
    using ChannelMap=std::unordered_map<int,Channel*>;
    ChannelMap channels_;
private:
    EventLoop*ownerLoop_;//定义poller所属的事件循环EventLoop
};