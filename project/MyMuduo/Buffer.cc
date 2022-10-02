#include"Buffer.h"

#include<errno.h>
#include<sys/uio.h>
#include<unistd.h>

//从fd上读取数据到缓冲区 Poller工作在LT模式，当fd有数据会一直返回
//buffer缓冲区是有大小的，但是从fd上读取数据的时候，却不知道tcp(流式数据)数据的最终大小，可能会超过缓冲区大小
//readv系统调用可以从fd读数据，然后自动填充多个缓冲区
ssize_t Buffer::readFd(int fd,int *savedErrno){
    char extrabuf[65536]={0};//开辟在栈上的缓冲区 64k
    struct iovec vec[2];
    const size_t writable=writeableBytes();//这是buffer剩余可写空间大小
    
    //第一块缓冲区
    vec[0].iov_base=begin()+writeIndex_;
    vec[0].iov_len=writable;

    //第二块缓冲区
    vec[1].iov_base=extrabuf;
    vec[1].iov_len=sizeof extrabuf;

    const int iovcnt=(writable<sizeof extrabuf)?2:1;//如果buffer的剩余可写空间大于64k,则不需要使用栈上空间，因为一个ipv4报文最大就64k
    const ssize_t n=::readv(fd,vec,iovcnt);
    if(n<0){
        *savedErrno=errno;
    }else if(n<=writable){
        //buffer自身空间就够用
        writeIndex_+=n;
    }else{
        //extrabuf中也写入了数据
        writeIndex_=buffer_.size();
        append(extrabuf,n-writable);//从writeIndex_开始写n-writable大小的数据
    }
    return n;
}

ssize_t Buffer::writeFd(int fd,int *savedErrno){
    ssize_t n=::write(fd,peek(),readableBytes());
    if(n<0){
        *savedErrno=errno;
    }
    return n;
}

