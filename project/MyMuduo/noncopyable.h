#pragma once
/*
    派生类的默认构造/拷贝构造/赋值函数都会先调用基类对应函数
    这样写可以保证任何继承了noncopyable的类都可以正常的构造和析构，但是不可以进行拷贝构造和赋值操作
*/
class noncopyable{
public:
    noncopyable(const noncopyable&)=delete;
    noncopyable& operator=(const noncopyable&)=delete;


protected:
    noncopyable()=default;
    ~noncopyable()=default;
};