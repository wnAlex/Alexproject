#include"offlinemessagemodel.hpp"
#include"db.hpp"


//存储用户的离线消息
void OfflineMsgModel::insert(int userid,string msg){
    //组装sql语句为一个字符串，然后将字符串传入mysql提供的api接口，其会自动解析
    char sql[1024]={0};
    sprintf(sql,"insert into offlinemessage values(%d,'%s')",userid,msg.c_str());

    MySQL mysql;
    if(mysql.connect()){
        mysql.update(sql);
    }
   
}

//删除用户的离线消息
void OfflineMsgModel::remove(int userid){
    char sql[1024]={0};
    sprintf(sql,"delete from offlinemessage where userid=%d",userid);

    MySQL mysql;
    if(mysql.connect()){
        mysql.update(sql);
    }
}

//查询用户的离线消息
vector<string> OfflineMsgModel::query(int userid){
    char sql[1024]={0};
    sprintf(sql,"select message from  offlinemessage where userid =%d",userid);
    vector<string>vec;
    MySQL mysql;
    if(mysql.connect()){
        MYSQL_RES *res=mysql.query(sql);//申请一块内存，存储sql的结果，底层调用嵌入式sql
        if(res!=nullptr){
            MYSQL_ROW row;
            //把userid用户的所有离线消息放入vector中并返回
            //以行的方式去一行行读取当前的sql结果
            while((row=mysql_fetch_row(res))!=nullptr){
                vec.push_back(row[0]);
            }
            mysql_free_result(res);
            return vec;
        }      
    }
    return vec;
}