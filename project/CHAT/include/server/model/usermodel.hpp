#pragma once
#include"user.hpp"

//user表的数据操作类，真正提供user表操作方法的类 
//model层提供给业务层的只应该是对象信息，而不是sql语句或者字段等信息，model层就是要将这些东西封装好
//封装了所有的mysql底层api调用
class UserModel{
public:
    //User表的增加方法 就是将sql语句进行一层层封装 直接传入user对象，然后就可以通过当前类拆解对象 调用mysql类封装的底层sql api完成操作
    bool insert(User &user);//传入引用是因为传入时id字段没法填，是数据库自动生成的，故传入引用生成id后再写入user中

    //根据用户id查询用户信息
    User query(int id);

    //更新用户的状态信息
    bool updateState(User user);

    //重置用户的状态信息
    void resetState();
};