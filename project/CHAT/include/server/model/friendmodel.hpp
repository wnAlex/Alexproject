#pragma once

#include"user.hpp"
#include<vector>
using namespace std;


//提供好友表的接口操作方法
class FriendModel{
public:
    //添加好友关系
    void insert(int userid,int friendid);

    //返回用户的好友列表 不能只返回用户的好友id，还要把用户的好友的name state一起返回
    vector<User> query(int userid);

};