/**
 * @file config/env.cpp
 * @brief 系统环境变量获取函数的实现
 */

#include "config/env.h"

#include <spdlog/spdlog.h>

namespace config
{
    [[nodiscard]]auto get_env(const char* key) -> std::string
    {
        spdlog::debug("正在获取环境变量 {}...", key);
        const char* val{ std::getenv(key) };
        if (val == nullptr)
        {
            spdlog::error("缺少必需的环境变量 {}！", key);
            std::exit(1);
        }
        spdlog::debug("环境变量 {} 获取成功。", key);
        return val;
    }

}