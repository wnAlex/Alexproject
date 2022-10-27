#include"friendmodel.hpp"
#include"db.hpp"
//添加好友关系
void FriendModel::insert(int userid,int friendid){
    char sql[1024]={0};
    sprintf(sql,"insert into friend values(%d,%d)",userid,friendid);

    MySQL mysql;
    if(mysql.connect()){
        mysql.update(sql);
    }
    
}

//返回用户的好友列表 不能只返回用户的好友id，还要把用户的好友的name state一起返回
vector<User> FriendModel::query(int userid){
    char sql[1024]={0};
    sprintf(sql,"select a.id,a.name,a.state from  user a inner join friend b on b.friendid = a.id  where b.userid =%d",userid);
    vector<User>vec;
    MySQL mysql;
    if(mysql.connect()){
        MYSQL_RES *res=mysql.query(sql);//申请一块内存，存储sql的结果，底层调用嵌入式sql
        if(res!=nullptr){
            MYSQL_ROW row;
            //把userid用户的好友的所有相关信息放入vector<user>中并返回
            //以行的方式去一行行读取当前的sql结果
            while((row=mysql_fetch_row(res))!=nullptr){
                User user;
                user.setId(atoi(row[0]));
                user.setName(row[1]);
                user.setState(row[2]);
                vec.push_back(user);
            }
            mysql_free_result(res);
            return vec;
        }      
    }
    return vec;
}
