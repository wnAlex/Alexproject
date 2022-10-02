#include"CurrentThread.h"


namespace CurrentThread
{
    __thread int t_cachaedTid=0;

    void cacheTid(){
        if(t_cachaedTid==0){
            //通过linux系统调用，获取当前线程的tid值  tid:ture id  线程的真实id值，独立于进程的值
            t_cachaedTid=static_cast<pid_t>(::syscall(SYS_gettid));
        }
    }
}
