#pragma once

/// +-------------------+------------------+------------------+
/// | prependable bytes |  readable bytes  |  writable bytes  |
/// |                   |     (CONTENT)    |                  |
/// +-------------------+------------------+------------------+
/// |                   |                  |                  |
/// 0      <=      readerIndex   <=   writerIndex    <=     size


#include<vector>
#include<string>
#include<algorithm>

//网络库底层的缓冲区定义
/*buffer要完成中间层的功能：
    retrieveAllAsString()是应用来调用的，将数据从buffer->应用中
    readFd()是muduo库内部调用的，将数据从fd->buffer中
*/
class Buffer {
public:
    
    static const size_t kCheapPrepend=8;//用来记录数据包的大小
    static const size_t kInitialSize=1024;//缓冲区的数据段大小

    explicit Buffer(size_t initialSize=kInitialSize)
        :buffer_(kCheapPrepend+initialSize)
        ,readerIndex_(kCheapPrepend)
        ,writeIndex_(kCheapPrepend)
    {

    }

    size_t readableBytes()const{
        return writeIndex_-readerIndex_;//缓冲区可读的数据长度
    }

    size_t writeableBytes()const{
        return buffer_.size()-writeIndex_;//缓冲区可写入数据的空闲长度(可以从fd的缓冲区读多少数据写过来)
    }

    size_t prependableBytes()const{
        return readerIndex_;//prependable区域大小
    }

    //返回缓冲区中可读数据的起始地址
    const char *peek()const {
        return begin()+readerIndex_;
    }

    //retrieve(检索)
    void retrieve (size_t len){
        if(len<readableBytes()){//提供了很多其他接口，可能一次并为将数据读完
            readerIndex_+=len;//应用只读取了可读缓冲区的len长度内容
        }else{//一次性将可读数据全部读完
            retrieveAll();
        }
    }

    void retrieveAll(){
        readerIndex_=kCheapPrepend;
        writeIndex_=kCheapPrepend;
    }

    //onMessage函数中调用 retrieveallasstring:将buffer中内容转为string类型，然后交给应用处理
    std::string retrieveAllAsString(){
        return retrieveAsString(readableBytes());//应用可读取的数据长度
    }

    std::string retrieveAsString(size_t len){
        std::string result(peek(),len);
        retrieve(len);//result已经将缓冲区可读数据读完了，下面要进行缓冲区复位操作
        return result;
    }

    void ensureWriteableBytes(size_t len){
        if(writeableBytes()<len){
            makeSpace(len);//如果当前缓冲区可以写入的长度小于sockfd想写入的长度，则缓冲区扩容，如果大于等于，就直接写
        }
    }

    //应用向缓冲区写数据或fd向缓冲区写数据
    //把[data,data+len]内存上的数据，添加到writeable缓冲区当中
    void append(const char*data,size_t len){
        ensureWriteableBytes(len);//确保缓冲区空间够
        std::copy(data,data+len,beginWrite());//将data写入到缓冲区中
        writeIndex_+=len;//更新writeIndex_
    }


    char* beginWrite(){
        return begin()+writeIndex_;
    }

    const char *beginWrite()const{
        return begin()+writeIndex_;
    }

    //从fd上读取数据到缓冲区
    ssize_t readFd(int fd,int *savedErrno);
    //向fd发送数据
    ssize_t writeFd(int fd,int *savedErrno);

private:

    char *begin(){
        return &*buffer_.begin();//vector底层数组首元素的地址，也就是数组的起始地址
    }
    
    const char*begin()const {
        return &*buffer_.begin();
    }

    void makeSpace(size_t len){
        if(writeableBytes()+prependableBytes()<len+kCheapPrepend){//可写区域加上buffer前面数据已经被读走的作废区域一起的总长度都不够->只能扩容
            buffer_.resize(writeIndex_+len);
        }else{//总的空闲区域可以满足要求，需要进行内存碎片整理
            size_t readable=readableBytes();
            std::copy(begin()+readerIndex_,begin()+writeIndex_,begin()+kCheapPrepend);
            readerIndex_=kCheapPrepend;
            writeIndex_=readerIndex_+readable;

        }
    }

    std::vector<char>buffer_;//socket读取数据需要的参数都是裸指针，这里面用vector是为了方便缓冲区扩容
    size_t readerIndex_;
    size_t writeIndex_;
};