/**
 * @file config/cache.cpp
 * @brief 各项公开接口缓存有效期（conf/cache.yml）配置加载实现
 */

#include "config/cache.h"
#include "config/config.h"

#include <cstdlib>

#include <spdlog/spdlog.h>
#include <yaml-cpp/yaml.h>

namespace
{
    /// 加载后的缓存有效期配置。
    config::CacheTtl g_ttl;
} // namespace






namespace config
{
    void init_cache()
    {
        const auto path = config::find_config_file("cache.yml");
        if (!path)
        {
            spdlog::error("未找到 conf/cache.yml 配置文件！请将 cache.yml 放在项目 conf/ 目录中。");
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
                spdlog::error("conf/cache.yml 缺少字段 {}！", field);
                std::exit(1);
            }
            try
            {
                const long long ttl = node.as<long long>();
                if (ttl <= 0)
                {
                    spdlog::error("conf/cache.yml 字段 {} 必须是正整数！", field);
                    std::exit(1);
                }
                return ttl;
            }
            catch (const YAML::Exception& e)
            {
                    spdlog::error("conf/cache.yml 字段 {} 必须是正整数！", field);
                std::exit(1);
            }
        };

        g_ttl.categories = read_ttl(root, "categories");
        g_ttl.tags       = read_ttl(root, "tags");
        g_ttl.blogs      = read_ttl(root, "blogs");
        g_ttl.blog       = read_ttl(root, "blog");
        g_ttl.images     = read_ttl(root, "images");
        g_ttl.friends    = read_ttl(root, "friends");
        g_ttl.about      = read_ttl(root, "about");
        g_ttl.profile    = read_ttl(root, "profile");

        spdlog::info("conf/cache.yml 缓存有效期配置加载完成。");
    }

    auto cache_ttl() -> const CacheTtl&
    {
        return g_ttl;
    }

} // namespace config
