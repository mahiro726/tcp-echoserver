#pragma once

#include <iostream>
#include <queue>
#include <vector>
#include <functional>
#include <unistd.h>
#include "Logger.hpp"
#include "Thread.hpp"
#include "Mutex.hpp"
#include "Cond.hpp"

const int defaultnum = 10;

using namespace NS_LOG_MODULE;

template <class T>
class ThreadPool
{
private:
    ThreadPool(int num = defaultnum)
        : _isrunning(false),
          _slaver_sleep_count(0),
          _num(num)
    {
        for (int i = 0; i < _num; i++)
        {
            _slavers.emplace_back([this]()
                                  { this->HandlerTask(); }); // 这里用lambda改造
        }
    }

    ThreadPool(const ThreadPool<T> &tp) = delete;

    ThreadPool<T> &operator=(const ThreadPool<T> &tp) = delete;

    void HandlerTask() // 子线程执行的函数
    {
        char name[64];
        pthread_getname_np(pthread_self(), name, sizeof(name));
        while (true)
        {
            T task;
            {
                // 加锁保护临界区
                LockGuard lockguard(_mutex);
                // 检测任务
                while (_tasks.empty() && _isrunning) // 线程池还在运行的时候才需要回去重睡
                {
                    // 没有任务，休眠，记录的休眠数量需要增加
                    _slaver_sleep_count++;
                    _cond.Wait(_mutex);
                    // 唤醒后又要--
                    _slaver_sleep_count--;
                }

                // 加退出机制，判条件：线程池已经没有run了
                if (!_isrunning && _tasks.empty()) // 任务已经为空的时候，才能退出，如果还有的话，就先把任务完成
                {

                    break; // 因为这个函数本质是在ThreadRoutine中调用的，所以只要break退出这个线程就会置为stop
                }

                // 有任务，取任务，本质：把任务由公共变成私有
                task = _tasks.front();
                _tasks.pop();
            }

            // 处理任务时，如果也加锁，那不就是单线程了吗
            LOG(LogLevel::INFO) << name << "处理任务";
            task();
        }
    }

public:
    static ThreadPool<T> *GetInstance()
    {
        if (_instance == nullptr) // 避免重复拿锁降低效率
        {
            LockGuard lockguard(_lock);
            if (_instance == nullptr)
            {
                _instance = new ThreadPool<T>();
                _instance->Start(); // 创建即启动，外部拿到就能用
                LOG(LogLevel::INFO) << "第一次线程池对象创建";
            }
        }

        return _instance;
    }

    void Start()
    {
        if (_isrunning)
        {
            LOG(LogLevel::WARNING) << "线程已经在运行，不满足条件";
            return;
        }
        else
        {
            _isrunning = true;
            for (Thread &slave : _slavers) // 要引用才拿到的是本体
            {
                slave.Start();
            }
        }
    }

    void Wait()
    {
        for (auto &slave : _slavers)
        {
            slave.Join();
        }
    }

    void Stop()
    {
        // version 2
        // 1. _isrunning = false
        // 2. 处理完成tasks所有的任务，让所有线程全部唤醒
        LockGuard lockguard(_mutex);
        if (!_isrunning)
        {
            LOG(LogLevel::WARNING) << "线程池没在运行，不满足条件，请勿重复调用";
            return;
        }

        _isrunning = false; // 这里写的_isrunning是共有的数据，会有并发问题，所以需要加锁保护
        _cond.Broadcast();  // 唤醒全部线程，本质是将停止线程的任务交给了HandlerTask
    }

    void Enqueue(const T &in)
    {
        _mutex.Lock();

        _tasks.push(in);             // 入任务
        if (_slaver_sleep_count > 0) // 来活了，自然要唤醒线程
        {
            _cond.Signal(); // 问题：这里的唤醒是随即唤醒一个线程吗？也就是如果有3个休眠线程然后会随机唤醒一个？
        }

        _mutex.Unlock();
    }

    ~ThreadPool()
    {
    }

private:
    // 需求分析
    // 标识线程池状态，线程池在跑吗？
    // 工作线程用容器管理
    // 给线程派发的任务用容器管理
    // 锁
    // 条件变量
    // 休眠线程数量
    // 线程总数
    bool _isrunning;
    std::vector<Thread> _slavers;
    std::queue<T> _tasks;
    Mutex _mutex;
    Cond _cond;
    int _slaver_sleep_count;
    int _num;

    // 添加单例模式
    static ThreadPool<T> *_instance;
    static Mutex _lock;
};

template <typename T>
ThreadPool<T> *ThreadPool<T>::_instance = nullptr;

template <typename T>
Mutex ThreadPool<T>::_lock;