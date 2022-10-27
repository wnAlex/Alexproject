#include"chatserver.hpp"
#include"signal.h"
#include"chatservice.hpp"
#include<iostream>

using namespace std;
void resetHandler(int){
    ChatService::instance()->reset();
    exit(0);
}
int main(int argc, char ** argv){
    if(argc<3){
        cerr<<"command invalid! example: ./ChatServer 127.0.0.1 6000"<<endl;
        exit(-1);
    }

    //解析通过命令行参数传递的ip和port
    char *ip=argv[1];
    uint16_t port=atoi(argv[2]);



    /*
    C的标准库有个信号处理函数,这个信号是指能够打断程序运行的信号,比如ctrl + c,非法访问寄存器,等非正常的程序终止这三类,
    signal(参数一,参数二)的用法:
        1:参数一:检测到的信号类型
        {
            SIGABRT = 程序异常终止;
            SIGINT = 用户手用关闭程序(ctrl + c)
            SIGSEGV = 非法访问存储器
        }
        2:参数二:中断信号的处理方式
        注意:
            1:信号处理函数signal()必须放在在程序开始,其作用类似于中断向量表!
            2:参数二也可以自己编写信号处理函数,但是一般以默认为主!
            3:当中断信号产生时,signum宏被自动为该信号的类型!
            4:注意参数二传递的是函数地址
    */
    //SIGINT是中断信号的宏
    /*
    	void (*signal(int sig, void (*func)(int)))(int)
        该函数设置一个函数来处理信号，即信号处理程序        
    */
    signal(SIGINT,resetHandler);
    EventLoop loop;
    InetAddress addr(ip,port);
    ChatServer server(&loop,addr,"ChatServer");

    server.start();
    loop.loop();

    return 0;
}