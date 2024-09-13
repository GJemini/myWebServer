#ifndef LOG_H
#define LOG_H

#include <mutex>
#include <string>
#include <thread>
#include <sys/time.h>
#include <string.h>
#include <stdarg.h>           // vastart va_end
#include <assert.h>
#include <sys/stat.h>         // mkdir
#include "blockqueue.h"
#include "../buffer/buffer.h"

class Log {
public:
    // 初始化日志实例（阻塞队列最大容量、日志保存路径、日志文件后缀）
    void init(int level, const char* path = "./log", 
                const char* suffix =".log",
                int maxQueueCapacity = 1024);

    static Log* Instance();
    static void FlushLogThread();   // 异步写日志公有方法，调用私有方法asyncWrite
    
    void write(int level, const char *format,...);  // 将输出内容按照标准格式整理
    void flush();

    int GetLevel();
    void SetLevel(int level);
    bool IsOpen() { return isOpen_; }
    
private:
    Log();
    void AppendLogLevelTitle_(int level);
    virtual ~Log();
    /*
    virtual关键字表示虚函数，可以实现运行时多态。
    即~Log()可以封装多种解析函数，并在调用时选择用哪一个;
    可以理解为是一个指向函数的指针。
    */
    void AsyncWrite_(); // 异步写日志方法

private:
    static const int LOG_PATH_LEN = 256;    // 日志文件最长文件名
    static const int LOG_NAME_LEN = 256;    // 日志最长名字
    static const int MAX_LINES = 50000;     // 日志文件内的最长日志条数

    const char* path_;          //路径名
    const char* suffix_;        //后缀名

    int MAX_LINES_;             // 最大日志行数

    int lineCount_;             //日志行数记录
    int toDay_;                 //按当天日期区分文件

    bool isOpen_;               
 
    Buffer buff_;       // 输出的内容，缓冲区
    int level_;         // 日志等级
    bool isAsync_;      // 是否开启异步日志

    FILE* fp_;                                          //打开log的文件指针
    std::unique_ptr<BlockQueue<std::string>> deque_;    //阻塞队列
    /*
    std::unique_ptr为c++11引入的独占式指针，对资源有专属所有权；
    同一时刻，只能有一个unique_ptr指向这个对象；当指针销毁，对象也会销毁；不支持赋值和拷贝操作；
    一旦指针放弃对所指堆内存空间的所有权，则该空间会立刻释放回收。
    */
    std::unique_ptr<std::thread> writeThread_;          //写线程的指针
    std::mutex mtx_;                                    //同步日志必需的互斥量
};

#define LOG_BASE(level, format, ...) \
    do {\
        Log* log = Log::Instance();\
        if (log->IsOpen() && log->GetLevel() <= level) {\
            log->write(level, format, ##__VA_ARGS__); \
            log->flush();\
        }\
    } while(0);
/*
\为继续符，表示本行与下一行连接起来。
->用于结构体指针变量访问成员，而.用于结构体变量访问成员。
为什么要do while(0)？因为宏对应代码是直接替换的，容易导致语法、替换等问题，
用do while(0)封装比较复杂的宏代码，可以避免问题。
*/

// 四个宏定义，主要用于不同类型的日志输出，也是外部使用日志的接口
// ...表示可变参数，__VA_ARGS__是编译器预定义的宏，可以将...的值复制到这里
// 前面加上##的作用是：当可变参数的个数为0时，这里的##可以把把前面多余的","去掉,否则会编译出错。
#define LOG_DEBUG(format, ...) do {LOG_BASE(0, format, ##__VA_ARGS__)} while(0);    
#define LOG_INFO(format, ...) do {LOG_BASE(1, format, ##__VA_ARGS__)} while(0);
#define LOG_WARN(format, ...) do {LOG_BASE(2, format, ##__VA_ARGS__)} while(0);
#define LOG_ERROR(format, ...) do {LOG_BASE(3, format, ##__VA_ARGS__)} while(0);

#endif //LOG_H
