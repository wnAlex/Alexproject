#include"Timestamp.h"
#include<sys/time.h>
#include<stdint.h>
#include<inttypes.h>

Timestamp::Timestamp():microSecondsSinceEpoch_(0){

}
Timestamp::Timestamp(int64_t microSecondsSinceEpoch):microSecondsSinceEpoch_(microSecondsSinceEpoch){

}
//获取当前系统时间，将当前时间转换为微秒并返回
Timestamp Timestamp::now(){
    struct timeval tv;
    gettimeofday(&tv, NULL);
    int64_t seconds = tv.tv_sec;
    Timestamp t(seconds * kMicroSecondsPerSecond + tv.tv_usec);
    return t;

}
std::string Timestamp::toString()const{
    char buf[128]={0};
    int64_t seconds = microSecondsSinceEpoch_ /kMicroSecondsPerSecond;
    tm* tm_time= localtime(&seconds);
    snprintf(buf,128,"%4d/%02d/%02d %02d:%02d:%02d",tm_time->tm_year+1900,tm_time->tm_mon+1,
    tm_time->tm_mday,tm_time->tm_hour,tm_time->tm_min,tm_time->tm_sec);
    return buf;
}



//  #include<iostream>
//  int main(){
//      std::cout<<Timestamp::now().toString()<<std::endl;
//      return 0;
//  }