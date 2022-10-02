#include"TcpServer.h"
#include"Logger.h"

#include<strings.h>
#include<functional>

static EventLoop* CheckLoopNotNull(EventLoop* loop) {
	if (loop == nullptr) {
		LOG_FATAL("%s:%s:%d mainloop is null \n", __FILE__, __FUNCTION__, __LINE__);
	}
	return loop;
}

TcpServer::TcpServer(EventLoop* loop, const InetAddress& listenAddr, const std::string& nameArg, Option option)
	:loop_(CheckLoopNotNull(loop))
	,ipPort_(listenAddr.toIpPort())
	,name_(nameArg)
	,acceptor_(new Acceptor(loop, listenAddr,option==kReusePort))
	,threadPool_(new EventLoopThreadPool(loop,name_))
	,connectionCallback_()
	,messageCallback_()
	,nextConnId_(1)//next connection id
	,started_(0)
{
	//当baseloop中有新用户连接时，会执行Tcpserver::newConnection回调
	acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection, this,
		std::placeholders::_1, std::placeholders::_2));

}


TcpServer::~TcpServer() {
	for(auto &item: connections_){
		TcpConnectionPtr conn(item.second);
		//connections_表中的TcpConnectionPtr不再指向TcpConnection对象，交给conn指向，
		//而conn出了循环会自动析构，释放TcpConnection资源
		item.second.reset();
	
		conn->getLoop()->runInLoop(std::bind(&TcpConnection::connectDestroyed,conn));//销毁连接
	}


}



void TcpServer::setThreadNum(int numThreads) {
	threadPool_->setThreadNum(numThreads);
}


//最终就是开启底层Acceptor的listen()
void TcpServer::start() {
	if (started_++ == 0) //防止一个Tcpserver对象被start多次
	{
		threadPool_->start(threadInitCallback_);//启动线程池
		loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get()));//直接执行

		//外部调用完start()后，就开始loop.loop()，主线程epoll就开始运行了
	}
}



/*
这个newConnection回调函数主要负责
根据轮询算法选择一个subloop
唤醒对应的subloop
把当前的connfd(传入的sockfd)封装成channel,并在channel中注册对应的回调函数
将channel分发给subloop，
*/
//当有一个客户端的连接到来，Acceptor就会执行这个回调操作
void TcpServer::newConnection(int sockfd, const InetAddress& peerAddr) {
	EventLoop*ioLoop=threadPool_->getNextLoop();
	char buf[64]={0};
	snprintf(buf, sizeof buf,"-%s#%d",ipPort_.c_str(),nextConnId_);
	nextConnId_++;//此处nextConnId_不是原子量，因为newConnection方法只在mainloop中调用，无线程安全问题
	std::string connName=name_+buf;//新建的connection的名称

	LOG_INFO("TcpServer::newConnection[%s] -new connection[%s] from %s \n"
			,name_.c_str(),connName.c_str(),peerAddr.toIpPort().c_str());

	//通过socket获取其绑定的服务器本机的ip地址和端口信息(在Acceptor构造函数中bind的)
	sockaddr_in local;
	::bzero(&local,sizeof local);
	socklen_t addrlen=sizeof local;
	if(::getsockname(sockfd,(sockaddr*)&local,&addrlen)<0){
		LOG_ERROR("scokets::getlocalAddr");
	}
	InetAddress localAddr(local);

	//根据连接成功的sockfd，创建TcpConnection连接对象
	TcpConnectionPtr conn(new TcpConnection(ioLoop,connName,sockfd,localAddr,peerAddr));
	connections_[connName]=conn;//将连接加入ConnectionMap中
	
	//下面的回调都是用户传给TcpServer的，然后TcpServer传给TcpConnection
	conn->setConnectionCallback(connectionCallback_);
	conn->setMessageCallback(messageCallback_);
	conn->setWriteCompleteCallback(writeCompleteCallback_);

	//设置了如何关闭连接的回调
	conn->setCloseCallback(std::bind(&TcpServer::removeConnection,this,std::placeholders::_1));
	//直接调用connectEstablished()方法
	//此时运行这段代码的是主线程，然后runInLoop()会唤醒subloop(TcpConnection对象所属的那个loop)，
	//子线程继续执行，最后执行到dopendingfunctors(),然后在子线程中执行TcpConnection对象的connectEstablished()方法
	//将channel注册到subloop的epoll上开始进行监听。
	ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished,conn));



}

void TcpServer::removeConnection(const TcpConnectionPtr& conn){
	loop_->runInLoop(std::bind(&TcpServer::removeConnectionInLoop,this,conn));
}

//将Tcpconnection的信息先从TcpServer的map表中删除
//然后调用执行这个Tcpconnection的connectDestroyed()
void TcpServer::removeConnectionInLoop(const TcpConnectionPtr& conn){
	LOG_INFO("TcpServer::removeConnectionInLoop[%s] - connection %s \n",name_.c_str(),conn->name().c_str());

	connections_.erase(conn->name());//将TcpConnection先从TcpServer的map表中删除
	EventLoop*ioLoop=conn->getLoop();
	ioLoop->queueInLoop(std::bind(&TcpConnection::connectDestroyed,conn));
}