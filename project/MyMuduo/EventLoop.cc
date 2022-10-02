#include"EventLoop.h"
#include"Logger.h"
#include"Poller.h"
#include"Channel.h"

#include<sys/eventfd.h>
#include<unistd.h>
#include<fcntl.h>
#include<errno.h>
#include<memory>

//防止一个线程创建多个eventloop
//全局变量 用来指向线程创建的eventloop对象
__thread EventLoop*t_loopInThisThread=nullptr;

const int kPollTimeMs=10000;//定义poller中epoll_wait()的超时时间

//全局函数 调用系统调用eventfd() 创建wakeupfd，用来唤醒subreactor去处理新来的channel
int creatEventfd(){
    int evtfd=::eventfd(0,EFD_NONBLOCK|EFD_CLOEXEC);//创建一个wakeupfd，用来唤醒阻塞的eventloop(subreactor)，交给其一个新连接的channel
    if(evtfd<0){
        LOG_ERROR("eventfd error :%d \n",errno);
    }
    return evtfd;
}


//-----------------------------------类内成员函数

EventLoop::EventLoop()
    :looping_(false)
    ,quit_(false)
    ,callingPendingFunctors_(false)
    ,threadId_(CurrentThread::tid())
    ,poller_(Poller::newDefaultPoller(this))
    ,wakeupFd_(creatEventfd())
    ,wakeupChannel_(new Channel(this,wakeupFd_))//每一个eventloop监听一个wakeupChannel_，
                                                //所以当subrecator阻塞时，mainreactor去notify(发8个字节的东西)其对应的wakeupfd，
                                                //就将其从epoll_wait()阻塞中唤醒

{
    LOG_DEBUG("EventLoop created %p in thread %d \n",this, threadId_);
    if(t_loopInThisThread){
        LOG_FATAL("Another EventLoop %p exits in this  thread %d",t_loopInThisThread,threadId_);
    }else{
        t_loopInThisThread=this;
    }

    //设置wakeupchannel中事件的类型以及发生事件后的回调操作
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead,this));
    //每一个EventLoop都将监听wakeupfd的EPOLLIN读事件
    wakeupChannel_->enableReading();

}


EventLoop::~EventLoop(){
    //对于eventloop类内资源进行回收 ----------->poller与wakeupchannel是智能指针，会自己调用析构函数进行堆区资源回收
    wakeupChannel_->disableAll();//将wakeupchannel从epoll中删除
    wakeupChannel_->remove();//将wakeupchannel从poller的channalmap中删除
    ::close(wakeupFd_);//wakeupfd是一个资源句柄，所以需要释放
    t_loopInThisThread=nullptr;

}



//调用底层poller的poll() 开启事件循环
void EventLoop::loop(){
    looping_=true;
    quit_=false;

    LOG_INFO("Eventloop %p start looping \n",this);

    while(!quit_){
        activeChannels_.clear();
        //监听两类fd，一种是client的fd:与客户端通信，一种是wakeupfd：mainreactor与subreactor通信的fd
        pollReturnTime_=poller_->poll(kPollTimeMs,&activeChannels_);
        for(Channel *channel:activeChannels_){
            //poller监听哪些channel发生了事件，然后上报给eventlop，
            //之后eventloop通过调用channel的handlevent函数来进行每个已发生事件revents的处理（调用事先在channel中注册的回调函数）
            channel->handleEvent(pollReturnTime_);//pollReturnTime_参数是为了外部处理业务逻辑去写回调函数时可以得到事件信息
        }
        //执行当前eventloop事件循环需要处理的回调操作
        /*
        IO线程 mainreactor 负责accept新的连接，然后生成fd并打包成channel给subreactor
        然后subreactor从poll中被唤醒，执行相应的channel中的回调处理函数
        然后在调用doPendingFunctors这个回调函数，即执行mainreactor在subreactor中注册派发的cb操作（事先写到subreactor的vector<Functors>pendingFunctors_中）
        其实就应该是mainreactor给subreactor分配新的channel要处理了，告诉subreactor如何处理(比如将新channel加入subreactor自身的poller的channel列表中，并且注册在其epoll中)
        */
        doPendingFunctors();
    }

    LOG_INFO("EventLoop %p stop looping. \n",this);
    looping_=false;

}


//退出事件循环 1:loop在自己的线程中调用quit，则loop肯定自身没有阻塞，然后将quit_设置为true，下次loop()中继续while(!quit_)不成立，会跳出，正常结束当前自己线程的loop
//           2.但是如果是在其他线程中，如在一个subreactor中调用了mainreactor的quit，要将mainreactor唤醒

void EventLoop::quit(){
    quit_=true;
    if(!isInloopThread()){
        wakeup();
    }
}



//让回调函数cb在当前loop中执行
void EventLoop::runInLoop(Functor cb){
    if(isInloopThread()){//在当前的loop线程中，执行cb
        cb();
    }else{//在非当前的loop线程中执行cb，就需要唤醒loop所在的线程，执行cb
        queueInLoop(cb);
    }
}

//把回调函数cb放入队列中，唤醒loop所在的线程，去执行cb
void EventLoop::queueInLoop(Functor cb){
    {
        //多个loop都可能调用当前loop的runinloop函数，让当前loop执行回调函数，所以vector存在并发访问，要加锁
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(cb);//push_back是拷贝构造，这里面是直接构造
    }
    //唤醒相应的，需要执行上面回调函数的loop线程
    //callingPendingFunctors_=true表明当前loop正在执行回调，但是其他loop又给当前loop增加了回调，所以当前loop执行完旧的回调后又回到循环，进入poll堵塞，
    //这时候此时的wakeup语句会给当前loop的wakeupfd写数据，从而是的poll()结束阻塞，继续运行
    if(!isInloopThread()||callingPendingFunctors_){
        wakeup();//唤醒loop所在的线程
    }
}

//mainreactor用来唤醒subreactor，即用来唤醒loop所在的线程
//向wakeupfd写一个数据,wakeupchannel就发生读事件，当前loop线程就会被唤醒
void EventLoop::wakeup(){
    uint64_t one=1;
    ssize_t n=write(wakeupFd_,&one,sizeof one);
    if(n!=sizeof one){
        LOG_ERROR("EventLoop::wakeup() writes %lu bytes instead of 8\n",n);
    }
}


//eventloop的方法->调用poller的方法
void EventLoop::updateChannel(Channel *channel){
    poller_->updateChannel(channel);
}
void EventLoop::removeChannel(Channel *channel){
    poller_->removeChannel(channel);
}
bool EventLoop::hasChannel(Channel *channel){
    return poller_->hasChannel(channel);
}


//执行回调函数
void EventLoop::doPendingFunctors(){
    std::vector<Functor>functors;
    callingPendingFunctors_=true;
    {
        std::unique_lock<std::mutex>lock(mutex_);
        functors.swap(pendingFunctors_);//这样的话哪怕我当前loop还没执行完要执行的回调，也不妨碍其他loop向当前vector中继续添加回调
    }
    for(const Functor &functor:functors){
        functor();//执行当前loop需要执行的回调操作
    }
    callingPendingFunctors_=false;
}




//wakeupfd的回调函数，当wakeupfd上发生可读事件eventloop会从epoll_wait()中返回，然后会调用其回调函数
void EventLoop::handleRead(){
    uint64_t one=1;
    ssize_t n=read(wakeupFd_,&one,sizeof one);
    if(n != sizeof one){
        LOG_ERROR("Eventloop::handleRead() reads %lu bytes instead of 8",n);
    }

}