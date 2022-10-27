#pragma once
#include<muduo/net/TcpServer.h>
#include<muduo/net/TcpConnection.h>
#include<unordered_map>
#include<functional>
#include<mutex>

#include"json.hpp"
#include"usermodel.hpp"
#include"offlinemessagemodel.hpp"
#include"friendmodel.hpp"
#include"groupmode.hpp"
#include"redis.hpp"

using namespace std;
using namespace muduo;
using namespace muduo::net;
using json=nlohmann::json;

//表示处理消息的事件回调类型
using MsgHandler=std::function<void(const TcpConnectionPtr&conn,json &js,Timestamp)>;

//聊天服务器业务类----单例模式
class ChatService{
public:

    //获取单例对象的接口函数
    static ChatService*instance();

    //获取消息对应的处理器  接口函数
    MsgHandler getHandler(int msgid);

    //处理redis返回信息
    void handlerRedisSubscribeMessage(int channel,string message);

    //处理登录业务
    void login(const TcpConnectionPtr&conn,json&js,Timestamp time);

    //处理注册业务
    void reg(const TcpConnectionPtr&conn,json &js ,Timestamp time);

    //处理注销业务
    void loginout(const TcpConnectionPtr&conn,json &js ,Timestamp time);

    //一对一聊天业务
    void oneChat(const TcpConnectionPtr&conn,json &js ,Timestamp time);

    //添加好友业务
    void addFriend(const TcpConnectionPtr&conn,json &js ,Timestamp time);

    //创建群组业务
    void creatGroup(const TcpConnectionPtr&conn,json &js ,Timestamp time);

    //加入群组业务
    void addGroup(const TcpConnectionPtr&conn,json &js ,Timestamp time);

    //群组聊天业务
    void groupChat(const TcpConnectionPtr&conn,json &js ,Timestamp time);

    //处理客户端异常退出
    void clientCloseException(const TcpConnectionPtr&conn);

    //处理服务器异常退出，进行业务重置
    void reset();


private:
    ChatService();
    
    //存储消息id和其对应业务的处理方法（回调函数）
    //回调函数onmessage()会被多个线程调用，所以所有的业务代码都会在多线程环境中使用
    //这个不需要考虑线程安全问题，因为MsgHandler都是代码运行前事先添加好的，不存在运行过程中MsgHandler增加和删除的情况
    //即代码运行过程中只会读取map，不会修改map
    unordered_map<int,MsgHandler>_msgHandlerMap;


    //存储在线用户的通信连接,用来聊天时服务器转发消息
    //回调函数onmessage()会被多个线程调用，所以所有的业务代码都会在多线程环境中使用
    //这个需要考虑线程安全问题，因为TcpConnectionPtr时代码运行过程中不断变化的，可能由一个A线程建立，又由B线程删除，
    //即同一时刻会有多个线程同时访问这个map，而map自身无法保证线程安全，可能会存在register++那种问题，故需要上锁
    //不然可能导致A，B线程同时调用回调函数，同时访问修改map，同时增加TcpConnectionPtr，而c++容器自身不考虑线程安全，故需要添加线程互斥操作
    unordered_map<int,TcpConnectionPtr>_userConnMap;


    //定义互斥锁保证_userConnMap的线程安全
    mutex _connMutex;

    //user数据操作类对象
    UserModel _userModel;

    //offlinemessage数据操作类对象
    OfflineMsgModel _offlineMsgModel;

    //friend数据操作类对象
    FriendModel _friendModel;

    //allgroup groupuser 数据操作类对象
    GroupModel _groupModel;

    //redis操作对象
    Redis _redis;

};