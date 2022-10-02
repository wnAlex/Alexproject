#pragma once
#include"noncopyable.h"
#include"InetAddress.h"
#include"Callbacks.h"
#include"Buffer.h"
#include"Timestamp.h"

#include<memory>
#include<string>
#include<atomic>

class Channel;
class EventLoop;
class Socket;

/*
    Tcpserver开启后，Acceptor开始运行，监听到新连接后产生connfd，然后封装成TcpConnection
    在TcpConnection中设置connfd的回调函数(最后设置到connfd对应的channel中)，然后将其交给subloop，
    subloop将其中的channel放入epoll进行监听，监听到后调用对应channel的回调函数
*/


//用来打包成功连接服务器与客户端的一条通信链路
//enable_shared_from_this用来保证当回调TcpConnection中的函数时，可以获悉TcpConnection这个对象的生存状态
class TcpConnection:noncopyable,public std::enable_shared_from_this<TcpConnection>{
public:

    TcpConnection(EventLoop *loop,const std::string &nameArg,int sockfd,
                const InetAddress&localAddr,const InetAddress &peerAddr);
    ~TcpConnection();

    EventLoop*getLoop()const{return loop_;}
    const std::string &name()const{return name_;}
    const InetAddress&localAddress(){return localAddr_;}
    const InetAddress&peerAddress(){return peerAddr_;}

    bool connected()const {return state_==kConnected;}

    void send(const std::string &buf);//发送数据
    void shutdown();//关闭tcp连接

    void setConnectionCallback(const ConnectionCallback&cb){connectionCallback_=cb;}
    void setMessageCallback(const MessageCallback &cb){messageCallback_=cb;}
    void setWriteCompleteCallback(const WriteCompleteCallback&cb){writeCompleteCallback_=cb;}
    void setCloseCallback(const CloseCallback &cb){closeCallback_=cb;}
    void setHighWaterMarkCallback(const HighWaterMarkCallback&cb,size_t highWaterMark)
    {
        highWaterMarkCallback_=cb;
        highWaterMark_=highWaterMark;
    }

    //连接建立
    void connectEstablished();
    //连接销毁
    void connectDestroyed();

    
private:

    enum StateE{kDisconnected,kConnecting,kConnected,kDisconnecting};//标识连接的状态
    void setState(StateE state){state_=state;}

    void handleRead(Timestamp receiveTime);
    void handleWrite();
    void handleClose();
    void handleError();

    void sendInLoop(const void *data,size_t len);

    void shutdownInLoop();

    EventLoop *loop_;//这里绝对不是baseloop,因为TcpConnection都是在subloop中管理的
    const std::string name_;
    std::atomic_int state_;//要在多线程中使用，故用atomic类型
    bool reading_;

    std::unique_ptr<Socket>socket_;
    std::unique_ptr<Channel>channel_;

    const InetAddress localAddr_;
    const InetAddress peerAddr_;

    //用户通过Tcpserver传入TcpConnection的回调函数,由TcpConnection扔给channel
    ConnectionCallback connectionCallback_;//有新连接时的回调
	MessageCallback messageCallback_;//有读写消息时的回调
	WriteCompleteCallback writeCompleteCallback_;//消息发送完后的回调
    HighWaterMarkCallback highWaterMarkCallback_;//发送数据量到达高水位的回调
    CloseCallback closeCallback_;//关闭连接的回调

    size_t highWaterMark_;//水位线

    Buffer inputBuffer_;//接受fd数据的缓冲区
    Buffer outputBuffer_;//向fd发数据的缓冲区
};
