#pragma once

#include"noncopyable.h"
#include"Thread.h"

#include<functional>
#include<mutex>
#include<condition_variable>
#include<string>

class EventLoop;

//EventLoopThread绑定一个loop和一个thread，在一个thread中创建一个loop，让一个loop运行在一个thread中
class EventLoopThread :noncopyable {
public:
	using ThreadInitCallback = std::function<void(EventLoop*)>;//创建一个线程后，要执行的初始化操作

	EventLoopThread(const ThreadInitCallback& cb = ThreadInitCallback(),
		const std::string& name = std::string());
	~EventLoopThread();

	EventLoop* startLoop();//开启循环

private:
	void threadFunc();//创建eventloop的函数

	EventLoop* loop_;
	bool exiting_;
	Thread thread_;
	std::mutex mutex_;
	std::condition_variable cond_;
	ThreadInitCallback callback_;
};