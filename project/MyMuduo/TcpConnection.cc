#include"TcpConnection.h"
#include"Logger.h"
#include"Socket.h"
#include"Channel.h"
#include"EventLoop.h"

#include<functional>
#include<errno.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<strings.h>
#include<netinet/tcp.h>
#include<string>

static EventLoop* CheckLoopNotNull(EventLoop* loop) {
	if (loop == nullptr) {
		LOG_FATAL("%s:%s:%d TcpConnection is null \n", __FILE__, __FUNCTION__, __LINE__);
	}
	return loop;
}




TcpConnection::TcpConnection(EventLoop *loop,const std::string &nameArg,int sockfd,const InetAddress&localAddr,const InetAddress &peerAddr)
    :loop_(CheckLoopNotNull(loop))
    ,name_(nameArg)
    ,state_(kConnecting)
    ,reading_(true)
    ,socket_(new Socket(sockfd))
    ,channel_(new Channel(loop,sockfd))
    ,localAddr_(localAddr)
    ,peerAddr_(peerAddr)
    ,highWaterMark_(64*1024*1024)//64M
{
    //给channel设置相应的回调函数，poler给channel通知感兴趣的事件发生了，channel会回调相应的操作函数
    channel_->setReadCallback(
        /*
            此处需要将当前TcpConnection对象的指针this传入函数对象中，当在channel中调用这个回调函数的时候
            可能当前的TcpConnection对象已经被析构了，所以传入this指针是不安全的，应该传入一个shared_ptr
            但是直接传入shared_ptr<>(this)会与Tcpserver中创建的shared_ptr<>(new TcpConnection)有着不同的引用计数
            即两个智能指针不共享引用计数会造成当前TcpConnection对象的多次释放，所以需要传入shared_from_this()
            这个函数能通过弱智能指针的提升来避免调用强智能指针的构造函数，从而与Tcpserver中的强智能指针共享一个引用计数
            这样的话当前的函数对象中会保留当前TcpConnection对象的强智能指针，并且传入channel中由channel的函数对象属性保留下来
            故channel中相当于有一个当前TcpConnection对象的强智能指针，后续当前线程析构了TcpConnection对象后，TcpConnection对象也不会直接释放
            会等到channel中的函数对象执行完释放后TcpConnection对象才会真正释放。
            这里面没有传入shared_from_this()是因为channel中有一个弱智能指针来监听当前对象(在TcpConnection：：connectEstablished中设置的)
            故可以在channel中通过弱智能指针的提升来判断当前TcpConnection对象是否析构从而决定是否调用当前对象中的回调函数
            shared_from_this()相当于直接让使用者取消了这种顾虑。两种办法都可以解决问题
        */
        std::bind(&TcpConnection::handleRead,this,std::placeholders::_1)
    );

    channel_->setWriteCallback(
        std::bind(&TcpConnection::handleWrite,this)
    );

    channel_->setCloseCallback(
        std::bind(&TcpConnection::handleClose,this)
    );

    channel_->setErrorCallback(
        std::bind(&TcpConnection::handleError,this)
    );

    LOG_INFO("TcpConnection::ctor[%s] at fd=%d\n",name_.c_str(),sockfd);
    socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection(){
    LOG_INFO("TcpConnection::dtor[%s] at fd=%d state=%d \n",name_.c_str(),channel_->fd(),(int)state_); 
}


void TcpConnection::send(const std::string &buf){
    if(state_==kConnected){
        if(loop_->isInloopThread()){
            sendInLoop(buf.c_str(),buf.size());
        }else{
            //如果当前loop不是TcpConnection所属的loop,比如在mainloop中调用send(),
            //需要将TcpConnection对应的线程去唤醒，去执行属于他的TcpConnection的send函数
            //不能在主线程去执行属于子线程的TcpConnection对象的函数，这样会让主线程无法一心一意来监听连接
            loop_->runInLoop(std::bind(&TcpConnection::sendInLoop,shared_from_this(),buf.c_str(),buf.size()));
        }
    }
}

//发送数据
//应用写得快，而内核发送数据慢，因此需要一个缓冲区，将待发送数据写入缓冲区，而且设置了水位回调，防止发的太快
void TcpConnection::sendInLoop(const void *data,size_t len){
    ssize_t nwrote=0;
    size_t remaining=len;//剩余需要写入缓冲区的数据
    bool faultError =false;

    //之前调用过shutdown函数关闭了当前TcpConnection，不能在进行发送了
    if(state_==kDisconnected)
    {
        LOG_ERROR("disconnected,give up writing");
        return;
    }

    //表示channel_第一次开始写数据，且缓冲区没有待发送的数据
    if(!channel_->isWriting()&&outputBuffer_.readableBytes()==0){
        nwrote=::write(channel_->fd(),data,len);
        if(nwrote>0){
            remaining=len-nwrote;
            //remaining==0表示不需要缓冲区暂时存储数据，直接一次性全放到内核fd发送缓冲区中
            //既然在这里数据全部一次性发送完成，就不用再给channel设置EPOLLOUT事件了，就不会再去执行handlewrite方法了
            if(remaining==0&&writeCompleteCallback_){
                loop_->queueInLoop(std::bind(writeCompleteCallback_,shared_from_this()));
            }
        }else{//nwrote<=0;
            nwrote=0;
            if(errno!=EWOULDBLOCK)//正常的无数据非阻塞返回
            {
                LOG_ERROR("Tcpconnection sendInloop");
                if(errno==EPIPE||errno==ECONNRESET)//接收到对端发出的socket重置信号
                {
                    faultError=true;
                }
            }
        }
    }

    //这时说明一次的write()调用并未将应用数据通过sockfd全部发给客户端，剩余的数据需要放到缓冲区中
    //然后给channel注册EPOLLOUT事件，poller监听到fd发送缓冲区为空便会返回，调用channel中的writecallback_;
    //也就是调用外界注册的handlewrite，继续从buf中读取数据放到fd发送缓冲区进行发送，
    //直至全部发完（只要未调用disablewriting，poller就会一直监听，一直发送）
    if(!faultError&&remaining>0){
        //目前发送缓冲区剩余的待发送数据的长度
        size_t oldLen=outputBuffer_.readableBytes();
        if(oldLen+remaining>=highWaterMark_
            &&oldLen<highWaterMark_
            &&highWaterMarkCallback_)
        {   
            //如果待发送的所有数据总和达到水位线64M，就调用回调
            loop_->queueInLoop(std::bind(highWaterMarkCallback_,shared_from_this(),oldLen+remaining));//水位线默认是64M
        }

        //将剩余的数据添加到缓冲区中
        outputBuffer_.append((char*)data+nwrote,remaining);
        if(!channel_->isWriting()){
            channel_->enableWriting();//这里一定要注册channel的写事件，否则poller不会给channel通知fd发送缓冲区可写，即通知EPOLLOUT
        }

    }


}

//会将当前连接设置为kDisconnecting状态--->由用户调用
/*
    用户调用shutdown()-->shutdownInLoop()-->shutdownWrite()-->触发socket的EPOLLHUP事件
    --->channel_调用closeCallback_--->也就是调用TcpConnection传入的handleclose()
*/
void TcpConnection::shutdown(){
    if(state_==kConnected){
        setState(kDisconnecting);
        loop_->runInLoop(std::bind(&TcpConnection::shutdownInLoop,this));//此处不是回调函数，就是正常调用函数，不需要考虑当前对象被析构
    }
}

void TcpConnection::shutdownInLoop(){
    if(!channel_->isWriting())//说明outputBuffer中的数据已经全部发送完成
    {
        socket_->shutdownWrite();//关闭当前socket写端，会触发当前socket的EPOLLHUP事件(不需要向epoll注册的事件，epoll本身就给所有fd注册过了)
    }
}



//连接建立 ->在Tcpserver中调用
void TcpConnection::connectEstablished(){
    setState(kConnected);//最开始初始化state_是kconnecting

    //tie中用一个弱智能指针记录TcpConnection对象，
    //在chanenl中调用回调前通过提升弱智能指针来查看TcpConnection对象是否被释放(因为TcpConnection对象是暴露给用户的)
    //如果被释放了就不进行回调操作了
    channel_->tie(shared_from_this());

    channel_->enableReading();//向poller注册channel的读事件

    connectionCallback_(shared_from_this());//有新连接建立了，执行回调(就是用户传入的OnConnection())（在handleclose()也会调用）
}

//连接销毁 ->在Tcpserver中调用
void TcpConnection::connectDestroyed(){

    //这个if是为了配合在~Tcpserver()中调用（从handleclose进来的调用都已经做过if中的动作了）
    if(state_==kConnected){
        setState(kDisconnected);
        channel_->disableAll();//将channel所有感兴趣的事件，从poller中delete掉
        connectionCallback_(shared_from_this());
    }
    channel_->remove();//把channel从poller中删除
}




//当sockfd上有数据到来会调用handleRead()这个回调函数，将fd缓冲区的数据读到inputbuffer中，
//然后再去调用messageCallback_()这个回调函数，去处理缓冲区的数据
void TcpConnection::handleRead(Timestamp receiveTime){
    int savedErrno=0;
    ssize_t n=inputBuffer_.readFd(channel_->fd(),&savedErrno);
    if(n>0){
        //shared_from_this()就是指向当前TcpConnection对象的智能指针(用智能指针是防止messageCallback_()调用时当前TcpConnection对象已经被析构了)
        //传这个参数是为了后续利用当前TcpConnection对象继续发送数据
        //TcpConnection对象中有连接的相关信息(如peerAddr,localAddr)
        /*
            主线程封装一个TcpConnection对象（开辟在堆区，各线程共享），然后将对象交给子线程
            子线程会调用该对象的成员方法（或者调用某个需要传入该对象自身作为参数的成员方法），如果在子线程调用该对象成员方法前，该对象在主线程中被从堆区析构了
            那么调用方法就会出错，因此需要进行对象的生存状态检测，可以通过通过传该对象的弱智能指针来监听对象生存状态
            当子线程需要调用方法时，先通过弱指针升级为强指针来判断对象状态，如果对象已经在主线程被析构了，就不再调用其方法（或将其当作参数）
            所以channel中有一个弱智能指针记录当前的Tcpconnection对象
            此处shared_from_this作用：和send以及sendinloop中的不一样（可以看当前类构造函数中的说明）
                messageCallback_需要传入当前Tcpconnection的强智能指针，但是在Tcpserver中已经建立了一个当前Tcpconnection的强智能指针
                此处如果再次传入this会再次调用当前Tcpconnection的构造函数而不是拷贝构造，
                会造成同一个this指针构造了两个有不同引用计数的(且都为1)而不是相同引用计数且为2的强智能指针
                故需要引入shared_from_this()来保证不再次调用Tcpconnection的构造函数，只是起到这个作用
        */
        messageCallback_(shared_from_this(),&inputBuffer_,receiveTime);//就是用户传入的onMessage()
    }
    else if(n==0){//表明客户端断开连接
        handleClose();
    }else{//出错
        errno=savedErrno;
        LOG_ERROR("TcpConnection::handleRead");
        handleError();
    }
}

void TcpConnection::handleWrite(){
    if(channel_->isWriting()){
        int savedErrno=0;
        ssize_t n=outputBuffer_.writeFd(channel_->fd(),&savedErrno);
        if(n>0){
            outputBuffer_.retrieve(n);
            if(outputBuffer_.readableBytes()==0){//数据从缓冲区向fd写完了
                channel_->disableWriting();//用户buf缓冲区数据全部发送完了，就不需要让poller继续监听fd的可写事件了
                if(writeCompleteCallback_){
                    //queueInLoop唤醒对应的线程执行回调，但这里面肯定是在subloop自己的线程中执行的，可以直接写回调writeCompleteCallback_()
                    loop_->queueInLoop(std::bind(writeCompleteCallback_,shared_from_this()));
                }
                if(state_==kDisconnecting){//说明在数据发送过程中有个地方调用了shutdown()将state_置为kDisconnecting
                    shutdownInLoop();//在当前loop中将当前TcpConnection对象删除
                }
            }

        }else{
            LOG_ERROR("Tcpconnection::handlewrite");
        }
    }else{
        LOG_ERROR("TcpConnection fd=%d is down,no more writing \n",channel_->fd());
    }
}

//poller通知channel调用closecallback，就是调用handleClose(),最终也会调用TcpServe传给TcpConnection的closeCallback_()
//也就是执行Tcpserver的removeConnection
void TcpConnection::handleClose(){
    LOG_INFO("TcpConnection::handleClose fd=%d,state=%d\n",channel_->fd(),(int)state_);
    setState(kDisconnected);
    channel_->disableAll();//将channel从poller上删除

    TcpConnectionPtr connPtr(shared_from_this());
    connectionCallback_(connPtr);
    closeCallback_(connPtr);//要将TcpConnection删除，执行Tcpserver的removeConnection
}

void TcpConnection::handleError(){
    int optval;
    socklen_t optlen=sizeof optval;
    int err=0;
    if(::getsockopt(channel_->fd(),SOL_SOCKET,SO_ERROR,&optval,&optlen)<0){
        err= errno;
    }else{
        err=optval;
    }
    LOG_ERROR("Tcpconnection::handleErrno name:%s -SO_ERROR:%d \n",name_.c_str(),err);
}




