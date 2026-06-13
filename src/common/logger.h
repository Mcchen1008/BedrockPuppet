#pragma once

#include "types.h"
#include <string>
#include <fstream>
#include <mutex>
#include <iostream>
#include <sstream>

namespace puppet {

class Logger {
public:
    static Logger& instance();

    // 初始化（设置日志文件和级别）
    bool init(const std::string& logFile, LogLevel level);

    // 日志方法
    void debug(const std::string& msg);
    void info(const std::string& msg);
    void warn(const std::string& msg);
    void error(const std::string& msg);

    // 通用日志
    void log(LogLevel level, const std::string& msg);

    // 设置级别
    void setLevel(LogLevel level);

    // 关闭
    void close();

private:
    Logger() = default;
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::string levelToString(LogLevel level) const;
    std::string currentTime() const;
    void write(LogLevel level, const std::string& msg);

    std::ofstream file_;
    std::mutex mutex_;
    LogLevel level_ = LogLevel::Info;
    bool initialized_ = false;
};

// 便捷宏
#define LOG_DEBUG(msg) puppet::Logger::instance().debug(msg)
#define LOG_INFO(msg)  puppet::Logger::instance().info(msg)
#define LOG_WARN(msg)  puppet::Logger::instance().warn(msg)
#define LOG_ERROR(msg) puppet::Logger::instance().error(msg)

} // namespace puppet
