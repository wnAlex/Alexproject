#pragma once
#include"noncopyable.h"
#include"Socket.h"
#include"Channel.h"

#include<functional>

class EventLoop;
class InetAddress;

class Acceptor:noncopyable {
public:
	using NewConnectionCallback = std::function<void(int sockfd, const InetAddress&)>;
	Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport);
	~Acceptor();

	void setNewConnectionCallback(const NewConnectionCallback& cb) {
		newConnectionCallback_ = std::move(cb);
	}

	bool listenning()const { return listenning_; }
	void listen();


private:

	void handleRead();

	EventLoop* loop_;//Acceptor用的就是用户定义的那个baseloop，也就是mainloop
	Socket acceptSocket_;
	Channel acceptChannel_;//封装最开始的listenfd，然后加入到baseloop的poller中去
	NewConnectionCallback newConnectionCallback_;//当有新用户连接后，会生成新的connfd，回调函数负责将其封装成channel，然后交给一个subloop去执行
	bool listenning_;
};