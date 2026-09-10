/**
 * @file config/config.cpp
 * @brief 配置统一初始化入口实现
 */

#include "config/config.h"

#include "config/cache.h"
#include "config/env.h"
#include "config/page_size.h"

namespace config
{
    auto config() -> const ConfigMap&
    {
        return ConfigMap::instance();
    }

    auto find_conf_dir() -> std::optional<std::filesystem::path>
    {
        // 约定所有配置文件均在 conf/ 下，故目录只向上查找并缓存一次
        static const auto cached = [] () -> std::optional<std::filesystem::path>
        {
            std::filesystem::path dir{ std::filesystem::current_path() };
            for (;;)
            {
                std::error_code ec;
                const auto candidate = dir / "conf";
                if (std::filesystem::is_directory(candidate, ec) && !ec)
                    return candidate;

                const auto parent = dir.parent_path();
                if (parent == dir)
                    return std::nullopt;
                dir = parent;
            }
        }();
        return cached;
    }

    auto find_config_file(const std::string& filename) -> std::optional<std::filesystem::path>
    {
        const auto dir = find_conf_dir();
        if (!dir)
            return std::nullopt;

        const auto path = *dir / filename;
        std::error_code ec;
        if (!std::filesystem::is_regular_file(path, ec) || ec)
            return std::nullopt;

        return path;
    }

    void init()
    {
        init_env();
        init_cache();
        init_page_size();
    }

} // namespace config
