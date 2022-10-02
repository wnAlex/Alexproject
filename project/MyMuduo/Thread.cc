#include"Thread.h"
#include"CurrentThread.h"

#include<semaphore.h>
//静态成员变量类内声明，类外初始化
std::atomic_int Thread::numCreated_ (0);

Thread::Thread(ThreadFunc func, const std::string& name)
	:started_(false)
	,joined_(false)
	,tid_(0)//tid为0，是因为线程还未创建，需要创建后才能返回线程号赋值给tid（tid应该是要作为参数传入pthread_creat()中的）
	,func_(std::move(func))
	,name_(name)
	//智能指针变量默认构造，因为此时还未想真正去启动线程
{
	setDefaultName();
}

Thread::~Thread() {
	//线程已经运行起来了才需要析构，否则啥也不需干
	if (started_ && !joined_) {
		//将当前线程设置为分离线程，自己负责自己的资源回收，防止出现孤儿线程
		thread_->detach();//detach()是thread类提供的设置分离线程的方法，调用底层pthread_detach();
	}
}

void Thread::start() {//一个thread对象记录的就是一个新线程的详细信息
	started_ = true;
	sem_t sem;
	sem_init(&sem, false, 0);//初始化信号量sem，非多进程间共享，初始值为0
	thread_ = std::shared_ptr<std::thread>(new std::thread([&]() {//用lambda表达式，以引用的方式接受外部对象,捕获中只写一个&，表明默认接受外界所有参数的引用（即整个类的全部 属性）。
		//获取线程的tid值
		tid_ = CurrentThread::tid();
		sem_post(&sem);

		func_();//子线程主要作用就是执行线程函数func_()

	}));

	//这里必须等待获取上面新创建的线程的tid值(start函数的要求,start必须要保证自身结束的时候线程已经创建好并启动了),故通过信号量实现线程间同步
	sem_wait(&sem);//如果sem为0，则调用start()函数的线程会阻塞在这里,等待创建的子线程给信号量+1后，切回来才能继续运行
}

void Thread::join() {
	joined_ = true;
	thread_->join();
}

void Thread::setDefaultName() {
	int num = ++numCreated_;
	if (name_.empty()) {
		char buf[32] = { 0 };
		snprintf(buf, sizeof buf, "Thread%d", num);
		name_ = buf;
	}

}