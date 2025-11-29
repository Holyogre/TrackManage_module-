#pragma once
#include <string>

class Logger {
public:
    virtual void debug(const std::string& msg) = 0;
    virtual void info(const std::string& msg) = 0;
    virtual void error(const std::string& msg) = 0;
    // ...其他日志级别
    static Logger* getInstance(); // 工厂方法
};

// 条件编译开关：是否启用日志 △
#ifdef ENABLE_SPDLOG
    #define LOG_DEBUG(msg) Logger::getInstance()->debug(msg)
    #define LOG_INFO(msg)  Logger::getInstance()->info(msg)
    #define LOG_ERROR(msg) Logger::getInstance()->error(msg)
#else
    #define LOG_DEBUG(msg)
    #define LOG_INFO(msg)
    #define LOG_ERROR(msg)
#endif
