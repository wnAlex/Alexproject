#pragma once

#include"noncopyable.h"

#include<functional>
#include<string>
#include<vector>
#include<memory>

class EventLoop;
class EventLoopThread;

class EventLoopThreadPool :noncopyable {
public:
	using ThreadInitCallback = std::function<void(EventLoop*)>;//创建一个线程后，要执行的初始化操作

	EventLoopThreadPool(EventLoop* baseLoop, const std::string& nameArg);
	~EventLoopThreadPool();

	//Tcpserver:setThreadNum->EventLoopThreadPool: setThreadNum
	void setThreadNum(int numThreads) { numThreads_ = numThreads; }

	void start(const ThreadInitCallback& cb = ThreadInitCallback());//根据指定的线程数量，开启对应的事件循环线程

	//如果工作在多线程中，baseloop(mainloop)默认以轮询的方式分配channel给subloop
	EventLoop* getNextLoop();

	std::vector<EventLoop*>getAllLoops();//返回pool中所有的loops

	bool started()const { return started_; }
	const std::string name()const { return name_; }

private:
	//在开始调用Tcpserver时，会先在main()中先创建一个eventloop(就是baseloop)，
	//如果不设置线程数，baseloop对应的就是主线程，这个线程既监听新用户连接，又进行连接用户的读写
	EventLoop* baseLoop_;
	std::string name_;
	bool started_;
	int numThreads_;
	int next_;
	std::vector<std::unique_ptr<EventLoopThread>>threads_;
	std::vector<EventLoop*>loops_;
};
