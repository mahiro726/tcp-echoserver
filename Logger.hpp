#ifndef __LOGGER_HPP
#define __LOGGER_HPP

#include <iostream>
#include <cstdio>
#include <string>
#include <sys/time.h>
#include <ctime>
#include <memory>
#include <unistd.h>
#include <filesystem> // c++17
#include <fstream>
#include <sstream>
#include "Mutex.hpp"

namespace NS_LOG_MODULE
{
    enum class LogLevel
    {
        INFO,
        WARNING,
        ERROR,
        FATAL,
        DEBUG
    };

    std::string LogLevel2Message(LogLevel level)
    {
        switch (level)
        {
        case LogLevel::INFO:
            return "Info";
        case LogLevel::WARNING:
            return "Warning";
        case LogLevel::ERROR:
            return "Error";
        case LogLevel::FATAL:
            return "Fatal";
        case LogLevel::DEBUG:
            return "Debug";
        default:
            return "Unknown";
        }
    }

    // 1. 时间戳  2. 日期 + 时间
    std::string GetCurrentTime()
    {
        struct timeval current_time;
        int n = gettimeofday(&current_time, nullptr);
        (void)n;

        struct tm struct_time;
        localtime_r(&(current_time.tv_sec), &struct_time); // r：可重入函数

        char timestr[128];
        snprintf(timestr, sizeof(timestr), "%04d-%02d-%02d %02d:%02d:%02d",
                 struct_time.tm_year + 1900,
                 struct_time.tm_mon + 1,
                 struct_time.tm_mday,
                 struct_time.tm_hour,
                 struct_time.tm_min,
                 struct_time.tm_sec);

        return timestr;
    }

    // 输出角度 -- 刷新策略
    // 1. 显示器打印
    // 2. 文件写入

    // 日志的生成：
    // 1. 构建日志字符串
    // 2. 根据不同的策略，进行刷新

    // 策略模式，策略接口
    class LogStrategy
    {
    public:
        virtual ~LogStrategy() = default;
        virtual void SyncLog(const std::string &message) = 0;
    };

    // 显示器策略日志
    class ConsoleStrategy : public LogStrategy
    {
    public:
        void SyncLog(const std::string &message) override
        {
            LockGuard lockguard(_mutex);
            std::cerr << message << std::endl;
        }

        ~ConsoleStrategy()
        {
        }

    private:
        Mutex _mutex;
    };

    // 文件策略
    const std::string defaultpath = "./log";
    const std::string defaultfilename = "log.txt";

    class FileLogStrategy : public LogStrategy
    {
    public:
        FileLogStrategy(const std::string &path = defaultpath, const std::string &name = defaultfilename)
            : _logpath(path),
              _logfilename(name)
        {
            LockGuard lockguard(_mutex);
            if (std::filesystem::exists(_logpath))
                return;
            try
            {
                std::filesystem::create_directories(_logpath);
            }
            catch (const std::filesystem::filesystem_error &e)
            {
                std::cerr << e.what() << '\n';
            }
        }

        void SyncLog(const std::string &message) override
        {
            {
                LockGuard lockguard(_mutex);

                if (!_logpath.empty() && _logpath.back() != '/')
                {
                    _logpath += "/";
                }

                std::string targetlog = _logpath + _logfilename;
                std::ofstream out(targetlog, std::ios::app); // 追加方式写入

                if (!out.is_open())
                {
                    return;
                }

                out << message << "\n";
                out.close();
            }
        }

        ~FileLogStrategy()
        {
        }

    private:
        std::string _logpath;
        std::string _logfilename;
        Mutex _mutex;
    };

    // 日志的生成：
    // 1. 构建日志字符串
    // 2. 根据不同的策略，进行测试

    // 日志类：
    // 1. 日志的生成
    // 2. 根据不同的策略，进行刷新
    class Logger
    {
    public:
        Logger()
        {}

        void UseConsoleStrategy()
        {
            _strategy = std::make_unique<ConsoleStrategy>();
        }

        void UseFileLogStrategy()
        {
            _strategy = std::make_unique<FileLogStrategy>();
        }

        void Debug(const std::string &message)
        {
            if (_strategy != nullptr)
            {
                _strategy->SyncLog(message);
            }
        }

        // 内部类，标识一条完整的日志信息
        // 一条完整的日志信息 = 左半部分固定部分 + 右半部分不固定部分
        class LogMessage
        {
        public:
            LogMessage(LogLevel level, std::string &filename, int line, Logger &logger)
                : _level(level),
                  _curr_time(GetCurrentTime()),
                  _pid(getpid()),
                  _filename(filename),
                  _line(line),
                  _logger(logger)
            {
                // 先构建出来左半部分
                std::stringstream ss;
                ss << "[" << _curr_time << "] "
                   << "[" << LogLevel2Message(_level) << "] "
                   << "[" << _pid << "] "
                   << "[" << _filename << "] "
                   << "[" << _line << "]  - ";

                _loginfo = ss.str();
            }

            template <typename T>
            LogMessage &operator<<(const T &info)
            {
                std::stringstream ss;
                ss << info;
                _loginfo += ss.str();
                return *this; // 返回当前LogMessage对象，方便下次继续使用进行 <<
            }

            ~LogMessage()
            {
                if (_logger._strategy)
                {
                    _logger._strategy->SyncLog(_loginfo);
                }
            }

        private:
            LogLevel _level;
            std::string _curr_time;
            pid_t _pid;
            std::string _filename;
            int _line;
            std::string _loginfo; // 一条完整的日志信息

            // 一个引用，引用外部的Logger类对象
            Logger &_logger; // 方便我们后续进行策略式刷新
        };

        // 这里已经不是内部类了
        // 故意采用拷贝LogMessage
        LogMessage operator()(LogLevel level, std::string filename, int line)
        {
            return LogMessage(level, filename, line, *this);
        }

        ~Logger()
        {}

    private:
        std::unique_ptr<LogStrategy> _strategy; // 刷新策略
    };

    // 日志对象，全局使用
    Logger logger;

#define ENABLE_CONSOLE_LOG_STRATEGY() logger.UseConsoleStrategy()
#define ENABLE_FILE_LOG_STRATEGY() logger.UseFileLogStrategy()

#define LOG(level) logger(level, __FILE__, __LINE__)
}

#endif