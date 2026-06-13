#pragma once

#include <string>
#include <vector>
#include <ctime>
#include <cstdint>

namespace puppet {
namespace utils {

// 时间工具
std::string currentTime();          // "YYYY-MM-DD HH:MM:SS"
std::string currentTimeISO();       // ISO 8601 格式
int64_t     nowTimestamp();         // Unix 时间戳（秒）

// 字符串工具
std::string trim(const std::string& s);
std::vector<std::string> split(const std::string& s, char delimiter);
bool startsWith(const std::string& s, const std::string& prefix);
bool endsWith(const std::string& s, const std::string& suffix);

// URL 编解码
std::string urlDecode(const std::string& s);

// 数字工具
std::string intToString(int64_t n);

} // namespace utils
} // namespace puppet
