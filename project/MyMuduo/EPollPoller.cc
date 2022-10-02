#include"EPollPoller.h"
#include"Logger.h"
#include"Channel.h"

#include<unistd.h>
#include<errno.h>
#include<strings.h>

//channel 未添加到poller中
const int kNew=-1;
//channel 已添加到poller中
const int kAdded=1;
//chanenl 从epoll中删除
const int kDeleted=2;

EpollPoller::EpollPoller(EventLoop*Loop)
    :Poller(Loop)
    ,epollfd_(::epoll_create1(EPOLL_CLOEXEC))
    ,events_(kInitEventListsize)//vector<epoll_event>
{
    if(epollfd_<0){
        LOG_FATAL("epoll_creat error:%d\n",errno);
    }
}


EpollPoller::~EpollPoller(){
    ::close(epollfd_);
}

//Eventloop调用poll()->poller调用->poll(), 通过这个函数将epoll_wait返回的真正发生事件的channel赋值给Eventloop调用这个函数时传入的实参activechannels中
//Eventloop拿到后，应该会调用channel里面注册的回调函数去做相应的操作
//两个功能：1.向Eeventloop返回发生事件的channel
//        2.更改发生事件的channel的revents参数  
Timestamp EpollPoller::poll(int timeoutMs,ChannelList *activeChannels){
    //poll的调用非常频繁，应该用LOG_DEBUG输出日志更为合理，防止过量日志输出
    LOG_INFO("func=%s => fd total count:%lu\n",__FUNCTION__,channels_.size());

    int numEvents=::epoll_wait(epollfd_,&*events_.begin(),static_cast<int>(events_.size()),timeoutMs);
    int saveError=errno;//防止多个线程的poll都对全局变量errno同时修改
    Timestamp now(Timestamp::now());

    if(numEvents>0){
        LOG_INFO("%d events happened \n",numEvents);
        fillActiveChannels(numEvents,activeChannels);//完成两个功能的主要函数
        if(numEvents==events_.size()){
            events_.resize(events_.size()*2);
        }
    }else if(numEvents==0){
        LOG_DEBUG("%s timeout \n",__FUNCTION__);
    }else{
        //说明epll_wait函数出现了错误退出的情况，此时记录一下errno的值
        //如果是因为中断导致退出，则不用管，继续运行就好
        //如果不是因为中断退出，则说明发生了错误，需要写日志输出，将saveerror的值写回errno，然后日志函数会去访问errno作输出（我们自己写的日志函数没有访问errno）
        if(saveError!=EINTR){
            errno=saveError;
            LOG_ERROR("EPollPoller::poll() err");
        }
    }
    return now;

}
/*
                  EventLoop
    ChannelLists            Poller
                            Channelmap <fd,channel*>
*/
//channel:update remove -> Eventloop:updateChannel removeChannel ->poller:updateChannel removeChannel
void EpollPoller::updateChannel(Channel *channel){
    const int index=channel->index();
    LOG_INFO("func=%s =>fd=%d events=%d index=%d \n",__FUNCTION__,channel->fd(),channel->events(),index);
    if(index==kNew||index==kDeleted){
        //这里面的kDeleted指的是从epoll中删除，但还在poller的channelmap中
        int fd=channel->fd();
        if(index==kNew){
            channels_[fd]=channel;
        }
        channel->set_index(kAdded);
        update(EPOLL_CTL_ADD,channel);
    }else{//channel已经在poller的哈希表中注册过了
        int fd=channel->fd();
        if(channel->isNoneEvent()){
            update(EPOLL_CTL_DEL,channel);
            channel->set_index(kDeleted);
        }else{
            update(EPOLL_CTL_MOD,channel);
        }
    }

}
//remove->removechannel 是用来将channel从channelmap和epoll中全删除
void EpollPoller::removeChannel(Channel *channel){
    int fd=channel->fd();
    int index=channel->index();
    LOG_INFO("func=%s =>fd=%d  \n",__FUNCTION__,channel->fd());
    channels_.erase(fd);
    if(index==kAdded){
        update(EPOLL_CTL_DEL,channel);
    }
    channel->set_index(kNew);
}


//填写活跃的连接
//两个功能：1.向Eeventloop返回发生事件的channel
//        2.更改发生事件的channel的revents参数  
void EpollPoller::fillActiveChannels(int numEvents,ChannelList *activeChannels)const 
{
    for(int i=0;i<numEvents;i++){
        Channel*channel=static_cast<Channel*>(events_[i].data.ptr);
        channel->set_revents(events_[i].events);
        activeChannels->push_back(channel);
    }
}
//更新channel通道
void EpollPoller::update(int operation,Channel *channel){
    epoll_event event;
    bzero(&event,sizeof event);
    event.events=channel->events();
    event.data.ptr=channel;
    int fd= channel->fd();
    if(::epoll_ctl(epollfd_,operation,fd,&event)<0){
        if(operation==EPOLL_CTL_DEL){
            LOG_ERROR("epoll_ctl del error:%d\n",errno);
        }else{
            LOG_FATAL("epoll_ctl add/mod error:%d\n",errno);
        }
    }
}