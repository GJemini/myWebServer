#ifndef BUFFER_H
#define BUFFER_H

#include <cstring>  
#include <iostream>
#include <unistd.h>  
#include <sys/uio.h> 
#include <vector> 
#include <atomic>
#include <assert.h>

class Buffer {
public:
    Buffer(int initBuffSize = 1024);//构造函数
    ~Buffer() = default; //析构函数

    size_t WritableBytes() const; //可写的字节数；成员函数后加const为常函数，常函数不可修改成员属性     
    size_t ReadableBytes() const ; //可读的字节数 
    size_t PrependableBytes() const; //前面可以用的空间

    const char* Peek() const;//peek的意思是不会真正读取走流中的字符，而只是“看一眼”
    void EnsureWriteable(size_t len);
    void HasWritten(size_t len);

    void Retrieve(size_t len);//取回
    void RetrieveUntil(const char* end);

    void RetrieveAll() ; 

    const char* BeginWriteConst() const;
    char* BeginWrite();

    void Append(const std::string& str);
    void Append(const char* str, size_t len);

    ssize_t ReadFd(int fd, int* Errno);
    //fd:文件描述符,file descriptor，是一个非负整数；errno：error number
    //当linux打开文件时，内核向进程返回一个文件描述符，后续read、write这个文件时，就用fd作为参数传入read、write函数
    //ssize_t WriteFd(int fd, int* Errno);

private:
    char* BeginPtr_(); 
    const char* BeginPtr_() const;
    //TODO：为什么要重载？
    void MakeSpace_(size_t len); 

    std::vector<char> buffer_; //具体装数据的vector
    std::atomic<std::size_t> readPos_;//std的原子变量，用于多线程编程，支持多种数据类型;
    std::atomic<std::size_t> writePos_; 
    //std::size_t 可以存放下理论上可能存在的对象的最大大小，该对象可以是任何类型，包括数组
    
    /*
    为什么要用atomic？
    因为普通的数据类型，在多线程场景是不安全的。
    如i++，本质上是两个操作：读取i的值，自加1并给i赋值；而在多线程中，这两个操作会被其他线程的执行打断。
    在代码段的上下加“Synchronized”关键字也可以实现线程安全，但资源开销大
    
    Buffer_结构：
    Buffer_从前到后依次为：prependable,readble,writable
    readPos_和writePos_本质上是buffer_的数组下标，
    readPos_是可读部分的开头，writePos_是可写部分的开头、可读部分的结尾
    */
};

#endif //BUFFER_H