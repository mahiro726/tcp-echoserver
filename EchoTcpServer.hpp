#pragma once

#include <iostream>
#include "Logger.hpp"
#include "InetAddr.hpp"
#include <strings.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include "ThreadPool.hpp"
#include <functional>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h> // 网络三件套

using namespace NS_LOG_MODULE;


using handler_t = std::function<std::string(const std::string&)>; // 处理数据的函数的类型
using task_t = std::function<void()>; // 给线程池的任务类型

enum
{
    SUCCESS,
    SOCKET_ERROR,
    BIND_ERROR,
    LISTEN_ERROR,
    USAGE_ERROR,
    FORK_ERROR
};

const uint16_t gport = 8080;
const int gbacklog = 16;

class TcpServer
{
private:
    struct ThreadData
    {
        TcpServer *_self;
        int _sockfd;
        InetAddr _clientaddress;

        ThreadData(TcpServer *self, int sockfd, const InetAddr clientaddress)
            : _self(self),
              _sockfd(sockfd),
              _clientaddress(clientaddress)
        {
        }

        ~ThreadData()
        {
        }
    };

    static void *ThreadRoutine(void *args) // pthread_create 对于参数有要求，所以只能是静态入口
    {
        // 主线程不需要再来回收
        pthread_detach(pthread_self());

        ThreadData *td = static_cast<ThreadData *>(args);

        td->_self->serverIO(td->_sockfd, td->_clientaddress);
        delete (td);

        return nullptr;
    }

public:
    TcpServer(uint16_t port = gport)
        : _port(port),
          _listensockfd(-1)
    {
    }

    void RegisterService(const handler_t& handler)
    {
        _handler = handler;
    }

    void Init()
    {
        _listensockfd = socket(AF_INET, SOCK_STREAM, 0); // TCP
        if (_listensockfd < 0)
        {
            LOG(LogLevel::FATAL) << "创建 socket 失败";
            exit(SOCKET_ERROR);
        }
        LOG(LogLevel::DEBUG) << "服务端创建 socket 成功";

        // 填充网络信息
        struct sockaddr_in local;
        bzero(&local, sizeof(local));
        local.sin_family = AF_INET;
        local.sin_port = htons(_port);
        local.sin_addr.s_addr = INADDR_ANY; // 任意ip

        // bind
        socklen_t len = sizeof(local);
        int n = bind(_listensockfd, (struct sockaddr *)&local, len);
        if (n < 0)
        {
            LOG(LogLevel::FATAL) << "服务端 bind 失败";
            exit(BIND_ERROR);
        }
        LOG(LogLevel::DEBUG) << "服务端 bind 成功";

        // tcp是面向连接的，所以tcp服务器一般要处于监听状态
        int m = listen(_listensockfd, gbacklog);
        if (m < 0)
        {
            LOG(LogLevel::FATAL) << "服务端 listen 失败";
            exit(LISTEN_ERROR);
        }
        LOG(LogLevel::DEBUG) << "服务端 listen 成功";
    }

    void serverIO(int sockfd, InetAddr clientaddress)
    {
        LOG(LogLevel::DEBUG)
        << "client info is: " << clientaddress.ToString();

        // 短链接，短服务   长服务的话仅仅只是加了一个循环
        // 读
        char inbuffer[1024];
        ssize_t n = read(sockfd, inbuffer, sizeof(inbuffer) - 1);

        if (n > 0)
        {
            inbuffer[n] = 0;
            LOG(LogLevel::DEBUG) << "client say# " << inbuffer;

            // 我们将回显改为处理数据，处理数据的函数由上层提供
            std::string request(inbuffer, n);
            // 在调用处理函数之前，可以判断一下函数是否存在，不存在就直接返回了
            // 这里省略
            std::string result = _handler(request);
            
            ssize_t m = write(sockfd, result.c_str(), result.size());
            if (m < 0)
            {
                LOG(LogLevel::ERROR) << "服务端 write 失败";
            }

            // // 写
            // std::string echo_string = "server echo# ";
            // echo_string += inbuffer;
            // ssize_t m = write(sockfd, echo_string.c_str(), echo_string.size());
            // if (m < 0)
            // {
            //     LOG(LogLevel::ERROR) << "服务端 write 失败";
            // }
        }
        else if (n == 0)
        {
            LOG(LogLevel::INFO) << "client quit, address: " << clientaddress.ToString();
        }
        else
        {
            LOG(LogLevel::WARNING) << "reat client error, address: " << clientaddress.ToString();
        }

        close(sockfd);
    }

    void Start()
    {
        while (true)
        {
            struct sockaddr_in clientaddr;
            socklen_t len = sizeof(clientaddr);

            // 获取连接
            int sockfd = accept(_listensockfd, (struct sockaddr *)&clientaddr, &len);
            if (sockfd < 0)
            {
                LOG(LogLevel::WARNING) << "accept 失败";
                continue;
            }
            LOG(LogLevel::DEBUG) << "accept success, sockfd: " << sockfd;

            // version3:线程池版本
            InetAddr clientaddress(clientaddr);
            
            auto tp = ThreadPool<task_t>::GetInstance();
            tp->Enqueue([this, sockfd, clientaddress](){
                this->serverIO(sockfd, clientaddress);
            });



#if 0
            // version2:多线程版本
            InetAddr clientaddress(clientaddr);
            ThreadData *td = new ThreadData(this, sockfd, clientaddress);
            pthread_t tid;
            int n = pthread_create(&tid, nullptr, ThreadRoutine, (void *)td);
            if (n != 0)
            {
                LOG(LogLevel::WARNING) << "线程创建失败";

                // 既然线程创建已经失败，那就没有人来接手这两项资源，只能释放掉，然后等下一轮
                delete (td);
                close(sockfd);
                continue;
            }
            LOG(LogLevel::DEBUG) << "线程创建成功,tid: " << tid;


            // version1:多进程版本处理
            pid_t id = fork();
            if (id < 0)
            {
                LOG(LogLevel::FATAL) << "fork error";
                exit(FORK_ERROR);
            }
            else if (id == 0)
            {   // 子进程
                close(_listensockfd); // 子进程和孙子进程都不需要门店，所以直接关掉
                if (fork() > 0) // 创建孙子进程，一旦创建成功就直接退出
                {
                    exit(0);
                }
                
                // 为什么要创建孙子进程呢，因为如果由子进程来执行IO，那父进程就要一直等着子进程，无法重新去获客，没有达到目的
                // 孙子进程 - 孤儿进程
                InetAddr clientaddress(clientaddr);
                serverIO(sockfd, clientaddress); // serverIO内置了关闭文件描述符的操作
                // 处理完后就要关闭文件描述符，且需要退出
                exit(0);
            }
            else
            {
                // 父进程
                // 文件描述符本身是一种资源，如果不用就要关掉
                close(sockfd);
                // 还需要回收子进程
                pid_t rid = waitpid(id, nullptr, 0);
                (void) rid;
            }
#endif
        }
    }

    ~TcpServer() // 析构函数这里应当关闭监听套接字
    {
        close(_listensockfd);
    }

private:
    uint16_t _port;
    // 不需要显示包含ip
    int _listensockfd;
    handler_t _handler;
};