#include"EventLoopThreadPool.h"
#include"EventLoopThread.h"

#include<memory>
#include<string>
EventLoopThreadPool::EventLoopThreadPool(EventLoop* baseLoop, const std::string& nameArg)
	:baseLoop_(baseLoop)
	,name_(nameArg)
	,started_(false)//未启动
	,numThreads_(0)
	,next_(0)//轮询的下标
{
}

EventLoopThreadPool::~EventLoopThreadPool() {
	//不需要关注vector中的Eventloop指针所指向的外部资源要不要手动delete
	//因为都是栈对象，会自动析构
}



void EventLoopThreadPool::start(const ThreadInitCallback& cb )//根据指定的线程数量，开启对应的事件循环线程
{
	started_ = true;
	for (int i = 0; i < numThreads_; i++) {
		char buf[name_.size() + 32];
		snprintf(buf, sizeof buf, "%s%d", name_.c_str(), i);
		EventLoopThread* t = new EventLoopThread(cb, buf);
		threads_.push_back(std::unique_ptr<EventLoopThread>(t));//将EventLoopThread加入vector中
		loops_.push_back(t->startLoop());//底层创建线程，绑定一个新的EventLoop，返回该loop的地址

	}

	//整个服务端只有一个主线程，运行着用户创建的那个loop(baseloop)
	//并且线程的初始化回调函数不为空
	if (numThreads_ == 0&&cb) {
		cb(baseLoop_);
	}
}

//如果工作在多线程中，baseloop(mainloop)默认以轮询的方式分配channel给subloop
EventLoop* EventLoopThreadPool::getNextLoop() {
	//如果没有创建多个线程，永远用的都是baseloop
	EventLoop* loop = baseLoop_;
	if (!loops_.empty()) {//通过轮询获取下一个处理事件的loop
		loop = loops_[next_];
		++next_;
		if (next_ >= loops_.size()) {
			next_ = 0;
		}
	}
	return loop;
}

std::vector<EventLoop*>EventLoopThreadPool::getAllLoops()//返回pool中所有的loops
{
	if (loops_.empty()) {
		return std::vector<EventLoop*>(1, baseLoop_);
	}
	else {
		return loops_;
	}
}