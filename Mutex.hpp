#pragma once
#include <pthread.h>

class Mutex
{
public:
    Mutex()
    {
        pthread_mutex_init(&_lock, nullptr);
    }

    void Lock()
    {
        pthread_mutex_lock(&_lock);
    }

    void Unlock()
    {
        pthread_mutex_unlock(&_lock);
    }

    pthread_mutex_t* Ptr()
    {
        return &_lock;
    }

    ~Mutex()
    {
        pthread_mutex_destroy(&_lock);
    }
private:
    pthread_mutex_t _lock;
};

class LockGuard
{
public:
    LockGuard(Mutex& lock)
    :_lockref(lock)
    {
        _lockref.Lock();
    }

    ~LockGuard()
    {
        _lockref.Unlock();
    }
private:
    Mutex &_lockref;
};