#include"db.hpp"
// 数据库服务器端配置信息
static string server = "127.0.0.1";
static string user = "root";
static string password = "nanshen123";
static string dbname = "chat";

MySQL::MySQL()
{
    _conn = mysql_init(nullptr);
}
// 释放数据库连接资源
MySQL::~MySQL()
{
    if (_conn != nullptr)
        mysql_close(_conn);
}
// 连接数据库
bool MySQL::connect()
{
    MYSQL *p = mysql_real_connect(_conn, server.c_str(), user.c_str(), password.c_str(), dbname.c_str(), 3306, nullptr, 0);
    if (p != nullptr)
    {   
        //设置client -utf8，如果不设置，从Mysql上拉下来的中文显示乱码，因为mysql server已经设置为utf8了  页面utf8----client utf8 ----server utf8
        mysql_query(_conn, "set names utf8");
        LOG_INFO<<"connect mysql success!";
    }else{
        LOG_INFO<<"connect mysql fail!";
    }
    return p;
}
// 更新操作
bool MySQL::update(string sql)
{
    if (mysql_query(_conn, sql.c_str()))
    {
        LOG_INFO << __FILE__ << ":" << __LINE__ << ":" << sql << "更新失败!";
        LOG_INFO<<mysql_errno(_conn);
        return false;
    }
    return true;
}
// 查询操作
MYSQL_RES *MySQL::query(string sql)
{
    if (mysql_query(_conn, sql.c_str()))
    {
        LOG_INFO << __FILE__ << ":" << __LINE__ << ":" << sql << "查询失败!";
        return nullptr;
    }
    return mysql_use_result(_conn);
}

//获取连接
MYSQL*MySQL::getConnection(){
    return _conn;
}
