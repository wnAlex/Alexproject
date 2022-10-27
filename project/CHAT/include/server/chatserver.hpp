#pragma once
#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>

using namespace muduo;
using namespace muduo::net;

//聊天服务器的主类
class ChatServer
{
public:
    //初始化聊天服务器对象
    ChatServer(EventLoop *loop,
               const InetAddress &listenAddr,
               const string &nameArg);

    //启动服务
    void start();

private:
    void onConnection(const TcpConnectionPtr &);//连接建立和断开时调用的回调

    void onMessage(const TcpConnectionPtr &,
                   Buffer *,
                   Timestamp);//已建立的连接发生读写事件时的回调

    TcpServer _server;//组合的muduo库中的服务器对象，用来实现chatServer
    EventLoop *_loop;//指向baseloop的指针，用来在外部关闭TcpServer对象
};
