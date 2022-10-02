#pragma once

/*
此处将头文件直接包含，给用户提供方便，不用用户自己再去包含了
*/
#include"EventLoop.h"
#include"Acceptor.h"
#include"InetAddress.h"
#include"noncopyable.h"
#include"EventLoopThreadPool.h"
#include"Callbacks.h"
#include"TcpConnection.h"
#include"Buffer.h"


#include<functional>
#include<string>
#include<memory>
#include<atomic>
#include<unordered_map>

//TcpServer将mainloop,subloop之间串联起来
class TcpServer :noncopyable{
public:
	using ThreadInitCallback = std::function<void(EventLoop*)>;

	enum Option {
		kNoReusePort,
		kReusePort,
	};

	TcpServer(EventLoop* loop, const InetAddress& listenAddr,const std::string &nameArg, Option option = kNoReusePort);
	~TcpServer();

	void setThreadInitcallback(const ThreadInitCallback& cb) { threadInitCallback_ = cb; }//一般不会去设置线程初始化回调函数
	void setConnectionCallback(const ConnectionCallback& cb) { connectionCallback_ = cb; }
	void setMessageCallback(const MessageCallback& cb) { messageCallback_ = cb; }
	void setWriteCompleteCallback(const WriteCompleteCallback& cb) { writeCompleteCallback_ = cb; }


	//设置底层subloop的个数 
	void setThreadNum(int numThreads);

	//用来触发Thread,Eventloopthread,Eventloopthreadpool类内的方法(EventLoopThreadPool::start()),开启多个线程
	//还要开启底层Acceptor的listen()
	void start();
private:

	void newConnection(int sockfd, const InetAddress& peerAddr);
	void removeConnection(const TcpConnectionPtr& conn);
	void removeConnectionInLoop(const TcpConnectionPtr& conn);


	using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;

	EventLoop* loop_;//用户定义的loop->baseloop

	const std::string ipPort_;
	const std::string name_;

	std::unique_ptr<Acceptor>acceptor_;//acceptor_运行在mainloop，监听新用户连接事件

	std::shared_ptr<EventLoopThreadPool>threadPool_;//one loop per thread

	ConnectionCallback connectionCallback_;//有新连接时的回调
	MessageCallback messageCallback_;//有读写消息时的回调
	WriteCompleteCallback writeCompleteCallback_;//消息发送完后的回调

	ThreadInitCallback threadInitCallback_;//loop线程初始化后调用的回调
	std::atomic_int started_;

	int nextConnId_;
	ConnectionMap connections_;//保存所有的连接



};