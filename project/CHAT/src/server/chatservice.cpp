#include"chatservice.hpp"
#include"public.hpp"
#include"usermodel.hpp"
#include"friendmodel.hpp"
#include<muduo/base/Logging.h>
#include<vector>
using namespace muduo;
using namespace std;



//获取单例对象的接口函数
//这种写法是线程安全的，因为c11以后编译器保证局部静态变量的线程安全,不存在两个线程同时定义一个局部静态变量的问题了
ChatService* ChatService::instance(){
    static ChatService service;
    return &service;
}

//构造函数进行成员变量初始化 注册消息已经对应的回调操作
ChatService::ChatService(){
    _msgHandlerMap.insert({LOGIN_MSG,std::bind(&ChatService::login,this,_1,_2,_3)});
    _msgHandlerMap.insert({REG_MSG,std::bind(&ChatService::reg,this,_1,_2,_3)});
    _msgHandlerMap.insert({ONE_CHAT_MSG,std::bind(&ChatService::oneChat,this,_1,_2,_3)});
    _msgHandlerMap.insert({ADD_FRIEND_MSG,std::bind(&ChatService::addFriend,this,_1,_2,_3)});
    _msgHandlerMap.insert({CREATE_GROUP_MSG,std::bind(&ChatService::creatGroup,this,_1,_2,_3)});
    _msgHandlerMap.insert({ADD_GROUP_MSG,std::bind(&ChatService::addGroup,this,_1,_2,_3)});
    _msgHandlerMap.insert({GROUP_CHAT_MSG,std::bind(&ChatService::groupChat,this,_1,_2,_3)});
    _msgHandlerMap.insert({LOGINOUT_MSG,std::bind(&ChatService::loginout,this,_1,_2,_3)});

    //连接redis服务器
    if(_redis.connect()){
        //设置上报消息的回调
        _redis.init_notify_handler(std::bind(&ChatService::handlerRedisSubscribeMessage,this,_1,_2));
    }

}

//从redis消息队列中获取的订阅的消息
void ChatService::handlerRedisSubscribeMessage(int userid,string msg){
    unique_lock<mutex>_lock(_connMutex);
    auto it=_userConnMap.find(userid);
    if(it!=_userConnMap.end()){
        it->second->send(msg);
        return ;
    }

    //存储该用户的离线消息--防止消息队列上报消息过程中userid下线
    _offlineMsgModel.insert(userid,msg);
}

//获取消息对应的处理器  接口函数
MsgHandler ChatService::getHandler(int msgid){
    //记录错误日志，msgid没有对应的事件处理回调函数--用muduo的日志系统打印
    auto it=_msgHandlerMap.find(msgid);
    if(it==_msgHandlerMap.end()){
        //没有对应的处理器，就返回一个空处理器，打印错误输出即可
        return [msgid](const TcpConnectionPtr&conn,json &js,Timestamp){
            LOG_ERROR<<"msgid:"<<msgid<<"can not find handler!";
        };
    }else{
        return _msgHandlerMap[msgid];
    }

    
}

