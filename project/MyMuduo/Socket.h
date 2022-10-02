#pragma once

#include"noncopyable.h"


class InetAddress;

//scoket类只是对于系统调用socket()返回的socketfd进行一个封装
class Socket:noncopyable {
public:

	explicit Socket(int sockfd) 
		:sockfd_(sockfd)
	{

	}

	~Socket();//内部应该要close(fd)

	int fd()const { return sockfd_; }
	void bindAddress(const InetAddress& localaddr);
	void listen();
	int accept(InetAddress* peeraddr);

	void shutdownWrite();

	//更改socket相关设置的函数
	void setTcpNoDelay(bool on);
	void setReuseAddr(bool on);
	void setReusePort(bool on);
	void setKeepAlive(bool on);

private:
	const int sockfd_;
};