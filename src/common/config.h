#pragma once

#include "types.h"
#include <string>

namespace puppet {

class ConfigLoader {
public:
    // 从 YAML 文件加载配置到 Config 结构体
    static bool load(const std::string& path, Config& config);

    // 重新加载配置
    static bool reload(const std::string& path, Config& config);

    // 获取配置文件路径
    static std::string configPath();
};

} // namespace puppet
