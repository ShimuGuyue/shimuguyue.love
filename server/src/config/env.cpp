/**
 * @file config/env.cpp
 * @brief 环境变量获取、初始化与存储实现
 */

#include "config/env.h"
#include "config/env_map.h"

#include <cstdlib>
#include <string>

#include <spdlog/spdlog.h>

namespace config
{

    void init()
    {
        EnvMap::env_values.set("FRONTEND_ORIGIN", get_env("FRONTEND_ORIGIN"));
        EnvMap::env_values.set("SERVER_HOST",     get_env("SERVER_HOST"));
        EnvMap::env_values.set("SERVER_PORT",     get_env("SERVER_PORT"));
        EnvMap::env_values.set("DOC_PATH",        get_env("DOC_PATH"));
        EnvMap::env_values.set("IMAGE_PATH",      get_env("IMAGE_PATH"));
        EnvMap::env_values.set("PGHOST",          get_env("PGHOST"));
        EnvMap::env_values.set("PGPORT",          get_env("PGPORT"));
        EnvMap::env_values.set("PGDATABASE",      get_env("PGDATABASE"));
        EnvMap::env_values.set("PGUSER",          get_env("PGUSER"));
        EnvMap::env_values.set("PGPASSWORD",      get_env("PGPASSWORD"));

        if (std::stoi(EnvMap::env_values["SERVER_PORT"]) <= 0)
        {
            spdlog::error("环境变量 SERVER_PORT 必须是有效的端口号！");
            std::exit(1);
        }
    }

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

    auto env() -> const EnvMap&
    {
        return EnvMap::instance();
    }

}
