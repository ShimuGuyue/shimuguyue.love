/**
 * @file config/cache.cpp
 * @brief 各项公开接口缓存有效期（cache.yml）配置加载实现
 */

#include "config/cache.h"

#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string_view>

#include <spdlog/spdlog.h>
#include <yaml-cpp/yaml.h>

namespace
{
    /// 配置文件名称。
    constexpr std::string_view CACHE_CONFIG_FILE{ "cache.yml" };

    /// 加载后的缓存有效期配置。
    config::CacheTtl g_ttl;

    /**
     * @brief 从当前目录向上查找 cache.yml。
     * @return 配置文件路径；未找到返回 std::nullopt。
     */
    auto find_cache_config() -> std::optional<std::filesystem::path>
    {
        std::filesystem::path dir{ std::filesystem::current_path() };
        for (;;)
        {
            const auto candidate = dir / CACHE_CONFIG_FILE;
            if (std::filesystem::is_regular_file(candidate))
                return candidate;

            const auto parent = dir.parent_path();
            if (parent == dir)
                return std::nullopt;
            dir = parent;
        }
    }
} // namespace






namespace config
{
    void init_cache()
    {
        const auto path = find_cache_config();
        if (!path)
        {
            spdlog::error("未找到 cache.yml 配置文件！请将 cache.yml 放在项目根目录。");
            std::exit(1);
        }

        YAML::Node root;
        try
        {
            root = YAML::LoadFile(path->string());
        }
        catch (const YAML::Exception& e)
        {
            spdlog::error("解析 cache.yml 失败：{}", e.what());
            std::exit(1);
        }

        const auto read_ttl = [](const YAML::Node& root, const char* field) -> long long
        {
            const YAML::Node node = root[field];
            if (!node || !node.IsScalar())
            {
                spdlog::error("cache.yml 缺少字段 {}！", field);
                std::exit(1);
            }
            try
            {
                const long long ttl = node.as<long long>();
                if (ttl <= 0)
                {
                    spdlog::error("cache.yml 字段 {} 必须是正整数！", field);
                    std::exit(1);
                }
                return ttl;
            }
            catch (const YAML::Exception& e)
            {
                spdlog::error("cache.yml 字段 {} 必须是正整数！", field);
                std::exit(1);
            }
        };

        g_ttl.categories = read_ttl(root, "categories");
        g_ttl.tags       = read_ttl(root, "tags");
        g_ttl.blogs      = read_ttl(root, "blogs");
        g_ttl.blog       = read_ttl(root, "blog");
        g_ttl.images     = read_ttl(root, "images");
        g_ttl.about      = read_ttl(root, "about");
        g_ttl.profile    = read_ttl(root, "profile");

        spdlog::info("cache.yml 缓存有效期配置加载完成。");
    }

    auto cache_ttl() -> const CacheTtl&
    {
        return g_ttl;
    }

} // namespace config
