/**
 * @file config/config.cpp
 * @brief 配置统一初始化入口实现
 */

#include "config/config.h"

#include "config/cache.h"
#include "config/env.h"

namespace config
{
    auto find_config_file(const std::string& filename) -> std::optional<std::filesystem::path>
    {
        std::filesystem::path dir{ std::filesystem::current_path() };
        for (;;)
        {
            const auto candidate = dir / "conf" / filename;
            if (std::filesystem::is_regular_file(candidate))
                return candidate;

            const auto parent = dir.parent_path();
            if (parent == dir)
                return std::nullopt;
            dir = parent;
        }
    }

    void init()
    {
        init_env();
        init_cache();
    }

} // namespace config
