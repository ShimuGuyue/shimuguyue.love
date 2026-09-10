/**
 * @file config/cache.cpp
 * @brief 各项公开接口缓存有效期（conf/cache.yml）配置加载实现
 */

#include "config/cache.h"
#include "config/config.h"

#include <cstdlib>

#include <spdlog/spdlog.h>
#include <yaml-cpp/yaml.h>

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

        // 校验通过的有效期写入 ConfigMap，全部配置统一存储于此
        ConfigMap::config_values.set("CACHE_TTL_CATEGORIES", std::to_string(read_ttl(root, "categories")));
        ConfigMap::config_values.set("CACHE_TTL_TAGS",       std::to_string(read_ttl(root, "tags")));
        ConfigMap::config_values.set("CACHE_TTL_BLOGS",      std::to_string(read_ttl(root, "blogs")));
        ConfigMap::config_values.set("CACHE_TTL_BLOG",       std::to_string(read_ttl(root, "blog")));
        ConfigMap::config_values.set("CACHE_TTL_IMAGES",     std::to_string(read_ttl(root, "images")));

        spdlog::info("conf/cache.yml 缓存有效期配置加载完成。");
    }

} // namespace config
