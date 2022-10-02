#pragma once

#include"Poller.h"
#include"Timestamp.h"
#include"Channel.h"


#include<unordered_map>
#include<vector>
#include<sys/epoll.h>
#include<memory>


class EpollPoller:public Poller{
public:
    EpollPoller(EventLoop*Loop);
    ~EpollPoller() override;


    //重写基类poller的抽象方法
    Timestamp poll(int timeoutMs,ChannelList *activeChannels)override;
    void updateChannel(Channel *Channel)override;
    void removeChannel(Channel *channel)override;

private:
    static const int kInitEventListsize=16;
    static const int MAXFDS = 100000;
    //填写活跃的连接
    void fillActiveChannels(int numEvents,ChannelList *activeChannels)const;
    //更新channel通道
    void update(int operation,Channel *channel);

    using EventList=std::vector<epoll_event>;

    int epollfd_;
    EventList events_;



};