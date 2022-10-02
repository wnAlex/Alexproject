#pragma once

#include<unistd.h>
#include<sys/syscall.h>
namespace CurrentThread
{
    //t_cachedTid是全局变量，应该是线程共享的 但_thread关键字会让多线程各自拷贝一份，各线程独享全局变量
    extern __thread int t_cachaedTid;

    void cacheTid();

    inline int tid(){
        if(__builtin_expect(t_cachaedTid==0,0)){
            cacheTid();
        }
        return t_cachaedTid;
    }


}