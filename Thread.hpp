#pragma once

#include <iostream>
#include <pthread.h>
#include <functional>

using callback_t = std::function<void()>;


static int gnumber = 1;
enum class TSTATUS
{
    THREAD_NEW,
    THREAD_RUNNING,
    THREAD_STOP
};

class Thread
{
private:
    void ToRunning()
    {
        _status = TSTATUS::THREAD_RUNNING;
    }

    void ToStop()
    {
        _status = TSTATUS::THREAD_STOP;
    }

    static void *ThreadRoutine(void *args) // 线程执行函数
    {
        Thread *self = static_cast<Thread *>(args);          // 把封装的线程递给线程执行函数，让线程可以执行类中的成员函数，并且访问自己的属性
        self->ToRunning();
        pthread_setname_np(self->_tid, self->_name.c_str()); // 设置 ps -a 中线程的名字
        self->_cb();
        self->ToStop(); // 线程跑完任务进入STOP状态
        return nullptr;
    }

public:
    Thread(callback_t cb)
        : _tid(-1),
          _status(TSTATUS::THREAD_NEW),
          _joinable(true),
          _cb(cb)
    {
        _name = "newthread-" + std::to_string(gnumber++); // 嗯……，这里有没有线程安全问题，也就是并发问题
    }

    bool Start()
    {
        int n = pthread_create(&_tid, nullptr, ThreadRoutine, this);
        if (n != 0)
        {
            return false;
        }
        return true;
    }

    void Join()
    {
        // 检查条件
        if (_joinable)
        {
            pthread_join(_tid, nullptr);
            _status = TSTATUS::THREAD_STOP;
        }
        else
        {
            std::cerr << _name << "join失败，不满足join条件" << std::endl;
        }
    }

    void Stop()
    {
        // 前置校验
        if (_status == TSTATUS::THREAD_RUNNING)
        {
            // 实际中止线程
            pthread_cancel(_tid);
            // 改状态
            _status = TSTATUS::THREAD_STOP;
        }
        else
        {
            std::cout << _name << "中止失败，不满足中止条件" << std::endl;
        }
    }

    void Detach()
    {
        // 前置检验
        if (_tid != -1 && _joinable == true) // _status是异步的，而_tid才是同步的
        {
            // pthread_detach
            pthread_detach(_tid);

            // 设置joinable
            _joinable = false;
        }
        else
        {
            std::cerr << _name << "detach失败，detach条件不满足" << std::endl;
        }
    }

    const char *StatusStr() const
    {
        switch (_status)
        {
        case TSTATUS::THREAD_NEW:
            return "new";
        case TSTATUS::THREAD_RUNNING:
            return "running";
        case TSTATUS::THREAD_STOP:
            return "stop";
        default:
            return "unknown";
        }
    }

    const char *JoinableStr() const
    {
        return _joinable ? "true" : "false";
    }

    void PrintInfo() const
    {
        std::cout << "_name: " << _name << std::endl;
        std::cout << "_tid: " << _tid << std::endl; 
        std::cout << "_status: " << StatusStr() << std::endl;
        std::cout << "_joinable: " << JoinableStr() << std::endl;
    }

    ~Thread()
    {
    }

private:
    // 每个线程需要名字，方便人来看
    // 每个线程需要tid值，方便传入函数控制
    // 需要标识线程状态，也是方便线程控制
    // 标识是否被Dettach，以及是否还能join
    // 线程要有专门执行的任务，即回调函数
    std::string _name;
    pthread_t _tid;
    TSTATUS _status;
    bool _joinable;
    callback_t _cb;
};