//处理登录业务
/*json格式 
          msgid LOGIN_MSG
          id:13
          password:"xxx"
*/
void ChatService::login(const TcpConnectionPtr&conn,json&js,Timestamp time){
    int id=js["id"].get<int>();//js["id"]本身还是js中的类型，可以直接输出是因为做了运算符重载，这里需要转成int
    string pwd=js["password"];

    User user=_userModel.query(id);
    if(user.getId()==id&&user.getPwd()==pwd){
        if(user.getState()=="online"){
            //该用户已经登陆，不能重复登陆
            json response;
            response["msgid"]=LOGIN_MSG_ACK;
            response["errno"]=2;
            response["errmsg"]="This account is using,please input another one!";
            conn->send(response.dump());
        }else{
            //登陆成功，记录用户连接信息，以用于后续聊天功能的实现
            {
                unique_lock<mutex>(_connMutex);
                _userConnMap.insert({id,conn});
            }
            
            //id登录成功后，向redis订阅channel(id)
            _redis.subscribe(id);

            //登陆成功，更新用户状态信息 state:offline->online 
            //数据库修改表对应的并法操作是由mysql server保证的
            user.setState("online");
            _userModel.updateState(user);
            json response;
            response["msgid"]=LOGIN_MSG_ACK;
            response["errno"]=0;
            response["id"]=user.getId();
            response["name"]=user.getName();
            
            //查询用户是否有离线消息
            vector<string>vec=_offlineMsgModel.query(id);
            if(!vec.empty()){
                response["offlinemsg"]=vec;
                //读取该用户的离线消息后，把该用户的所有离线消息删除掉
                _offlineMsgModel.remove(id);
            }
            
            //查询该用户的好友信息，一起返回回去  一般的业务中不需要从服务器返回用户的好友信息
            //用户的好友信息应该保存在客户端本地文件中，每次登陆直接读取即可，从服务器发送对服务器负载太大了
            vector<User>userVec=_friendModel.query(id);
            if(!userVec.empty()){
                vector<string>vec2;
                for(User&user:userVec){
                    json js;
                    js["id"]=user.getId();
                    js["name"]=user.getName();
                    js["state"]=user.getState();
                    vec2.push_back(js.dump());
                }
                //此处不可以直接response["friends"]=userVec,因为User对象不是内置数据类型，js不知道如何排列
                //故需要转成string然后在放入js对象中
                response["friends"]=vec2;
            }

            //查询用户的群组信息
            vector<Group>groupsuerVec=_groupModel.queryGroups(id);
            if(!groupsuerVec.empty()){
                //group:[{groupid:[xxx,xxx,xxx,xxx]}]
                vector<string>groupV;
                for(Group&group:groupsuerVec){
                    json grpjson;
                    grpjson["id"]=group.getId();
                    grpjson["groupname"]=group.getName();
                    grpjson["groupdesc"]=group.getDesc();
                    vector<string>userV;
                    for(GroupUser&user:group.getUsers()){
                        json js;
                        js["id"]=user.getId();
                        js["name"]=user.getName();
                        js["state"]=user.getState();
                        js["role"]=user.getRole();
                        userV.push_back(js.dump());
                    }
                    grpjson["users"]=userV;
                    groupV.push_back(grpjson.dump());
                }
                response["groups"]=groupV;
            }


            conn->send(response.dump());

            
        }
        
    }else{
        //用户不存在或密码错误，登陆失败
        json response;
        response["msgid"]=LOGIN_MSG_ACK;
        response["errno"]=1;
        response["errmsg"]="Id or password is wrong! ";
        conn->send(response.dump());

    }

}

//处理注册业务
/*json格式 
          msgid REG_MSG
          name:"zhangsan"
          password:"xxx"
*/
void ChatService::reg(const TcpConnectionPtr&conn,json &js ,Timestamp time){
    string name=js["name"];
    string pwd=js["password"];

    User user;
    user.setName(name);
    user.setPwd(pwd);
    bool state=_userModel.insert(user);
    if(state){
        //注册成功
        json response;
        response["msgid"]=REG_MSG_ACK;
        response["errno"]=0;
        response["id"]=user.getId();
        conn->send(response.dump());
    }else{
        //注册失败
        json response;
        response["msgid"]=REG_MSG_ACK;
        response["errno"]=1;
        conn->send(response.dump());
    }

}


 //处理注销业务
void ChatService::loginout(const TcpConnectionPtr&conn,json &js ,Timestamp time){
    int userid=js["id"].get<int>();
    {
        unique_lock<mutex>lock(_connMutex);
        auto it=_userConnMap.find(userid);
        if(it!=_userConnMap.end()){
            _userConnMap.erase(it);
        }
    }

    //用户注销，相当于下线，在redis中取消订阅通道
    _redis.unsubscribe(userid);

    //更新用户的状态信息
    User user(userid,"","","offline");
    _userModel.updateState(user);
}





