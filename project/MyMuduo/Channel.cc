#include"Channel.h"
#include<sys/epoll.h>
#include"EventLoop.h"
#include"Logger.h"
const int Channel::kNoneEvent=0;
const int Channel::kReadEvent=EPOLLIN|EPOLLPRI;
const int Channel::kWriteEvent=EPOLLOUT;

Channel::Channel(EventLoop*loop,int fd)
    :loop_(loop)
    ,fd_(fd)
    ,events_(0)
    ,revents_(0)
    ,index_(-1)
    ,tied_(false)
{
}

Channel::~Channel(){}

//channel的tie方法何时调用过->在Tcpconnection::connectEstablished()
void Channel::tie(const std::shared_ptr<void>&obj){
    tie_=obj;
    tied_=true;
}

//当改变channel所表示fd的events事件后，update负责在poller里面更改相应的事件 
//channel和poller是两个模块，需要通过eventloop来完成注册事件
void Channel::update(){
    //通过channel所属的eventloop，调用poller的相应方法完成注册
     
    loop_->updateChannel(this);
}

//在channel所属的eventloop中，把当前的channel删除掉
void Channel::remove(){
    
    loop_->removeChannel(this);
}

void Channel::handleEvent(Timestamp receivetime){
    if(tied_){
        std::shared_ptr<void>guard=tie_.lock(); //给tie_提升为强智能指针
        if(guard){
            handleEventWithGuard(receivetime);
        }
    }else{
        handleEventWithGuard(receivetime);
    }
}

//根据poller返回的具体事件来执行具体的回调操作
void Channel::handleEventWithGuard(Timestamp receiveTime){

    LOG_INFO("channel handleEvent revents:%d\n",revents_);

    if((revents_&EPOLLHUP)&&!(revents_&EPOLLIN)){
        if(closeCallback_){
            closeCallback_();
        }
    }
    if(revents_&EPOLLERR){
        if(errorCallback_){
            errorCallback_();
        }
    }
    if(revents_&EPOLLIN){
        if(readCallback_){
            readCallback_(receiveTime);
        }
    }
    if(revents_&EPOLLOUT){
        if(writeCallback_){
            writeCallback_();
        }
    }

}