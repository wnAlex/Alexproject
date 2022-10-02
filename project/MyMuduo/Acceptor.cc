#include"Acceptor.h"
#include"Logger.h"
#include"InetAddress.h"

#include<sys/types.h>
#include<sys/socket.h>
#include<errno.h>
#include<functional>
#include<unistd.h>

//产生socket的函数，静态全局函数，只在当前.cc中有效
static int createNonblocking() {
	int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
	if (sockfd < 0) {
		LOG_FATAL("%s:%s:%d listen socket create err:%d \n", __FILE__, __FUNCTION__, __LINE__, errno);
	}
    return sockfd;
}


Acceptor::Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport) 
	:loop_(loop)
	,acceptSocket_(createNonblocking()) //创建一个套接字
	,acceptChannel_(loop,acceptSocket_.fd())//loop下面封装了channel和poller，所以channel必须通过loop才能将自己加到poller上面
	,listenning_(false)
{
	acceptSocket_.setReuseAddr(true);
	acceptSocket_.setReusePort(true);
	acceptSocket_.bindAddress(listenAddr);//绑定一个套接字
	//Tcpserver:start()->Acceptor:listen() 
	//当有新用户连接后，会生成新的connfd，回调函数负责将其封装成channel，然后交给一个subloop去执行
	//此时就在封装回调函数，将newConnectionCallback_封装成handleRead这个回调中
	acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));

}

Acceptor::~Acceptor() {
	acceptChannel_.disableAll();
	acceptChannel_.remove();
	//close(fd)是由socket的析构函数去做的
}




void Acceptor::listen() {
	listenning_ = true;
	acceptSocket_.listen();//监听一个套接字
	acceptChannel_.enableReading();//将acceptChannel_注册到baseloop的poller上面，有事件发生就会执行回调(handleRead())
}

//listenfd上面有新用户连接了，就回调handleRead，这个函数中会再去回调newConnectionCallback_
//newConnectionCallback_是由Tcpserver设置的
void Acceptor::handleRead() {
	InetAddress peerAddr;
	//单纯的socket编程中accept是一个阻塞函数，未收到连接就会阻塞，但是这里面相当于利用epoll_wait去阻塞，其返回后在调用accept去读取连接
	int connfd = acceptSocket_.accept(&peerAddr);
	if (connfd >= 0) {
		if (newConnectionCallback_) {
			newConnectionCallback_(connfd, peerAddr);//此回调就是用来轮询找到subloop，唤醒，分发当前的新客户端的channel
		}
		else {//如果来了新用户连接，但是没有相应的回调，说明无法提供给客户端服务
			::close(connfd);
		}
	}
	else {
		LOG_ERROR("%s:%s:%d accept err:%d \n", __FILE__, __FUNCTION__, __LINE__, errno);
		if (errno == EMFILE)//当前进程文件描述符用完了,一般可以修改进程文件描述符限制，或者进行分布式部署服务器
		{
			LOG_ERROR("%s:%s:%d sockfd reached limit \n", __FILE__, __FUNCTION__, __LINE__);
		}
	}
}