/**
 * @file config/page_size.cpp
 * @brief 各分页列表每页条数（conf/page_size.yml）配置加载实现
 */

#include "config/page_size.h"
#include "config/config.h"

#include <cstdlib>

#include <spdlog/spdlog.h>
#include <yaml-cpp/yaml.h>

namespace config
{
    void init_page_size()
    {
        const auto path = config::find_config_file("page_size.yml");
        if (!path)
        {
            spdlog::error("未找到 conf/page_size.yml 配置文件！请将 page_size.yml 放在项目 conf/ 目录中。");
            std::exit(1);
        }

        YAML::Node root;
        try
        {
            root = YAML::LoadFile(path->string());
        }
        catch (const YAML::Exception& e)
        {
            spdlog::error("解析 page_size.yml 失败：{}", e.what());
            std::exit(1);
        }

        const auto read_size = [](const YAML::Node& root, const char* field) -> int
        {
            const YAML::Node node = root[field];
            if (!node || !node.IsScalar())
            {
                spdlog::error("conf/page_size.yml 缺少字段 {}！", field);
                std::exit(1);
            }
            try
            {
                const int size = node.as<int>();
                if (size <= 0)
                {
                    spdlog::error("conf/page_size.yml 字段 {} 必须是正整数！", field);
                    std::exit(1);
                }
                return size;
            }
            catch (const YAML::Exception& e)
            {
                spdlog::error("conf/page_size.yml 字段 {} 必须是正整数！", field);
                std::exit(1);
            }
        };

        // 校验通过的每页条数写入 ConfigMap，全部配置统一存储于此
        ConfigMap::config_values.set("BLOGS_PAGESIZE", std::to_string(read_size(root, "blogs")));

        spdlog::info("conf/page_size.yml 分页配置加载完成。");
    }

} // namespace config
