#include"EventLoopThread.h"
#include"EventLoop.h"

#include<mutex>
EventLoopThread::EventLoopThread(const ThreadInitCallback& cb ,
	const std::string& name) 
	:loop_(nullptr)//此时loop指向为空，因为要先创建线程，再创建loop对象
	,exiting_(false)//标识线程是否退出
	,thread_(std::bind(&EventLoopThread::threadFunc,this),name)//此时Thread对象创建了，但子线程thread并未开始创建并启动(要手动调用start()才可以)
	,mutex_()
	,cond_()
	,callback_(cb)
{

}
EventLoopThread::~EventLoopThread() {
	exiting_ = true;
	if (loop_ != nullptr) {
		loop_->quit();
		thread_.join();//等待底层子线程结束
	}
}

EventLoop* EventLoopThread::startLoop()//开启循环
{
	thread_.start();//开启循环第一件事就是开启底层的一个新线程，然后去执行传入的回调函数threadFunc()
	EventLoop* loop = nullptr;

	{
		//这里用条件变量是为了防止新线程创建后先去执行主线程
		//若先执行主线程，此处将mutex上锁，然后阻塞在wait函数，wait函数会将mutex释放
		//之后切到子线程，给mutex上锁后继续执行，唤醒cond，然后主线程才继续执行，
		//总的来说，就是为了保证子线程操作在主线程前面
		std::unique_lock<std::mutex>lock(mutex_);
		while (loop_ == nullptr) {
			cond_.wait(lock);//当新线程的eventloop还未创建好时，当前线程就等待在这个锁上，直到新线程调用cond_.notify_one()唤醒当前线程
		}
		loop = loop_;
	}

	return loop;//当调用startLoop()方法时，会获得一个新线程，然后返回这个新线程中eventloop对象的地址，类似于线程间通信
}

//当前方法，是每次一个新线程创建后要去运行的方法
void EventLoopThread::threadFunc()//创建eventloop的函数
{
	EventLoop loop;//新线程上来就创建一个独立的eventloop，和线程一一对应 one loop per thread
	if (callback_) {
		callback_(&loop);//可以通过这个回调函数，设置想对每个线程的loop做的事情
	}

	{
		std::unique_lock<std::mutex>lock(mutex_);
		loop_ = &loop;//EventloopThread的成员变量eventloop*记录了线程中创建的eventloop
		cond_.notify_one();//通知一个等待在这把锁上的对象
	}
	loop.loop();//Eventloop:loop() -> Poller:poll()

	std::unique_lock<std::mutex>lock(mutex_);
	loop_ = nullptr; 
	

}