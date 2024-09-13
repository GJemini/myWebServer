#include "buffer.h"

Buffer::Buffer(int initBuffSize): buffer_(initBuffSize), readPos_(0), writePos_(0) {}
//第一个buffer是class buffer；第二个buffer是构造函数；最后三个是private中成员函数，默认为整数0

//成员函数在h文件中声明，在cpp文件中写入；
//可写长度
size_t Buffer::WritableBytes() const{
    return buffer_.size() - writePos_;
}  

//可读长度：即已写入的长度。Buffer_分为prependable,readble,writable三部分
size_t Buffer::ReadableBytes() const{
    return writePos_ - readPos_;
}

//前置长度
size_t Buffer::PrependableBytes() const{
    return readPos_;
}

//找到readable开头处的指针
const char* Buffer::Peek() const{
    return BeginPtr_() + readPos_;
}

//确认可写：若可写长度不够，则扩容
void Buffer::EnsureWriteable(size_t len){
    if(WritableBytes() < len) {
        MakeSpace_(len); //对容器的操作（扩容 或者 拷贝移动数据）
    }
    assert(WritableBytes() >= len);
}

//已写入len长度：将writePos_后移len位，表示readable长度增长len
void Buffer::HasWritten(size_t len){
    writePos_ += len;
}

//取回len长度：将readPos_后移len位，表示这部分已经读取完毕，其内容变成predendable
void Buffer::Retrieve(size_t len){
    assert(len <= ReadableBytes());
    readPos_ += len;
}

//取回end指针之前的所有数：Peek()返回readable开头的指针；用retrive函数将readPos_后移end-peek位
void Buffer::RetrieveUntil(const char* end){
    assert(Peek() <= end );
    Retrieve(end - Peek()); //移动读指针
}

//清零：bzero(void *s,int n)可以将s指针后前n个字节清零
void Buffer::RetrieveAll(){
    bzero(&buffer_[0], buffer_.size()); 
    readPos_ = 0;
    writePos_ = 0;
} 

//找到writePos_对应的指针，即writable的开头；本函数为const版本，不可改变成员变量的值
const char* Buffer::BeginWriteConst() const{
    return BeginPtr_() + writePos_;
}
char* Buffer::BeginWrite(){
    return BeginPtr_() + writePos_;
}

//在确认可写后，将str中长度len的内容复制入writable，并将writePos_后移len位；
//重载函数：len的长度即为str总长度
void Buffer::Append(const std::string& str){
    Append(str.data(), str.length());//str.data()将str转换为数组，并返回其指针
}
//此处第一个参数为char*，这是cpp与c兼容的规范化写法（c不支持string作为参数）
void Buffer::Append(const char* str, size_t len){
    assert(str);
    EnsureWriteable(len); //这里的len是临时数组中的数据个数
    std::copy(str, str + len, BeginWrite());//std::copy(start,end,container)
    HasWritten(len);
}

/*
size_t类型一般用于计数。表示无符号整数类型；ssize_t类型表示可以被执行读写操作的数据块大小
函数作用：缓冲区的只读接口；本代码中缓冲区的writeFd被注释了；本函数使用readv()函数实现
readv()函数可以实现分散输入：从fd所指文件中读取一串连续字符，分散到iov指定的缓冲区中。
用readv()读取数据，iovec有两块，第一块指向writable，第二块指向buff栈区；如果数据太长，readv就会
把数据分成两块，第一块放进writable，第二块放进栈区，然后再把buff的数据append进buffer
*/
ssize_t Buffer::ReadFd(int fd, int* saveErrno){
    char buff[65535]; //临时数组：栈区
    struct iovec iov[2]; //定义了一个向量元素  分散的内存；
    //iovec是描写缓冲区的结构体，为一个数组，数组元素有两个指针成员iov_base和iov_len
    const size_t writable = WritableBytes();
    /* 分散读， 保证数据全部读完 */ 
    iov[0].iov_base = BeginPtr_() + writePos_;//iove_base指向缓冲区的writable
    iov[0].iov_len = writable; //writable长度
    iov[1].iov_base = buff;//栈区
    iov[1].iov_len = sizeof(buff);

    const ssize_t len = readv(fd, iov, 2); 
    /*ssize_t readv(int fd, const struct iovec *iov, int iovcnt);
    将数据从fd文件读到分散的内存块中；与之对应的是writev，集中写，将分散的内存一并写入fd中
    返回读到的总字节数
    */
    if(len < 0) {
        *saveErrno = errno;
    }
    //static_cast是类型转换运算符。不同于强制转换，编译器不会对这种转换警告
    else if(static_cast<size_t>(len) <= writable) {
        writePos_ += len;
    }
    else {
        writePos_ = buffer_.size();
        Append(buff, len - writable);//把剩下的数据做处理（继续放在当前容器里或者是扩容）
    }

    return len;
}

// ssize_t Buffer::WriteFd(int fd, int* saveErrno){
// }

char* Buffer::BeginPtr_() {
    return &*buffer_.begin();//写成&*是先解引用再取地址，目的是保证不会发生未定义的行为
}
//为什么要重载一个const版本？如果声明了一个const Buffer a，那么调用a.BeginPtr_()就调用const版本
const char* Buffer::BeginPtr_() const {
    return &*buffer_.begin();
}

void Buffer::MakeSpace_(size_t len) {
    if(WritableBytes() + PrependableBytes() < len) { //剩余可写的大小 加 前面可用的空间 小于 临时数组中的长度
        buffer_.resize(writePos_ + len + 1);
    } 
    else { //可以装len长度的数据 就直接将后面的数据拷贝到前面
        size_t readable = ReadableBytes(); 
        std::copy(BeginPtr_() + readPos_, BeginPtr_() + writePos_, BeginPtr_());
        readPos_ = 0; 
        writePos_ = readPos_ + readable; 
        assert(readable == ReadableBytes());
    }
}

/*
非阻塞IO缓冲区：若输入数据长度超过buffer的writable：
ReadFd发现长度超出；引用Append(buff, len - writable)，将buff中暂存的len-writable位移入buffer；append会先ensurewritable再写入，
而ensurewritable会makespace确认可写入。makespace会把writable的内容移到最前面，从而使append可以把buff的内容移到后面
*/
