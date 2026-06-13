#include "logger.h"
#include <chrono>
#include <iomanip>
#include <ctime>
#include <filesystem>

namespace puppet {

Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

Logger::~Logger() {
    close();
}

bool Logger::init(const std::string& logFile, LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);

    level_ = level;

    // 确保日志目录存在
    auto parent = std::filesystem::path(logFile).parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }

    file_.open(logFile, std::ios::app);
    if (!file_.is_open()) {
        std::cerr << "[Logger] 无法打开日志文件: " << logFile << std::endl;
        return false;
    }

    initialized_ = true;
    return true;
}

void Logger::debug(const std::string& msg) { log(LogLevel::Debug, msg); }
void Logger::info(const std::string& msg)  { log(LogLevel::Info, msg); }
void Logger::warn(const std::string& msg)  { log(LogLevel::Warn, msg); }
void Logger::error(const std::string& msg) { log(LogLevel::Error, msg); }

void Logger::log(LogLevel level, const std::string& msg) {
    if (level < level_) return;
    write(level, msg);
}

void Logger::setLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    level_ = level;
}

void Logger::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_.is_open()) {
        file_.flush();
        file_.close();
    }
    initialized_ = false;
}

std::string Logger::levelToString(LogLevel level) const {
    switch (level) {
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO ";
        case LogLevel::Warn:  return "WARN ";
        case LogLevel::Error: return "ERROR";
        default:              return "?????";
    }
}

std::string Logger::currentTime() const {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::tm tm_buf{};
    localtime_r(&time, &tm_buf);

    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S")
        << "." << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

void Logger::write(LogLevel level, const std::string& msg) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::string timestamp = currentTime();
    std::string line = "[" + timestamp + "] [" + levelToString(level) + "] " + msg;

    // 同时输出到控制台
    if (level >= LogLevel::Warn) {
        std::cerr << line << std::endl;
    } else {
        std::cout << line << std::endl;
    }

    // 写入文件
    if (file_.is_open()) {
        file_ << line << "\n";
        file_.flush();
    }
}

} // namespace puppet
