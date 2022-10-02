#pragma once
#include"noncopyable.h"

#include<functional>
#include<memory>
#include<thread>
#include<unistd.h>
#include<string>
#include<atomic>

class Thread :noncopyable {
public:
	//传入Thread的函数类型为void(),如果后续想传参数，可以采用bind进行参数绑定
	using ThreadFunc = std::function<void()>;

	explicit Thread(ThreadFunc, const std::string& name = std::string());
	~Thread();

	void start();
	void join();

	bool started() { return started_; }//标识线程是否启动
	pid_t tid()const { return tid_; }//返回线程号，不是pthread_self那个线程号，是pid指令查看的线程号
	const std::string& name()const { return name_; }//返回线程名字

	static int numCreated() { return numCreated_; }//返回全局线程数


private:
	void setDefaultName();//给线程设置名字

	bool started_;
	bool joined_;
	//无法直接定义std::thread thread_  ->因为这样创建线程后会直接启动，需要用智能指针控制线程启动时间
	std::shared_ptr<std::thread> thread_;
	pid_t tid_;
	ThreadFunc func_;//存储线程函数
	std::string name_;//线程名字，用于调试
	static std::atomic_int numCreated_;//记录全局线程个数
	
};
