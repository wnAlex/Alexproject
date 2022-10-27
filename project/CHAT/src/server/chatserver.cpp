#include "chatserver.hpp"
#include"json.hpp"
#include"chatservice.hpp"

#include <functional>
#include<string>

using namespace std;
using namespace placeholders;
using json=nlohmann::json;


//初始化聊天服务器对象
ChatServer::ChatServer(EventLoop *loop,
                       const InetAddress &listenAddr,
                       const string &nameArg)
    : _server(loop, listenAddr, nameArg), _loop(loop)
{
    //注册连接回调
    _server.setConnectionCallback(std::bind(&ChatServer::onConnection,this,_1));
    //注册读写回调
    _server.setMessageCallback(std::bind(&ChatServer::onMessage,this,_1,_2,_3));
    //设置线程数量
    _server.setThreadNum(2);
}

//启动服务
void ChatServer::start()
{
    _server.start();
}
 
//连接建立和断开时调用的回调
void ChatServer::onConnection(const TcpConnectionPtr &conn)
{   
    //客户端断开连接
    if(!conn->connected()){

        ChatService::instance()->clientCloseException(conn);
        conn->shutdown();//回收服务器端fd资源

    }
}

//已建立的连接发生读写事件时的回调
//这个回调会被多个线程调用，所以所有的业务代码都会在多线程环境中使用
void ChatServer::onMessage(const TcpConnectionPtr &conn,
               Buffer *buffer,
               Timestamp time)
{
    string buf=buffer->retrieveAllAsString();
    //数据的反序列化
    json js =json::parse(buf);

    //这部分仍然属于网络模块，在此处可以根据json对象的msgid去调用不同的业务代码，但是不服务网络模块和业务模块拆分的oop原则
    //因此引入了chatservice作为业务类，chatserver作为网络类，可以采用的一种方式是类似与muduo的方式（chatserver和tcpserver的关系）
    //1.在网络类中提供setcallback()接口和对应的_funccallback回调函数对象，然后在业务模块书写相应的回调函数
    //  并调用网络类的setcallback()接口将回调一个个设置给网络类的函数对象，然后在网络类中根据json对象的msgid去调用内部不同的函数对象
    //  但是这样的话当业务模块发生了更改后，还是需要去改变网络模块的代码，例如需要增加相应的函数对象和增加相应的if选择语句
    //故为了完全的分离，引入了第二种方法
    //2.主要的工作就是将根据msgid去调用不同函数的工作转移到了业务类中，在业务类保留一个map表，保存msgid对应的不同回调
    //  并且在业务类中完成相应的map表项注册，并提供一个单例接口，在网络类调用业务类单例接口获取单例对象，然后根据msgid调用不同的回调函数
    //  这样当业务代码变更时，对于网络模块代码不需要任何改动，只在业务类中进行回调的更改和map表的注册即可
    //-------最主要的关键在于msgid和回调的配合，第一种方式由业务类将回调对象传给网络类，网络类保留回调对象和msgid，多路分解的操作放在网络类完成，而回调对象本身会随着业务的变更而改变
    //---------第二种方法将回调对象由业务类持有，反而是网络类将msgid传给业务类，多路分解的操作放在业务类完成，map起到了多路分解的作用
    //------且第二种方法chatservice是单例模式，不需要考虑网络层调用回调时业务层对象已经被其他线程释放了的情况
    //-------同时如果不是单例模式，则chatserver应该有一个智能指针指向chatservice,从而保证chatservice对象中的_userConnMap是全局共享的
    auto msgHandler=ChatService::instance()->getHandler(js["msgid"].get<int>());//js["msgid"]本身还是js中的类型，可以直接输出是因为做了运算符重载，这里需要转成int
    msgHandler(conn,js,time);
} 