//一对一聊天业务
/*json格式 
          msgid ONE_CHAT_MSG
          id:1
          name:"zhangsan"
          to:3
          msg:"xxx"
*/         
void ChatService::oneChat(const TcpConnectionPtr&conn,json &js ,Timestamp time){
    int toid=js["to"].get<int>();
    {
        unique_lock<mutex>lock(_connMutex);
        auto it=_userConnMap.find(toid);
        //toid用户在线，直接转发消息,服务器主动推送消息给toid用户
        if(it!=_userConnMap.end()){
            //这一步转发消息必须加锁，因为不加锁可能A线程刚确认完it在表中，然后切到B线程把it删除了，
            //再切回A线程转发消息，就会出现问题
            it->second->send(js.dump());
            return;
        }
    }
    //查询toid是否在线，如果在线，说明toid登录在了其他服务器上
    User user=_userModel.query(toid);
    if(user.getState()=="online"){
        _redis.publish(toid,js.dump());
        return ;
    }
    //toid不在线，存储离线消息
    _offlineMsgModel.insert(toid,js.dump());
    return;

}

//添加好友业务
/*json格式 
          msgid ADD_FRIEND_MSG
          id:1
          friendid:3
*/
void ChatService::addFriend(const TcpConnectionPtr&conn,json &js ,Timestamp time){
    int userid=js["id"].get<int>();
    int friendid=js["friendid"].get<int>();

    //存储好友的信息
    _friendModel.insert(userid,friendid);
}

//创建群组业务
/*json格式 
          msgid CREATE_GROUP_MSG
          id:1
          groupname:""
          groupdesc:""
*/
void ChatService::creatGroup(const TcpConnectionPtr&conn,json &js ,Timestamp time){
    int userid=js["id"].get<int>();
    string name=js["groupname"];
    string desc=js["groupdesc"];

    //存储新创建的群组信息
    Group group(-1,name,desc);
    if(_groupModel.createGroup(group)){
        //存储群组创建人的信息
        _groupModel.addGroup(userid,group.getId(),"creator");
        
        //可以后续增加给客户端发送的相应代码

    }

}

//加入群组业务
/*json格式 
          msgid ADD_GROUP_MSG
          id:13
          groupid:1
*/
void ChatService::addGroup(const TcpConnectionPtr&conn,json &js ,Timestamp time){
    int userid=js["id"].get<int>();
    int groupid=js["groupid"].get<int>();
    _groupModel.addGroup(userid,groupid,"normal");

    //可以后续增加给客户端发送的相应代码

}

//群组聊天业务
/*json格式 
          msgid ADD_GROUP_MSG
          id:13
          groupid:1
          msg:""
*/
void ChatService::groupChat(const TcpConnectionPtr&conn,json &js ,Timestamp time){
    int userid=js["id"].get<int>();
    int groupid=js["groupid"].get<int>();
    vector<int> useridVec=_groupModel.queryGroupUsers(userid,groupid);

    unique_lock<mutex>(_connMutex);
    for(int id:useridVec){
        
        auto it=_userConnMap.find(id);
        if(it!=_userConnMap.end()){
            //用户在线，转发消息
            it->second->send(js.dump());
        }else{
            //查询toid是否在线，如果在线，说明toid登录在了其他服务器上
            User user=_userModel.query(id);
            if(user.getState()=="online"){
                _redis.publish(id,js.dump());
                
            }else{
                //用户离线，存储消息
                _offlineMsgModel.insert(id,js.dump());
            }
            
        }

    }

}

 //处理客户端异常退出
void ChatService::clientCloseException(const TcpConnectionPtr&conn){
    User user;
    {
        unique_lock<mutex>lock(_connMutex);
        for(auto it=_userConnMap.begin();it!=_userConnMap.end();it++){
            if(it->second==conn){
                user.setId(it->first);
                _userConnMap.erase(it);
                break;
            }
        }
    }

    //用户注销，相当于下线，在redis中取消订阅通道
    _redis.unsubscribe(user.getId());

    //防止万一没找到
    if(user.getId()!=-1){
        user.setState("offline");
        _userModel.updateState(user);
    }
    

}


//处理服务器异常退出，进行业务重置
void ChatService::reset(){
    //把所有online状态的用户全都改成offline状态，以防下次开启服务器，用户登陆不上
    _userModel.resetState();
}