#include "config.h"
#include "logger.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>

namespace puppet {

// 简单 YAML 解析器（支持 key: value 和嵌套缩进）
namespace {

std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end = s.find_last_not_of(" \t\r\n");
    return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
}

std::string removeComment(const std::string& s) {
    size_t pos = s.find('#');
    return (pos != std::string::npos) ? s.substr(0, pos) : s;
}

std::string extractValue(const std::string& line) {
    size_t colon = line.find(':');
    if (colon == std::string::npos) return "";
    std::string val = trim(line.substr(colon + 1));
    // 去除引号
    if (val.size() >= 2 && val.front() == '"' && val.back() == '"') {
        val = val.substr(1, val.size() - 2);
    }
    if (val.size() >= 2 && val.front() == '\'' && val.back() == '\'') {
        val = val.substr(1, val.size() - 2);
    }
    return val;
}

std::string extractKey(const std::string& line) {
    size_t colon = line.find(':');
    if (colon == std::string::npos) return "";
    return trim(line.substr(0, colon));
}

bool toBool(const std::string& s) {
    std::string lower = s;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return lower == "true" || lower == "1" || lower == "yes";
}

int toInt(const std::string& s, int def = 0) {
    try {
        return std::stoi(s);
    } catch (...) {
        return def;
    }
}

} // anonymous namespace

std::string ConfigLoader::configPath() {
    return "config/puppet.yml";
}

bool ConfigLoader::load(const std::string& path, Config& config) {
    std::ifstream file(path);
    if (!file.is_open()) {
        LOG_ERROR("无法打开配置文件: " + path);
        return false;
    }

    std::string line;
    std::string section;    // 当前 section（如 "ports", "api", "data_logging"）
    std::string subSection; // 当前子 section（如 "logging", "bds", "webhook"）

    while (std::getline(file, line)) {
        std::string cleaned = trim(removeComment(line));
        if (cleaned.empty()) continue;

        // 计算缩进级别
        size_t indent = 0;
        for (char c : line) {
            if (c == ' ') indent++;
            else if (c == '\t') indent += 4;
            else break;
        }

        std::string key = extractKey(cleaned);
        std::string val = extractValue(cleaned);

        if (key.empty()) continue;

        // 顶层 section（无缩进）
        if (indent == 0) {
            section = key;
            continue;
        }

        // 解析各 section 下的键值
        if (section == "ports") {
            if (key == "websocket") config.wsPort = toInt(val, 8000);
            else if (key == "http_api") config.httpPort = toInt(val, 8001);
        }
        else if (section == "api") {
            if (key == "listen") config.apiListen = val;
            else if (key == "api_key") config.apiKey = val;
        }
        else if (section == "data_logging") {
            if (key == "chat")        config.logging.chat = toBool(val);
            else if (key == "join")        config.logging.join = toBool(val);
            else if (key == "death")       config.logging.death = toBool(val);
            else if (key == "kill")        config.logging.kill = toBool(val);
            else if (key == "achievement") config.logging.achievement = toBool(val);
            else if (key == "item")        config.logging.item = toBool(val);
            else if (key == "command")     config.logging.command = toBool(val);
            else if (key == "block")       config.logging.block = toBool(val);
        }
        else if (section == "logging") {
            if (key == "level") config.logLevel = val;
            else if (key == "file") config.logFile = val;
        }
        else if (section == "bds") {
            if (key == "path") config.bdsPath = val;
            else if (key == "args") config.bdsArgs = val;
        }
        else if (section == "webhook") {
            if (key == "timeout") config.webhookTimeout = toInt(val, 5);
            else if (key == "retry") config.webhookRetry = toInt(val, 3);
        }
    }

    // 解析 listen 地址
    if (!config.apiListen.empty()) {
        size_t colon = config.apiListen.find(':');
        // port 已从 httpPort 获取，apiListen 可能包含地址和端口
        if (colon != std::string::npos) {
            config.httpPort = toInt(config.apiListen.substr(colon + 1), config.httpPort);
        }
    }

    LOG_INFO("配置文件加载成功: " + path);
    return true;
}

bool ConfigLoader::reload(const std::string& path, Config& config) {
    Config newConfig;
    if (!load(path, newConfig)) {
        LOG_ERROR("配置重载失败");
        return false;
    }
    config = newConfig;
    LOG_INFO("配置重载成功");
    return true;
}

} // namespace puppet
